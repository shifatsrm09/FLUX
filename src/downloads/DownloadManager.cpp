#include "DownloadManager.h"
#include "../search/FolderBrowser.h"
#include "../search/SearchResult.h"
#include "../core/Logger.h"

#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace Flux {

namespace {

constexpr int kMaxParallelJobs = 2;     // jobs downloading at the same time (files in a pack are sequential)
constexpr int kMaxPackFiles = 3000;     // safety cap for one pack
constexpr int kMaxScanDepth = 8;        // folder nesting followed inside a pack
constexpr int kMaxSegmentLen = 140;     // keeps full paths under Windows' MAX_PATH more often
constexpr int kMaxAutoRetries = 3;

const QString kDot = QStringLiteral(" \u00B7 ");

// Make one path segment (file or folder name) safe for Windows
QString sanitizeSegment(QString s) {
    s = s.trimmed();
    static const QRegularExpression bad(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]"));
    s.replace(bad, QStringLiteral("_"));
    while (s.endsWith('.') || s.endsWith(' ')) s.chop(1);

    if (s.size() > kMaxSegmentLen) {
        const int dot = s.lastIndexOf('.');
        if (dot > 0 && s.size() - dot <= 8) {
            s = s.left(kMaxSegmentLen - (s.size() - dot)) + s.mid(dot);
        } else {
            s = s.left(kMaxSegmentLen);
        }
    }

    static const QRegularExpression reserved(QStringLiteral("^(?:CON|PRN|AUX|NUL|COM\\d|LPT\\d)(?:\\..*)?$"),
                                             QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(s).hasMatch()) s.prepend('_');
    if (s.isEmpty()) s = QStringLiteral("download");
    return s;
}

QStringList decodedSegments(const QString &url) {
    const QUrl u(url, QUrl::TolerantMode);
    return QUrl::fromPercentEncoding(u.path(QUrl::FullyEncoded).toUtf8()).split('/', Qt::SkipEmptyParts);
}

// Folder name for a pack. A bare "Season 2" folder is prefixed with its parent so seasons of
// different shows never end up in the same place.
QString packFolderName(const QString &url, const QString &title) {
    const QStringList segs = decodedSegments(url);
    QString name = segs.isEmpty() ? title : segs.last();
    if (name.isEmpty()) name = title;

    static const QRegularExpression seasonRe(QStringLiteral("^(?:season|series|s)[\\s._\\-]*\\d{1,2}(?:\\b.*)?$"),
                                             QRegularExpression::CaseInsensitiveOption);
    if (segs.size() >= 2 && seasonRe.match(name).hasMatch()) {
        name = segs.at(segs.size() - 2) + QStringLiteral(" - ") + name;
    }
    return name;
}

QString uniquePath(const QString &path) {
    if (!QFileInfo::exists(path)) return path;
    const QFileInfo fi(path);
    const QString base = fi.completeBaseName();
    const QString suffix = fi.suffix();
    for (int i = 1; i < 1000; ++i) {
        const QString candidate = fi.absolutePath() + QLatin1Char('/') + base
                                  + QStringLiteral(" (%1)").arg(i)
                                  + (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
    return path;
}

QString formatSpeed(double bytesPerSec) {
    return SearchResult::formatBytes(static_cast<qint64>(bytesPerSec)) + QStringLiteral("/s");
}

QString native(const QString &path) {
    return QDir::toNativeSeparators(path);
}

bool isVideoSuffix(const QString &suffix) {
    static const QSet<QString> kVideo = {
        "MKV", "MP4", "AVI", "MOV", "WMV", "M4V", "TS", "M2TS", "WEBM", "FLV", "MPG", "MPEG"
    };
    return kVideo.contains(suffix.toUpper());
}

constexpr int kCountCap = 5000;

// Video files below a folder (recursive, capped so a huge tree can't stall the UI)
int countVideos(const QString &dirPath) {
    int n = 0;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && n < kCountCap) {
        it.next();
        if (isVideoSuffix(it.fileInfo().suffix())) ++n;
    }
    return n;
}

QString videoCountText(int n) {
    if (n >= kCountCap) return QStringLiteral("%1+ videos").arg(kCountCap);
    return n == 1 ? QStringLiteral("1 video") : QStringLiteral("%1 videos").arg(n);
}

// One video file as a Library entry (rel is relative to the download location root)
QVariantMap videoEntry(const QFileInfo &fi, const QString &root) {
    QVariantMap m;
    m["name"] = fi.fileName();
    m["kind"] = QStringLiteral("video");
    m["rel"] = QDir(root).relativeFilePath(fi.absoluteFilePath());
    m["path"] = native(fi.absoluteFilePath());
    m["url"] = QString::fromLatin1(QUrl::fromLocalFile(fi.absoluteFilePath()).toEncoded());
    m["sizeBytes"] = fi.size();
    m["detail"] = SearchResult::formatBytes(fi.size());
    return m;
}

// One folder (a pack) as a Library entry
QVariantMap folderEntry(const QFileInfo &fi, const QString &root, int videos) {
    QVariantMap m;
    m["name"] = fi.fileName();
    m["kind"] = QStringLiteral("folder");
    m["rel"] = QDir(root).relativeFilePath(fi.absoluteFilePath());
    m["path"] = native(fi.absoluteFilePath());
    m["count"] = videos;
    m["detail"] = videoCountText(videos);
    return m;
}

bool samePath(const QString &a, const QString &b) {
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

// True when `path` lies below `dir` (both cleaned, forward slashes)
bool isInside(const QString &path, const QString &dir) {
    const QString prefix = dir.endsWith('/') ? dir : dir + QLatin1Char('/');
    return path.size() > prefix.size() && path.startsWith(prefix, Qt::CaseInsensitive);
}

constexpr int kVisitCap = 30000;   // files looked at per folder count in an added target

// Video files below an added-target folder. Skips the download location, and gives up after
// kVisitCap files so pointing a target at a whole drive can't stall the UI.
int countVideosIn(const QString &dirPath, const QString &skipRoot, bool &truncated) {
    int n = 0;
    int visited = 0;
    truncated = false;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        if (++visited > kVisitCap) { truncated = true; break; }
        if (isInside(f, skipRoot)) continue;
        if (isVideoSuffix(QFileInfo(f).suffix()) && ++n >= kCountCap) break;
    }
    return n;
}

} // namespace

// ============================================================================
// Construction / model
// ============================================================================

DownloadManager::DownloadManager(FolderBrowser *browser, QObject *parent)
    : QAbstractListModel(parent)
    , m_browser(browser) {
    m_root = QDir::cleanPath(QDir::fromNativeSeparators(defaultLocation()));
    ensureFolders();
    FLUX_LOG_INFO("Downloads", QString("Download location: %1").arg(native(m_root)));
}

DownloadManager::~DownloadManager() {
    // Leave every .part file on disk: the next run can resume it
    m_shuttingDown = true;
    for (auto &job : m_jobs) {
        if (job->reply) {
            QNetworkReply *r = job->reply;
            job->reply = nullptr;
            r->disconnect(this);
            r->abort();
        }
        closePart(*job, false);
    }
}

int DownloadManager::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_jobs.size());
}

QVariant DownloadManager::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_jobs.size())) {
        return QVariant();
    }
    const Job &j = *m_jobs[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:          return j.id;
    case TitleRole:       return j.title;
    case KindRole:        return j.isPack ? QStringLiteral("pack") : QStringLiteral("file");
    case CategoryRole:    return j.category;
    case StatusRole:      return statusName(j.status);
    case ProgressRole:    return progressOf(j);
    case DetailRole:      return detailOf(j);
    case DestinationRole: return native(j.destDir);
    case ErrorRole:       return j.error;
    case ActiveRole:      return j.status == Status::Scanning || j.status == Status::Queued || j.status == Status::Running;
    default:              return QVariant();
    }
}

QHash<int, QByteArray> DownloadManager::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole]          = "jobId";
    roles[TitleRole]       = "jobTitle";
    roles[KindRole]        = "kind";
    roles[CategoryRole]    = "category";
    roles[StatusRole]      = "status";
    roles[ProgressRole]    = "progress";
    roles[DetailRole]      = "detail";
    roles[DestinationRole] = "destination";
    roles[ErrorRole]       = "errorText";
    roles[ActiveRole]      = "isActive";
    return roles;
}

QString DownloadManager::statusName(Status s) {
    switch (s) {
    case Status::Scanning:  return QStringLiteral("scanning");
    case Status::Queued:    return QStringLiteral("queued");
    case Status::Running:   return QStringLiteral("running");
    case Status::Done:      return QStringLiteral("done");
    case Status::Failed:    return QStringLiteral("failed");
    case Status::Cancelled: return QStringLiteral("cancelled");
    }
    return QString();
}

double DownloadManager::progressOf(const Job &j) {
    if (j.status == Status::Done) return 1.0;
    const int n = static_cast<int>(j.items.size());
    if (j.status == Status::Scanning || n == 0) return 0.0;

    if (j.sizesKnown && j.totalBytes > 0) {
        return std::clamp(static_cast<double>(j.doneBytes + j.curBytes) / static_cast<double>(j.totalBytes), 0.0, 1.0);
    }

    double cur = 0.0;
    if (j.status == Status::Running && j.nextIndex < n) {
        const qint64 total = j.curTotal > 0 ? j.curTotal : j.items[static_cast<size_t>(j.nextIndex)].size;
        if (total > 0) cur = std::clamp(static_cast<double>(j.curBytes) / static_cast<double>(total), 0.0, 1.0);
    }
    return std::clamp((static_cast<double>(j.doneCount) + cur) / static_cast<double>(n), 0.0, 1.0);
}

QString DownloadManager::detailOf(const Job &j) {
    const int n = static_cast<int>(j.items.size());
    const QString total = (j.sizesKnown && j.totalBytes > 0) ? SearchResult::formatBytes(j.totalBytes) : QString();

    switch (j.status) {
    case Status::Scanning:
        return n > 0 ? QStringLiteral("Scanning folder\u2026 %1 files found").arg(n)
                     : QStringLiteral("Scanning folder\u2026");

    case Status::Queued: {
        if (!j.isPack) return QStringLiteral("Waiting in queue");
        QStringList parts{QStringLiteral("%1 files").arg(n)};
        if (!total.isEmpty()) parts << total;
        parts << QStringLiteral("Waiting in queue");
        return parts.join(kDot);
    }

    case Status::Running: {
        QStringList parts;
        if (j.isPack) parts << QStringLiteral("File %1 of %2").arg(std::min(j.nextIndex + 1, n)).arg(n);

        if (!total.isEmpty()) {
            parts << QStringLiteral("%1 of %2").arg(SearchResult::formatBytes(j.doneBytes + j.curBytes), total);
        } else if (j.curBytes > 0) {
            parts << (j.curTotal > 0
                      ? QStringLiteral("%1 of %2").arg(SearchResult::formatBytes(j.curBytes), SearchResult::formatBytes(j.curTotal))
                      : SearchResult::formatBytes(j.curBytes));
        } else {
            parts << QStringLiteral("Starting\u2026");
        }

        if (!j.note.isEmpty()) parts << j.note;
        else if (j.speed > 1.0) parts << formatSpeed(j.speed);
        return parts.join(kDot);
    }

    case Status::Done: {
        QStringList parts;
        if (j.isPack) parts << QStringLiteral("%1 files").arg(n);
        parts << SearchResult::formatBytes(j.doneBytes);
        return parts.join(kDot);
    }

    case Status::Failed:
        return j.error.isEmpty() ? QStringLiteral("Download failed") : j.error;

    case Status::Cancelled:
        return QStringLiteral("Cancelled");
    }
    return QString();
}

DownloadManager::Job *DownloadManager::findJob(int id, int *row) const {
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i]->id == id) {
            if (row) *row = static_cast<int>(i);
            return m_jobs[i].get();
        }
    }
    return nullptr;
}

DownloadManager::Job *DownloadManager::findActiveBySource(const QString &url) const {
    for (const auto &j : m_jobs) {
        const bool active = j->status == Status::Scanning || j->status == Status::Queued || j->status == Status::Running;
        if (active && j->sourceUrl == url) return j.get();
    }
    return nullptr;
}

void DownloadManager::insertJob(std::unique_ptr<Job> job) {
    beginInsertRows(QModelIndex(), 0, 0);
    m_jobs.insert(m_jobs.begin(), std::move(job));
    endInsertRows();
    updateSummary();
}

void DownloadManager::touch(const Job &job) {
    int row = -1;
    if (!findJob(job.id, &row)) return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void DownloadManager::updateSummary() {
    int active = 0;
    bool finished = false;
    for (const auto &j : m_jobs) {
        switch (j->status) {
        case Status::Scanning:
        case Status::Queued:
        case Status::Running:   ++active; break;
        default:                finished = true; break;
        }
    }
    m_activeCount = active;
    m_hasFinished = finished;
    emit summaryChanged();
}

// ============================================================================
// Location
// ============================================================================

QString DownloadManager::defaultLocation() const {
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloads.isEmpty()) downloads = QDir::homePath() + QStringLiteral("/Downloads");
    return native(QDir::cleanPath(downloads + QStringLiteral("/FLUX")));
}

QString DownloadManager::downloadLocation() const { return native(m_root); }
QString DownloadManager::seriesDir() const { return native(m_root + QStringLiteral("/Series")); }
QString DownloadManager::individualsDir() const { return native(m_root + QStringLiteral("/Individuals")); }

void DownloadManager::setDownloadLocation(const QString &path) {
    changeLocation(path);
}

void DownloadManager::ensureFolders() const {
    QDir().mkpath(m_root + QStringLiteral("/Series"));
    QDir().mkpath(m_root + QStringLiteral("/Individuals"));
}

bool DownloadManager::changeLocation(const QString &path) {
    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (path.trimmed().isEmpty() || !QDir::isAbsolutePath(p)) {
        emit locationError(QStringLiteral("Please choose a full folder path, for example D:\\Movies"));
        return false;
    }

    if (!QDir().mkpath(p + QStringLiteral("/Series")) || !QDir().mkpath(p + QStringLiteral("/Individuals"))
        || !QFileInfo(p).isWritable()) {
        emit locationError(QStringLiteral("Couldn't create or write to %1").arg(native(p)));
        return false;
    }

    if (p == m_root) return true;

    m_root = p;
    FLUX_LOG_INFO("Downloads", QString("Download location changed to: %1").arg(native(m_root)));
    emit locationChanged();
    ++m_offlineRevision;
    emit offlineChanged();
    return true;
}

void DownloadManager::resetLocation() {
    changeLocation(defaultLocation());
}

void DownloadManager::openRoot() {
    ensureFolders();
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));
}

void DownloadManager::openFolder(int id) {
    const Job *job = findJob(id);
    if (!job) return;
    QDir().mkpath(job->destDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(job->destDir));
}

// ----------------------------------------------------------------------------
// Folder chooser helpers
// ----------------------------------------------------------------------------

QVariantList DownloadManager::listDirs(const QString &path) const {
    QVariantList out;
    const QString p = QDir::fromNativeSeparators(path.trimmed());

    if (p.isEmpty()) {
        const QFileInfoList drives = QDir::drives();
        for (const QFileInfo &d : drives) {
            QVariantMap m;
            m["name"] = native(d.absoluteFilePath());
            m["path"] = native(d.absoluteFilePath());
            out.append(m);
        }
        return out;
    }

    const QDir dir(p);
    if (!dir.exists()) return out;

    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::NoSort);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });

    for (const QFileInfo &fi : entries) {
        QVariantMap m;
        m["name"] = fi.fileName();
        m["path"] = native(fi.absoluteFilePath());
        out.append(m);
    }
    return out;
}

QString DownloadManager::parentDir(const QString &path) const {
    const QString p = QDir::fromNativeSeparators(path.trimmed());
    if (p.isEmpty()) return QString();
    QDir d(p);
    if (d.isRoot()) return QString();   // above a drive root = the drives list
    d.cdUp();
    return native(d.absolutePath());
}

QString DownloadManager::makeDir(const QString &parent, const QString &name) const {
    const QString clean = sanitizeSegment(name);
    const QString base = QDir::fromNativeSeparators(parent.trimmed());
    if (base.isEmpty() || name.trimmed().isEmpty()) return QString();
    const QString full = QDir::cleanPath(base + QLatin1Char('/') + clean);
    return QDir().mkpath(full) ? native(full) : QString();
}

// ----------------------------------------------------------------------------
// Offline library
// ----------------------------------------------------------------------------

QVariantList DownloadManager::offlineList(const QString &relPath) const {
    QVariantList out;

    // "@<absolute folder>" = browsing inside an added target
    if (relPath.startsWith('@')) return targetList(relPath.mid(1));

    QString rel = QDir::fromNativeSeparators(relPath.trimmed());
    while (rel.startsWith('/')) rel.remove(0, 1);
    while (rel.endsWith('/')) rel.chop(1);
    if (rel.split('/', Qt::SkipEmptyParts).contains(QStringLiteral(".."))) return out;

    // Top level: the actual series/pack folders first, then the individual movies.
    // (The "Series" and "Individuals" container folders themselves are not shown, and the
    // episodes inside the pack folders are not listed here.)
    if (rel.isEmpty()) {
        QCollator topCollator;
        topCollator.setNumericMode(true);
        topCollator.setCaseSensitivity(Qt::CaseInsensitive);
        auto topByName = [&topCollator](const QFileInfo &a, const QFileInfo &b) {
            return topCollator.compare(a.fileName(), b.fileName()) < 0;
        };

        QFileInfoList packs = QDir(m_root + QStringLiteral("/Series"))
                                  .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
        std::sort(packs.begin(), packs.end(), topByName);
        for (const QFileInfo &fi : packs) {
            const int n = countVideos(fi.absoluteFilePath());
            if (n == 0) continue;
            QVariantMap m = folderEntry(fi, m_root, n);
            m["section"] = QStringLiteral("TV Series & Packs");
            out.append(m);
        }

        QFileInfoList movies = QDir(m_root + QStringLiteral("/Individuals"))
                                   .entryInfoList(QDir::Files, QDir::NoSort);
        movies.erase(std::remove_if(movies.begin(), movies.end(),
                                    [](const QFileInfo &f) { return !isVideoSuffix(f.suffix()); }),
                     movies.end());
        std::sort(movies.begin(), movies.end(), topByName);
        for (const QFileInfo &fi : movies) {
            QVariantMap m = videoEntry(fi, m_root);
            m["section"] = QStringLiteral("Movies");
            out.append(m);
        }

        // Extra folders the user added in Settings (one entry each, opened like a pack)
        for (const QString &t : m_targets) {
            const QFileInfo fi(t);
            QVariantMap m;
            m["name"] = fi.fileName().isEmpty() ? native(t) : fi.fileName();
            m["kind"] = QStringLiteral("folder");
            m["rel"] = QStringLiteral("@") + t;
            m["path"] = native(t);
            m["detail"] = QDir(t).exists() ? native(t) : QStringLiteral("Not available \u00B7 ") + native(t);
            m["section"] = QStringLiteral("Added folders");
            out.append(m);
        }
        return out;
    }

    const QDir dir(QDir::cleanPath(m_root + QLatin1Char('/') + rel));
    if (!dir.exists()) return out;
    // Never list outside the download location
    if (!dir.canonicalPath().startsWith(QDir(m_root).canonicalPath(), Qt::CaseInsensitive)) return out;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto byName = [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    };

    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    std::sort(dirs.begin(), dirs.end(), byName);
    for (const QFileInfo &fi : dirs) {
        const int n = countVideos(fi.absoluteFilePath());
        if (n == 0) continue;   // empty / non-video folders aren't worth showing
        QVariantMap m;
        m["name"] = fi.fileName();
        m["kind"] = QStringLiteral("folder");
        m["rel"] = rel + QLatin1Char('/') + fi.fileName();
        m["path"] = native(fi.absoluteFilePath());
        m["count"] = n;
        m["detail"] = videoCountText(n);
        out.append(m);
    }

    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::NoSort);
    files.erase(std::remove_if(files.begin(), files.end(),
                               [](const QFileInfo &f) { return !isVideoSuffix(f.suffix()); }),
                files.end());
    std::sort(files.begin(), files.end(), byName);
    for (const QFileInfo &fi : files) {
        QVariantMap m;
        m["name"] = fi.fileName();
        m["kind"] = QStringLiteral("video");
        m["rel"] = rel + QLatin1Char('/') + fi.fileName();
        m["path"] = native(fi.absoluteFilePath());
        m["url"] = QString::fromLatin1(QUrl::fromLocalFile(fi.absoluteFilePath()).toEncoded());
        m["sizeBytes"] = fi.size();
        m["detail"] = SearchResult::formatBytes(fi.size());
        out.append(m);
    }
    return out;
}

// ----------------------------------------------------------------------------
// Library targets (extra folders scanned for playable media)
// ----------------------------------------------------------------------------

QStringList DownloadManager::targets() const {
    QStringList out;
    for (const QString &t : m_targets) out << native(t);
    return out;
}

void DownloadManager::setTargets(const QStringList &paths) {
    QStringList cleaned;
    for (const QString &raw : paths) {
        const QString p = QDir::cleanPath(QDir::fromNativeSeparators(raw.trimmed()));
        if (p.isEmpty() || !QDir::isAbsolutePath(p)) continue;
        bool dup = false;
        for (const QString &c : cleaned) dup = dup || samePath(c, p);
        if (!dup) cleaned << p;
    }
    if (cleaned == m_targets) return;
    m_targets = cleaned;
    emit targetsChanged();
    ++m_offlineRevision;
    emit offlineChanged();
}

bool DownloadManager::addTarget(const QString &path) {
    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (path.trimmed().isEmpty() || !QDir::isAbsolutePath(p) || !QFileInfo(p).isDir()) {
        emit targetError(QStringLiteral("Please choose an existing folder."));
        return false;
    }

    // The download location is already part of the Library on its own
    if (samePath(p, m_root) || isInside(p, m_root)) {
        emit targetError(QStringLiteral("That folder is inside your download location, which is already in the Library."));
        return false;
    }

    for (const QString &t : m_targets) {
        if (samePath(p, t) || isInside(p, t)) {
            emit targetError(QStringLiteral("That folder is already covered by an added target."));
            return false;
        }
    }

    // A new target that contains older ones replaces them
    m_targets.erase(std::remove_if(m_targets.begin(), m_targets.end(),
                                   [&p](const QString &t) { return isInside(t, p); }),
                    m_targets.end());
    m_targets.append(p);

    FLUX_LOG_INFO("Downloads", QString("Library target added: %1").arg(native(p)));
    emit targetsChanged();
    ++m_offlineRevision;
    emit offlineChanged();
    return true;
}

void DownloadManager::removeTarget(const QString &path) {
    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    const int before = m_targets.size();
    m_targets.erase(std::remove_if(m_targets.begin(), m_targets.end(),
                                   [&p](const QString &t) { return samePath(t, p); }),
                    m_targets.end());
    if (m_targets.size() == before) return;

    FLUX_LOG_INFO("Downloads", QString("Library target removed: %1").arg(native(p)));
    emit targetsChanged();
    ++m_offlineRevision;
    emit offlineChanged();
}

QString DownloadManager::offlineParent(const QString &rel) const {
    // Inside an added target
    if (rel.startsWith('@')) {
        const QString p = QDir::cleanPath(QDir::fromNativeSeparators(rel.mid(1)));
        for (const QString &t : m_targets) {
            if (samePath(p, t)) return QString();   // target root -> back to the Library top
        }
        QDir d(p);
        if (!d.cdUp()) return QString();
        return QStringLiteral("@") + QDir::cleanPath(d.absolutePath());
    }

    // Inside the download location: the Series / Individuals containers are never shown
    const int i = rel.lastIndexOf('/');
    if (i < 0) return QString();
    const QString parent = rel.left(i);
    return (parent == QLatin1String("Series") || parent == QLatin1String("Individuals")) ? QString() : parent;
}

// Lists one folder inside an added target: sub folders that hold videos, then the videos
QVariantList DownloadManager::targetList(const QString &absPath) const {
    QVariantList out;

    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(absPath.trimmed()));
    if (p.isEmpty() || p.split('/', Qt::SkipEmptyParts).contains(QStringLiteral(".."))) return out;

    // Only ever list inside an added target, and never inside the download location
    bool allowed = false;
    for (const QString &t : m_targets) {
        if (samePath(p, t) || isInside(p, t)) { allowed = true; break; }
    }
    if (!allowed) return out;
    if (samePath(p, m_root) || isInside(p, m_root)) return out;

    const QDir dir(p);
    if (!dir.exists()) return out;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto byName = [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    };

    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::NoSort);
    std::sort(dirs.begin(), dirs.end(), byName);
    for (const QFileInfo &fi : dirs) {
        const QString fp = QDir::cleanPath(fi.absoluteFilePath());
        if (samePath(fp, m_root) || isInside(fp, m_root)) continue;   // never list the download location

        bool truncated = false;
        const int n = countVideosIn(fp, m_root, truncated);
        if (n == 0 && !truncated) continue;   // no videos in there

        QVariantMap m;
        m["name"] = fi.fileName();
        m["kind"] = QStringLiteral("folder");
        m["rel"] = QStringLiteral("@") + fp;
        m["path"] = native(fp);
        m["count"] = n;
        m["detail"] = n > 0 ? videoCountText(n) : QStringLiteral("Folder");
        out.append(m);
    }

    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::NoSort);
    files.erase(std::remove_if(files.begin(), files.end(),
                               [](const QFileInfo &f) { return !isVideoSuffix(f.suffix()); }),
                files.end());
    std::sort(files.begin(), files.end(), byName);
    for (const QFileInfo &fi : files) {
        QVariantMap m = videoEntry(fi, m_root);
        m["rel"] = QStringLiteral("@") + QDir::cleanPath(fi.absoluteFilePath());
        out.append(m);
    }
    return out;
}

QVariantList DownloadManager::offlineSearch(const QString &query) const {
    QVariantList out;

    static const QRegularExpression sep(QStringLiteral("[\\s._\\-\\[\\]()/\\\\]+"));
    const QStringList terms = query.toLower().split(sep, Qt::SkipEmptyParts);
    if (terms.isEmpty()) return out;

    // True when every search word appears in the (normalised) text
    auto matches = [&terms](const QString &text) {
        QString hay = text.toLower();
        hay.replace(sep, QStringLiteral(" "));
        for (const QString &t : terms) {
            if (!hay.contains(t)) return false;
        }
        return true;
    };

    constexpr int kMaxSearchResults = 300;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    // Pack folders whose name matches
    QFileInfoList packs = QDir(m_root + QStringLiteral("/Series"))
                              .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    std::sort(packs.begin(), packs.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });
    for (const QFileInfo &fi : packs) {
        if (!matches(fi.fileName())) continue;
        const int n = countVideos(fi.absoluteFilePath());
        if (n == 0) continue;
        QVariantMap m = folderEntry(fi, m_root, n);
        m["section"] = QStringLiteral("Results");
        out.append(m);
    }

    // Videos anywhere in the library (including episodes inside packs)
    QFileInfoList hits;
    QDirIterator it(m_root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && hits.size() < kMaxSearchResults) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (!isVideoSuffix(fi.suffix())) continue;
        if (matches(QDir(m_root).relativeFilePath(fi.absoluteFilePath()))) hits.append(fi);
    }
    std::sort(hits.begin(), hits.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });

    for (const QFileInfo &fi : hits) {
        QVariantMap m = videoEntry(fi, m_root);
        m["section"] = QStringLiteral("Results");
        // Folder the file lives in, without the Series / Individuals container
        QString folder = QDir(m_root).relativeFilePath(fi.absolutePath());
        const int slash = folder.indexOf('/');
        folder = slash < 0 ? QString() : folder.mid(slash + 1);
        m["folder"] = folder;
        out.append(m);
    }

    // Videos inside the folders the user added as targets (never the download location)
    for (const QString &t : m_targets) {
        const QDir tdir(t);
        if (!tdir.exists()) continue;
        const QString tname = QFileInfo(t).fileName().isEmpty() ? native(t) : QFileInfo(t).fileName();

        QFileInfoList thits;
        int visited = 0;
        QDirIterator tit(t, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (tit.hasNext() && thits.size() < kMaxSearchResults && visited < 60000) {
            const QString f = tit.next();
            ++visited;
            if (isInside(f, m_root)) continue;
            const QFileInfo fi(f);
            if (!isVideoSuffix(fi.suffix())) continue;
            if (matches(tname + QLatin1Char('/') + tdir.relativeFilePath(f))) thits.append(fi);
        }
        std::sort(thits.begin(), thits.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
            return collator.compare(a.fileName(), b.fileName()) < 0;
        });

        for (const QFileInfo &fi : thits) {
            QVariantMap m = videoEntry(fi, m_root);
            m["rel"] = QStringLiteral("@") + QDir::cleanPath(fi.absoluteFilePath());
            m["section"] = QStringLiteral("Results");
            const QString sub = tdir.relativeFilePath(fi.absolutePath());
            m["folder"] = (sub == QLatin1String(".")) ? tname : tname + QLatin1Char('/') + sub;
            out.append(m);
        }
    }
    return out;
}

void DownloadManager::openPath(const QString &path) {
    const QFileInfo fi(QDir::fromNativeSeparators(path));
    if (!fi.exists()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
}

// ============================================================================
// Creating jobs
// ============================================================================

int DownloadManager::downloadFile(const QString &url, const QString &title) {
    if (url.isEmpty()) return 0;
    if (const Job *dup = findActiveBySource(url)) return dup->id;

    QString name = QUrl::fromPercentEncoding(url.toUtf8());
    name = name.mid(name.lastIndexOf('/') + 1);
    if (name.isEmpty()) name = title;

    auto job = std::make_unique<Job>();
    job->id = ++m_nextId;
    job->title = name;
    job->sourceUrl = url;
    job->category = QStringLiteral("Individuals");
    job->destDir = m_root + QStringLiteral("/Individuals");
    job->isPack = false;

    Item item;
    item.url = url;
    item.relPath = sanitizeSegment(name);
    job->items.push_back(item);
    job->status = Status::Queued;

    const int id = job->id;
    FLUX_LOG_INFO("Downloads", QString("Queued file: %1").arg(name));
    insertJob(std::move(job));
    pump();
    return id;
}

int DownloadManager::downloadPack(const QString &folderUrl, const QString &title) {
    if (folderUrl.isEmpty()) return 0;

    QString url = folderUrl;
    if (!url.endsWith('/')) url += QLatin1Char('/');
    if (const Job *dup = findActiveBySource(url)) return dup->id;

    const QString name = packFolderName(url, title);

    auto job = std::make_unique<Job>();
    job->id = ++m_nextId;
    job->title = name;
    job->sourceUrl = url;
    job->category = QStringLiteral("Series");
    job->destDir = m_root + QStringLiteral("/Series/") + sanitizeSegment(name);
    job->isPack = true;
    job->status = Status::Scanning;
    job->pendingLists = 1;
    job->scanGen = 1;

    const int id = job->id;
    FLUX_LOG_INFO("Downloads", QString("Queued pack: %1").arg(name));
    insertJob(std::move(job));
    scanFolder(id, url, QString(), 0, 1);
    return id;
}

// ============================================================================
// Scanning a folder for its files
// ============================================================================

void DownloadManager::scanFolder(int id, const QString &folderUrl, const QString &rel, int depth, int gen) {
    if (!m_browser) {
        if (Job *job = findJob(id)) failJob(*job, QStringLiteral("Folder browsing is unavailable"));
        return;
    }

    QPointer<DownloadManager> self(this);
    m_browser->listFolder(folderUrl, [self, id, rel, depth, gen](bool ok, std::vector<SearchResult> items) {
        if (!self) return;
        self->onScanResult(id, rel, depth, gen, ok, std::move(items));
    });
}

void DownloadManager::onScanResult(int id, const QString &rel, int depth, int gen, bool ok,
                                   std::vector<SearchResult> items) {
    Job *job = findJob(id);
    if (!job || job->status != Status::Scanning || job->scanGen != gen) return;   // cancelled / superseded

    if (!ok) {
        failJob(*job, QStringLiteral("Couldn't read this folder. Check your connection."));
        return;
    }

    for (const SearchResult &r : items) {
        if (job->status != Status::Scanning) return;   // a nested listing failed or was cancelled

        if (r.isFolder) {
            if (depth < kMaxScanDepth) {
                ++job->pendingLists;
                scanFolder(id, r.playUrl, rel + sanitizeSegment(r.displayName) + QLatin1Char('/'), depth + 1, gen);
            }
        } else if (static_cast<int>(job->items.size()) < kMaxPackFiles) {
            Item item;
            item.url = r.playUrl;
            item.relPath = rel + sanitizeSegment(r.displayName);
            item.size = r.sizeBytes > 0 ? r.sizeBytes : -1;
            job->items.push_back(item);
        }
    }

    // Decrement only after the loop: a nested listing answered from cache runs synchronously
    // and must not finish the scan before this folder's own files have been added.
    if (job->status == Status::Scanning && --job->pendingLists == 0) {
        finalizeScan(*job);
    } else {
        touch(*job);
    }
}

void DownloadManager::finalizeScan(Job &job) {
    if (job.items.empty()) {
        failJob(job, QStringLiteral("No video files found in this folder."));
        return;
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(job.items.begin(), job.items.end(), [&collator](const Item &a, const Item &b) {
        return collator.compare(a.relPath, b.relPath) < 0;
    });

    job.sizesKnown = true;
    job.totalBytes = 0;
    for (const Item &it : job.items) {
        if (it.size <= 0) job.sizesKnown = false;
        else job.totalBytes += it.size;
    }

    job.scanned = true;
    job.status = Status::Queued;
    FLUX_LOG_INFO("Downloads", QString("Pack '%1': %2 files, %3")
                  .arg(job.title).arg(job.items.size())
                  .arg(job.sizesKnown ? SearchResult::formatBytes(job.totalBytes) : QStringLiteral("size unknown")));
    touch(job);
    updateSummary();
    pump();
}

// ============================================================================
// Running jobs
// ============================================================================

void DownloadManager::pump() {
    if (m_pumping) {
        m_pumpAgain = true;
        return;
    }
    m_pumping = true;

    do {
        m_pumpAgain = false;

        int running = 0;
        for (const auto &j : m_jobs) {
            if (j->status == Status::Running) ++running;
        }

        for (const auto &j : m_jobs) {
            if (running >= kMaxParallelJobs) break;
            if (j->status != Status::Queued) continue;

            j->status = Status::Running;
            ++running;
            touch(*j);
            startNext(*j);
            if (j->status != Status::Running) --running;   // failed / finished instantly
        }
    } while (m_pumpAgain);

    m_pumping = false;
    updateSummary();
}

void DownloadManager::closePart(Job &job, bool removeFile) {
    if (job.file) {
        job.file->close();
        job.file.reset();
    }
    if (removeFile && !job.partPath.isEmpty()) QFile::remove(job.partPath);
}

void DownloadManager::failJob(Job &job, const QString &message) {
    closePart(job, false);   // keep the partial file so Retry can resume
    job.status = Status::Failed;
    job.error = message;
    job.note.clear();
    job.speed = 0.0;
    FLUX_LOG_WARN("Downloads", QString("'%1' failed: %2").arg(job.title, message));
    touch(job);
    updateSummary();
    pump();
}

void DownloadManager::finishJob(Job &job) {
    job.status = Status::Done;
    job.note.clear();
    job.speed = 0.0;
    job.curBytes = 0;
    FLUX_LOG_INFO("Downloads", QString("Completed '%1' -> %2").arg(job.title, native(job.destDir)));
    touch(job);
    updateSummary();
    pump();
}

void DownloadManager::startNext(Job &job) {
    while (true) {
        const int n = static_cast<int>(job.items.size());
        if (job.nextIndex >= n) {
            finishJob(job);
            return;
        }

        Item &item = job.items[static_cast<size_t>(job.nextIndex)];
        if (item.done) {
            ++job.nextIndex;
            continue;
        }

        QString finalPath = QDir::cleanPath(job.destDir + QLatin1Char('/') + item.relPath);
        if (finalPath.size() > 250) {
            failJob(job, QStringLiteral("Path is too long. Pick a shorter download location."));
            return;
        }

        const QFileInfo outInfo(finalPath);
        if (!QDir().mkpath(outInfo.absolutePath())) {
            failJob(job, QStringLiteral("Couldn't create folder %1").arg(native(outInfo.absolutePath())));
            return;
        }

        if (outInfo.exists()) {
            if (item.size > 0 && outInfo.size() == item.size) {
                // Already downloaded (a re-run of a pack skips what it has)
                item.done = true;
                job.doneBytes += item.size;
                ++job.doneCount;
                ++job.nextIndex;
                continue;
            }
            if (!job.isPack) finalPath = uniquePath(finalPath);   // never overwrite a different file
        }

        job.finalPath = finalPath;
        job.partPath = finalPath + QStringLiteral(".part");
        job.note.clear();
        job.writeFailed = false;
        job.headersChecked = false;
        job.httpStatus = 0;

        job.resumeFrom = QFileInfo::exists(job.partPath) ? QFileInfo(job.partPath).size() : 0;
        if (item.size > 0 && job.resumeFrom > item.size) {
            QFile::remove(job.partPath);   // corrupt leftovers: start over
            job.resumeFrom = 0;
        }

        job.file = std::make_unique<QFile>(job.partPath);
        const QIODevice::OpenMode mode = QIODevice::WriteOnly
                                         | (job.resumeFrom > 0 ? QIODevice::Append : QIODevice::Truncate);
        if (!job.file->open(mode)) {
            failJob(job, QStringLiteral("Can't write to %1").arg(native(job.partPath)));
            return;
        }

        job.curBytes = job.resumeFrom;
        job.curTotal = item.size > 0 ? item.size : 0;

        // The partial file is already complete: just finish it
        if (item.size > 0 && job.resumeFrom == item.size) {
            if (!finalizeItem(job)) return;
            continue;
        }

        QNetworkRequest request{QUrl(item.url, QUrl::TolerantMode)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(30000);
        if (job.resumeFrom > 0) {
            request.setRawHeader("Range", "bytes=" + QByteArray::number(job.resumeFrom) + "-");
        }

        job.clock.start();
        job.emitClock.start();
        job.speedMarkMs = 0;
        job.speedMarkBytes = job.curBytes;

        QNetworkReply *reply = m_network.get(request);
        job.reply = reply;

        const int id = job.id;
        connect(reply, &QNetworkReply::readyRead, this, [this, id, reply]() { onReadyRead(id, reply); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [this, id, reply](qint64 received, qint64 total) { onProgress(id, reply, received, total); });
        connect(reply, &QNetworkReply::finished, this, [this, id, reply]() { onFinished(id, reply); });

        FLUX_LOG_INFO("Downloads", QString("Downloading %1 (%2)").arg(item.relPath,
                      job.resumeFrom > 0 ? QStringLiteral("resuming at %1").arg(SearchResult::formatBytes(job.resumeFrom))
                                         : QStringLiteral("from start")));
        touch(job);
        return;
    }
}

void DownloadManager::onReadyRead(int id, QNetworkReply *reply) {
    Job *job = findJob(id);
    if (!job || job->reply != reply || m_shuttingDown) return;

    if (!job->headersChecked) {
        job->headersChecked = true;
        job->httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (job->resumeFrom > 0 && job->httpStatus == 200 && job->file) {
            // The server ignored our Range request and is sending the whole file again
            job->file->resize(0);
            job->file->seek(0);
            job->resumeFrom = 0;
            job->curBytes = 0;
        }
    }

    const QByteArray chunk = reply->readAll();
    if (job->httpStatus >= 400 || !job->file || chunk.isEmpty()) return;   // error page body: don't save it

    if (job->file->write(chunk) != chunk.size()) {
        job->writeFailed = true;
        reply->abort();
    }
}

void DownloadManager::onProgress(int id, QNetworkReply *reply, qint64 received, qint64 total) {
    Job *job = findJob(id);
    if (!job || job->reply != reply || m_shuttingDown) return;

    job->curBytes = job->resumeFrom + received;
    if (total > 0) job->curTotal = job->resumeFrom + total;

    const qint64 now = job->clock.elapsed();
    if (now - job->speedMarkMs >= 700) {
        const double instant = static_cast<double>(job->curBytes - job->speedMarkBytes) * 1000.0
                               / static_cast<double>(std::max<qint64>(1, now - job->speedMarkMs));
        job->speed = job->speed > 0.0 ? job->speed * 0.6 + instant * 0.4 : instant;
        job->speedMarkMs = now;
        job->speedMarkBytes = job->curBytes;
    }

    if (job->emitClock.elapsed() >= 200) {
        job->emitClock.restart();
        touch(*job);
    }
}

void DownloadManager::onFinished(int id, QNetworkReply *reply) {
    reply->deleteLater();

    Job *job = findJob(id);
    if (!job || job->reply != reply || m_shuttingDown) return;   // cancelled, removed or shutting down
    job->reply = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!job->headersChecked) job->httpStatus = status;

    // Whatever arrived after the last readyRead
    if (job->file && job->httpStatus < 400 && reply->bytesAvailable() > 0) {
        const QByteArray rest = reply->readAll();
        if (job->file->write(rest) != rest.size()) job->writeFailed = true;
    }

    // 416 on a resume means the partial file already is the whole file
    const bool rangeAlreadyComplete = (status == 416 && job->resumeFrom > 0);

    if (job->writeFailed) {
        failJob(*job, QStringLiteral("Couldn't write to disk. Is the drive full?"));
        return;
    }

    if (reply->error() != QNetworkReply::NoError && !rangeAlreadyComplete) {
        const QString message = reply->errorString();
        closePart(*job, false);

        // Hiccups on a long transfer are common: retry a few times, resuming from the .part file
        const bool hardError = (status == 404 || status == 403 || status == 401 || status == 410);
        if (!hardError && job->attempts < kMaxAutoRetries && job->status == Status::Running) {
            ++job->attempts;
            job->note = QStringLiteral("Connection lost, retrying\u2026");
            job->speed = 0.0;
            touch(*job);
            QTimer::singleShot(2500, this, [this, id]() {
                Job *j = findJob(id);
                if (j && j->status == Status::Running && !j->reply && !m_shuttingDown) startNext(*j);
            });
            return;
        }

        failJob(*job, message);
        return;
    }

    if (!finalizeItem(*job)) return;
    startNext(*job);
}

bool DownloadManager::finalizeItem(Job &job) {
    Item &item = job.items[static_cast<size_t>(job.nextIndex)];

    if (job.file) {
        job.file->flush();
        job.file->close();
        job.file.reset();
    }

    const qint64 partSize = QFileInfo(job.partPath).size();
    const qint64 expected = item.size > 0 ? item.size : (job.curTotal > 0 ? job.curTotal : -1);
    if (expected > 0 && partSize != expected) {
        if (partSize > expected) QFile::remove(job.partPath);   // can't be resumed
        failJob(job, QStringLiteral("Incomplete download (%1 of %2)")
                .arg(SearchResult::formatBytes(partSize), SearchResult::formatBytes(expected)));
        return false;
    }

    if (QFileInfo::exists(job.finalPath)) QFile::remove(job.finalPath);
    if (!QFile::rename(job.partPath, job.finalPath)) {
        failJob(job, QStringLiteral("Couldn't save %1").arg(native(job.finalPath)));
        return false;
    }

    item.done = true;
    job.doneBytes += partSize;
    ++job.doneCount;
    ++job.nextIndex;
    job.curBytes = 0;
    job.curTotal = 0;
    job.attempts = 0;
    touch(job);
    ++m_offlineRevision;
    emit offlineChanged();
    return true;
}

// ============================================================================
// User actions
// ============================================================================

void DownloadManager::cancel(int id) {
    Job *job = findJob(id);
    if (!job) return;
    if (job->status == Status::Done || job->status == Status::Failed || job->status == Status::Cancelled) return;

    job->status = Status::Cancelled;   // set first: scan callbacks and retry timers check it
    job->error.clear();
    job->note.clear();
    job->speed = 0.0;
    ++job->scanGen;

    if (job->reply) {
        QNetworkReply *r = job->reply;
        job->reply = nullptr;          // onFinished will then ignore this reply
        r->abort();
    }
    closePart(*job, true);             // an abandoned download shouldn't leave a .part behind

    FLUX_LOG_INFO("Downloads", QString("Cancelled '%1'").arg(job->title));
    touch(*job);
    updateSummary();
    pump();
}

void DownloadManager::retry(int id) {
    Job *job = findJob(id);
    if (!job) return;
    if (job->status != Status::Failed && job->status != Status::Cancelled) return;

    job->error.clear();
    job->note.clear();
    job->attempts = 0;

    if (job->isPack && !job->scanned) {
        job->items.clear();
        job->status = Status::Scanning;
        job->pendingLists = 1;
        ++job->scanGen;
        touch(*job);
        updateSummary();
        scanFolder(id, job->sourceUrl, QString(), 0, job->scanGen);
        return;
    }

    job->status = Status::Queued;
    touch(*job);
    updateSummary();
    pump();
}

void DownloadManager::remove(int id) {
    int row = -1;
    Job *job = findJob(id, &row);
    if (!job) return;

    cancel(id);   // no-op when already finished

    row = -1;
    if (!findJob(id, &row)) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_jobs.erase(m_jobs.begin() + row);
    endRemoveRows();
    updateSummary();
}

void DownloadManager::clearFinished() {
    for (int row = static_cast<int>(m_jobs.size()) - 1; row >= 0; --row) {
        const Status s = m_jobs[static_cast<size_t>(row)]->status;
        if (s == Status::Done || s == Status::Failed || s == Status::Cancelled) {
            beginRemoveRows(QModelIndex(), row, row);
            m_jobs.erase(m_jobs.begin() + row);
            endRemoveRows();
        }
    }
    updateSummary();
}

} // namespace Flux
