#include "TorrentManager.h"
#include "../core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QClipboard>
#include <QGuiApplication>
#include <QRegularExpression>


#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace Flux {

TorrentManager::TorrentManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
    , m_engineCheckTimer(new QTimer(this))
{
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &TorrentManager::onPollTimer);

    m_engineCheckTimer->setInterval(1500);
    connect(m_engineCheckTimer, &QTimer::timeout, this, &TorrentManager::pingEngine);

    checkEngine();
}

TorrentManager::~TorrentManager() {
    if (m_pollTimer) m_pollTimer->stop();
    if (m_engineCheckTimer) m_engineCheckTimer->stop();
    if (m_searchReply) {
        m_searchReply->abort();
        m_searchReply->deleteLater();
    }
    if (m_detailReply) {
        m_detailReply->abort();
        m_detailReply->deleteLater();
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        FLUX_LOG_INFO("TorrentManager", "Terminating background torrent engine");
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
        }
    }
}

QString TorrentManager::formattedSize() const {
    return formatBytes(m_totalSize);
}

QString TorrentManager::formattedDownloadSpeed() const {
    if (m_downloadSpeed <= 0) return QStringLiteral("0 KB/s");
    return formatBytes(m_downloadSpeed) + QStringLiteral("/s");
}

QString TorrentManager::formattedUploadSpeed() const {
    if (m_uploadSpeed <= 0) return QStringLiteral("0 KB/s");
    return formatBytes(m_uploadSpeed) + QStringLiteral("/s");
}

QString TorrentManager::formatBytes(qint64 bytes) {
    if (bytes <= 0) return QStringLiteral("0 B");
    const double b = static_cast<double>(bytes);
    if (b >= 1024.0 * 1024.0 * 1024.0) {
        return QString::asprintf("%.2f GB", b / (1024.0 * 1024.0 * 1024.0));
    }
    if (b >= 1024.0 * 1024.0) {
        return QString::asprintf("%.1f MB", b / (1024.0 * 1024.0));
    }
    if (b >= 1024.0) {
        return QString::asprintf("%.0f KB", b / 1024.0);
    }
    return QString::asprintf("%lld B", bytes);
}

bool TorrentManager::isVideoFile(const QString &path) {
    const QString lower = path.toLower();
    const QStringList exts = {
        QStringLiteral(".mkv"), QStringLiteral(".mp4"), QStringLiteral(".avi"),
        QStringLiteral(".mov"), QStringLiteral(".wmv"), QStringLiteral(".webm"),
        QStringLiteral(".flv"), QStringLiteral(".m4v"), QStringLiteral(".ts"),
        QStringLiteral(".iso")
    };
    for (const QString &ext : exts) {
        if (lower.endsWith(ext)) return true;
    }
    return false;
}

QString TorrentManager::browseTorrentFile() {
#ifdef Q_OS_WIN
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Torrent Files (*.torrent)\0*.torrent\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        return QString::fromWCharArray(szFile);
    }
#endif
    return QString();
}

QString TorrentManager::findEngineExecutable() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/FLUX/bin");
    const QString sourceThirdParty = QDir::currentPath() + QStringLiteral("/third_party/torrserver");

    const QStringList candidates = {
        appDir + QStringLiteral("/torrserver.exe"),
        appDir + QStringLiteral("/tools/torrserver.exe"),
        localAppData + QStringLiteral("/torrserver.exe"),
        sourceThirdParty + QStringLiteral("/torrserver.exe")
    };

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

void TorrentManager::checkEngine() {
    pingEngine();
}

void TorrentManager::pingEngine() {
    QUrl url(QStringLiteral("http://127.0.0.1:%1/echo").arg(m_port));
    QNetworkRequest req(url);
    req.setTransferTimeout(1200);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            if (!m_engineReady) {
                m_engineReady = true;
                m_isSettingUp = false;
                m_engineCheckTimer->stop();
                emit engineReadyChanged();
                emit setupChanged();
                FLUX_LOG_INFO("TorrentManager", "Torrent engine connected and active on port " + QString::number(m_port));
                configureEngineSettings();
            }
        } else {
            if (!m_engineReady && !m_isSettingUp) {
                startEngineProcess();
            }
        }
    });
}

void TorrentManager::startEngineProcess() {
    const QString exePath = findEngineExecutable();
    if (exePath.isEmpty()) {
        FLUX_LOG_INFO("TorrentManager", "TorrServer executable not found locally, starting download");
        downloadEngine();
        return;
    }

    if (m_process->state() == QProcess::Running) {
        return;
    }

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/FLUX/torrent_cache");
    QDir().mkpath(cacheDir);

    QStringList args;
    args << QStringLiteral("-p") << QString::number(m_port)
         << QStringLiteral("-i") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-d") << QDir::toNativeSeparators(cacheDir);

    FLUX_LOG_INFO("TorrentManager", "Launching TorrServer: " + exePath + " " + args.join(" "));
    m_process->start(exePath, args);

    if (!m_engineCheckTimer->isActive()) {
        m_engineCheckTimer->start();
    }
}

void TorrentManager::downloadEngine() {
    if (m_isSettingUp) return;

    m_isSettingUp = true;
    m_setupProgress = 0.0;
    m_setupStatus = QStringLiteral("Initializing torrent engine...");
    emit setupChanged();

    const QUrl downloadUrl(QStringLiteral("https://github.com/YouROK/TorrServer/releases/download/MatriX.147.1/TorrServer-windows-amd64.exe"));
    QNetworkRequest req(downloadUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_setupProgress = static_cast<double>(received) / static_cast<double>(total);
            m_setupStatus = QString::asprintf("Downloading torrent engine (%.0f%%)...", m_setupProgress * 100.0);
            emit setupChanged();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const QString targetDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/FLUX/bin");
            QDir().mkpath(targetDir);
            const QString targetFile = targetDir + QStringLiteral("/torrserver.exe");

            QFile file(targetFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();
                FLUX_LOG_INFO("TorrentManager", "Downloaded TorrServer successfully to " + targetFile);
                m_setupStatus = QStringLiteral("Torrent engine ready!");
                m_setupProgress = 1.0;
                emit setupChanged();
                startEngineProcess();
                return;
            }
        }
        m_isSettingUp = false;
        m_errorMessage = QStringLiteral("Failed to download torrent engine: ") + reply->errorString();
        emit errorChanged();
        emit setupChanged();
    });
}

void TorrentManager::configureEngineSettings() {
    QUrl url(QStringLiteral("http://127.0.0.1:%1/settings").arg(m_port));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject sets;
    sets[QStringLiteral("CacheSize")] = 134217728; // 128 MB RAM buffer
    sets[QStringLiteral("ReaderReadAHead")] = 95;   // 95% sequential lookahead
    sets[QStringLiteral("PreloadCache")] = 25;      // 25% (32MB) preloaded before buffer ready
    sets[QStringLiteral("UseDisk")] = false;
    sets[QStringLiteral("ResponsiveMode")] = true;

    QJsonObject root;
    root[QStringLiteral("action")] = QStringLiteral("set");
    root[QStringLiteral("sets")] = sets;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(root).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
        FLUX_LOG_INFO("TorrentManager", "Engine sequential download settings configured successfully");
    });
}

void TorrentManager::startSequentialPreload() {
    if (m_preloadReply) {
        m_preloadReply->abort();
        m_preloadReply->deleteLater();
        m_preloadReply = nullptr;
    }

    if (m_torrentHash.isEmpty() || m_selectedFileIndex == -1) {
        return;
    }

    m_status = QStringLiteral("preload");
    m_statusMessage = QStringLiteral("Starting sequential download buffer...");
    m_bufferProgress = 0.0;
    m_isReadyToPlay = false;
    emit statusChanged();
    emit bufferingChanged();
    emit readyToPlayChanged();

    QUrl preloadUrl(QStringLiteral("http://127.0.0.1:%1/stream?link=%2&index=%3&preload")
                        .arg(m_port)
                        .arg(m_torrentHash)
                        .arg(m_selectedFileIndex));

    QNetworkRequest req(preloadUrl);
    req.setTransferTimeout(120000);

    FLUX_LOG_INFO("TorrentManager", "Triggering sequential preload: " + preloadUrl.toString());
    m_preloadReply = m_nam->get(req);

    connect(m_preloadReply, &QNetworkReply::finished, this, [this]() {
        if (!m_preloadReply) return;
        if (m_preloadReply->error() == QNetworkReply::NoError) {
            FLUX_LOG_INFO("TorrentManager", "Sequential preload buffer filled and ready!");
            m_isReadyToPlay = true;
            m_status = QStringLiteral("ready");
            m_statusMessage = QString::asprintf("Sequential buffer ready (Peers: %d · ⬇ %s)", 
                                               m_activePeers, 
                                               qPrintable(formattedDownloadSpeed()));
            emit readyToPlayChanged();
            emit statusChanged();
            emit bufferingChanged();
            emit torrentReadyToPlay(m_streamUrl, m_selectedFileName.isEmpty() ? m_torrentName : m_selectedFileName);
        } else if (m_preloadReply->error() != QNetworkReply::OperationCanceledError) {
            FLUX_LOG_WARN("TorrentManager", "Preload reply note: " + m_preloadReply->errorString());
        }
        m_preloadReply->deleteLater();
        m_preloadReply = nullptr;
    });
}

void TorrentManager::startTorrent(const QString &magnetOrPath) {
    QString input = magnetOrPath.trimmed();
    if (input.startsWith(QLatin1String("file:///"), Qt::CaseInsensitive)) {
        input = QUrl(input).toLocalFile();
    }

    if (m_preloadReply) {
        m_preloadReply->abort();
        m_preloadReply->deleteLater();
        m_preloadReply = nullptr;
    }
    m_isReadyToPlay = false;
    emit readyToPlayChanged();
    emit bufferingChanged();

    m_status = QStringLiteral("adding");
    m_statusMessage = QStringLiteral("Loading torrent metadata...");
    m_errorMessage.clear();
    m_torrentName = QStringLiteral("Loading...");
    m_torrentHash.clear();
    m_files.clear();
    m_selectedFileIndex = -1;
    m_selectedFileName.clear();
    m_streamUrl.clear();
    m_progress = 0.0;
    m_bufferProgress = 0.0;
    m_downloadSpeed = 0;
    m_uploadSpeed = 0;
    m_activePeers = 0;
    m_totalPeers = 0;

    emit statusChanged();
    emit torrentChanged();
    emit filesChanged();
    emit selectedFileIndexChanged();
    emit streamUrlChanged();
    emit errorChanged();

    if (input.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)) {
        addMagnet(input);
    } else if (QFile::exists(input) && (input.endsWith(QLatin1String(".torrent"), Qt::CaseInsensitive))) {
        uploadTorrentFile(input);
    } else {
        m_status = QStringLiteral("error");
        m_errorMessage = QStringLiteral("Invalid input. Please paste a valid magnet link or drop a .torrent file.");
        emit statusChanged();
        emit errorChanged();
    }
}

void TorrentManager::addMagnet(const QString &magnet) {
    QUrl url(QStringLiteral("http://127.0.0.1:%1/torrents").arg(m_port));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject obj;
    obj[QStringLiteral("action")] = QStringLiteral("add");
    obj[QStringLiteral("link")] = magnet;
    obj[QStringLiteral("save_to_db")] = false;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                m_torrentHash = root[QStringLiteral("hash")].toString();
                m_torrentName = root[QStringLiteral("name")].toString(root[QStringLiteral("title")].toString());
                if (m_torrentName.isEmpty()) m_torrentName = m_torrentHash;

                m_status = QStringLiteral("connecting");
                m_statusMessage = QStringLiteral("Connecting to peers...");
                emit torrentChanged();
                emit statusChanged();

                if (!m_pollTimer->isActive()) {
                    m_pollTimer->start();
                }
                return;
            }
        }
        m_status = QStringLiteral("error");
        m_errorMessage = QStringLiteral("Error adding torrent: ") + reply->errorString();
        emit statusChanged();
        emit errorChanged();
    });
}

void TorrentManager::uploadTorrentFile(const QString &filePath) {
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        m_status = QStringLiteral("error");
        m_errorMessage = QStringLiteral("Could not read .torrent file.");
        emit statusChanged();
        emit errorChanged();
        delete file;
        return;
    }

    QUrl url(QStringLiteral("http://127.0.0.1:%1/torrent/upload").arg(m_port));
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/x-bittorrent")));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(filePath).fileName())));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest req(url);
    QNetworkReply *reply = m_nam->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            parseStatusResponse(data);
            if (!m_pollTimer->isActive()) {
                m_pollTimer->start();
            }
            return;
        }
        m_status = QStringLiteral("error");
        m_errorMessage = QStringLiteral("Failed to upload torrent: ") + reply->errorString();
        emit statusChanged();
        emit errorChanged();
    });
}

void TorrentManager::onPollTimer() {
    if (m_torrentHash.isEmpty()) {
        m_pollTimer->stop();
        return;
    }

    QUrl url(QStringLiteral("http://127.0.0.1:%1/torrents").arg(m_port));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject obj;
    obj[QStringLiteral("action")] = QStringLiteral("get");
    obj[QStringLiteral("hash")] = m_torrentHash;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            parseStatusResponse(reply->readAll());
        }
    });
}

void TorrentManager::parseStatusResponse(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    if (m_torrentHash.isEmpty()) {
        m_torrentHash = root[QStringLiteral("hash")].toString();
    }

    QString name = root[QStringLiteral("name")].toString(root[QStringLiteral("title")].toString());
    if (!name.isEmpty() && name != m_torrentName) {
        m_torrentName = name;
        emit torrentChanged();
    }

    m_totalSize = root[QStringLiteral("torrent_size")].toVariant().toLongLong();
    m_downloadSpeed = root[QStringLiteral("download_speed")].toVariant().toLongLong();
    m_uploadSpeed = root[QStringLiteral("upload_speed")].toVariant().toLongLong();
    m_activePeers = root[QStringLiteral("active_peers")].toInt(0);
    m_totalPeers = root[QStringLiteral("total_peers")].toInt(0);

    const qint64 preloaded = root[QStringLiteral("preloaded_bytes")].toVariant().toLongLong();
    const qint64 preloadSize = root[QStringLiteral("preload_size")].toVariant().toLongLong();
    if (preloadSize > 0) {
        m_bufferProgress = qBound(0.0, static_cast<double>(preloaded) / static_cast<double>(preloadSize), 1.0);
    } else {
        m_bufferProgress = 0.0;
    }

    if (m_totalSize > 0) {
        const qint64 downloaded = root[QStringLiteral("loaded_bytes")].toVariant().toLongLong();
        m_progress = qBound(0.0, static_cast<double>(downloaded) / static_cast<double>(m_totalSize), 1.0);
    }

    // Process file list
    QJsonArray fileArr = root[QStringLiteral("file_stats")].toArray();
    if (!fileArr.isEmpty()) {
        QVariantList files;
        int largestVideoId = -1;
        qint64 largestVideoSize = -1;

        for (int i = 0; i < fileArr.size(); ++i) {
            QJsonObject f = fileArr[i].toObject();
            int id = f[QStringLiteral("id")].toInt(i + 1);
            QString path = f[QStringLiteral("path")].toString();
            qint64 len = f[QStringLiteral("length")].toVariant().toLongLong();
            bool isVid = isVideoFile(path);

            if (isVid && len > largestVideoSize) {
                largestVideoSize = len;
                largestVideoId = id;
            }

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("name")] = path;
            item[QStringLiteral("size")] = len;
            item[QStringLiteral("formattedSize")] = formatBytes(len);
            item[QStringLiteral("isVideo")] = isVid;
            item[QStringLiteral("selected")] = (m_selectedFileIndex == id);
            files.append(item);
        }

        m_files = files;
        emit filesChanged();

        // Default to largest video file if none chosen yet
        if (m_selectedFileIndex == -1 && largestVideoId != -1) {
            selectFile(largestVideoId);
        }
    }

    const int statCode = root[QStringLiteral("stat")].toInt();
    const QString statString = root[QStringLiteral("stat_string")].toString();

    if (statCode == 1) { // Getting info
        m_status = QStringLiteral("connecting");
        m_statusMessage = QStringLiteral("Fetching metadata & peers...");
    } else if (statCode == 2) { // Preload
        m_status = QStringLiteral("preload");
        m_statusMessage = QString::asprintf("Buffering: %.0f%% (Peers: %d · ⬇ %s)", 
                                           m_bufferProgress * 100.0, 
                                           m_activePeers, 
                                           qPrintable(formattedDownloadSpeed()));
    } else if (statCode == 3 || m_isReadyToPlay) { // Working / Ready
        m_status = QStringLiteral("ready");
        m_statusMessage = QString::asprintf("Sequential buffer ready (Peers: %d · ⬇ %s)", 
                                           m_activePeers, 
                                           qPrintable(formattedDownloadSpeed()));
    } else if (!statString.isEmpty()) {
        m_statusMessage = statString;
    }

    if (m_bufferProgress >= 1.0 && !m_isReadyToPlay && !m_streamUrl.isEmpty()) {
        m_isReadyToPlay = true;
        emit readyToPlayChanged();
        emit bufferingChanged();
        emit torrentReadyToPlay(m_streamUrl, m_selectedFileName.isEmpty() ? m_torrentName : m_selectedFileName);
    }

    emit statsChanged();
    emit statusChanged();
}

void TorrentManager::selectFile(int fileIndex) {
    if (m_selectedFileIndex == fileIndex) return;

    m_selectedFileIndex = fileIndex;
    m_selectedFileName.clear();
    m_isReadyToPlay = false;
    emit readyToPlayChanged();
    emit bufferingChanged();

    for (int i = 0; i < m_files.size(); ++i) {
        QVariantMap map = m_files[i].toMap();
        bool isSel = (map[QStringLiteral("id")].toInt() == fileIndex);
        map[QStringLiteral("selected")] = isSel;
        m_files[i] = map;
        if (isSel) {
            m_selectedFileName = map[QStringLiteral("name")].toString();
        }
    }

    emit filesChanged();
    emit selectedFileIndexChanged();
    updateStreamUrl();

    // Start sequential downloading / preloading immediately
    startSequentialPreload();
}

void TorrentManager::updateStreamUrl() {
    if (m_torrentHash.isEmpty() || m_selectedFileIndex == -1) {
        m_streamUrl.clear();
        emit streamUrlChanged();
        return;
    }

    // TorrServer stream url format:
    // http://127.0.0.1:<port>/stream?link=<hash>&index=<file_id>&play
    m_streamUrl = QStringLiteral("http://127.0.0.1:%1/stream?link=%2&index=%3&play")
                      .arg(m_port)
                      .arg(m_torrentHash)
                      .arg(m_selectedFileIndex);

    emit streamUrlChanged();
    FLUX_LOG_INFO("TorrentManager", "Generated stream URL: " + m_streamUrl);
}

void TorrentManager::stopTorrent() {
    if (m_pollTimer) m_pollTimer->stop();

    if (m_preloadReply) {
        m_preloadReply->abort();
        m_preloadReply->deleteLater();
        m_preloadReply = nullptr;
    }

    if (!m_torrentHash.isEmpty()) {
        QUrl url(QStringLiteral("http://127.0.0.1:%1/torrents").arg(m_port));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QJsonObject obj;
        obj[QStringLiteral("action")] = QStringLiteral("rem");
        obj[QStringLiteral("hash")] = m_torrentHash;

        m_nam->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }

    m_isReadyToPlay = false;
    emit readyToPlayChanged();
    emit bufferingChanged();

    m_status = QStringLiteral("idle");
    m_statusMessage.clear();
    m_torrentName.clear();
    m_torrentHash.clear();
    m_streamUrl.clear();
    m_files.clear();
    m_selectedFileIndex = -1;
    m_selectedFileName.clear();
    m_totalSize = 0;
    m_downloadSpeed = 0;
    m_uploadSpeed = 0;
    m_activePeers = 0;
    m_totalPeers = 0;
    m_progress = 0.0;
    m_bufferProgress = 0.0;

    emit statusChanged();
    emit torrentChanged();
    emit statsChanged();
    emit filesChanged();
    emit selectedFileIndexChanged();
    emit streamUrlChanged();
}


void TorrentManager::search1337x(const QString &query, const QString &category) {
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        m_searchError = QStringLiteral("Please enter a search query.");
        emit searchErrorChanged();
        return;
    }

    if (m_searchReply) {
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }

    m_isSearching1337x = true;
    m_searchError.clear();
    m_searchResults.clear();
    emit searching1337xChanged();
    emit searchErrorChanged();
    emit searchResultsChanged();

    QByteArray encodedQuery = QUrl::toPercentEncoding(trimmed);
    encodedQuery.replace("%20", "+");

    QString cat = category.trimmed();
    QString path;
    if (cat.isEmpty() || cat.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) {
        path = QStringLiteral("/sort-search/%1/seeders/desc/1/").arg(QString::fromUtf8(encodedQuery));
    } else {
        path = QStringLiteral("/sort-category-search/%1/%2/seeders/desc/1/").arg(QString::fromUtf8(encodedQuery), cat);
    }

    const QUrl searchUrl(m_currentBaseUrl + path);
    FLUX_LOG_INFO("TorrentManager", "Searching 1337x: " + searchUrl.toString());

    QNetworkRequest req(searchUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    req.setTransferTimeout(15000);

    m_searchReply = m_nam->get(req);
    connect(m_searchReply, &QNetworkReply::finished, this, [this, searchUrl]() {
        if (!m_searchReply) return;
        m_isSearching1337x = false;

        if (m_searchReply->error() == QNetworkReply::NoError) {
            const QString html = QString::fromUtf8(m_searchReply->readAll());
            parse1337xSearchHtml(html, m_currentBaseUrl);
        } else if (m_searchReply->error() != QNetworkReply::OperationCanceledError) {
            m_searchError = QStringLiteral("Search failed: %1").arg(m_searchReply->errorString());
            FLUX_LOG_WARN("TorrentManager", "1337x search error: " + m_searchReply->errorString());
        }

        m_searchReply->deleteLater();
        m_searchReply = nullptr;

        emit searching1337xChanged();
        emit searchErrorChanged();
        emit searchResultsChanged();
    });
}

void TorrentManager::cancelSearch1337x() {
    if (m_searchReply) {
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }
    if (m_isSearching1337x) {
        m_isSearching1337x = false;
        emit searching1337xChanged();
    }
}

void TorrentManager::clearSearchResults() {
    cancelSearch1337x();
    m_searchResults.clear();
    m_searchError.clear();
    emit searchResultsChanged();
    emit searchErrorChanged();
}

QString TorrentManager::decodeHtmlEntities(const QString &raw) {
    QString res = raw;
    res.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    res.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    res.replace(QStringLiteral("&#039;"), QStringLiteral("'"));
    res.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    res.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    res.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    res.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    return res.trimmed();
}

void TorrentManager::parse1337xSearchHtml(const QString &html, const QString &baseUrl) {
    m_searchResults.clear();

    const static QRegularExpression tbodyRegex(QStringLiteral("<tbody>(.*?)</tbody>"), QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch tbodyMatch = tbodyRegex.match(html);
    if (!tbodyMatch.hasMatch()) {
        m_searchError = QStringLiteral("No results found on 1337x.");
        return;
    }

    const QString tbody = tbodyMatch.captured(1);
    const static QRegularExpression rowRegex(QStringLiteral("<tr>(.*?)</tr>"), QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator rowIt = rowRegex.globalMatch(tbody);

    const static QRegularExpression nameRegex(QStringLiteral("<td[^>]*class=\"[^\"]*name[^\"]*\"[^>]*>.*?<a[^>]*href=\"(/torrent/\\d+/[^\"]+)\"[^>]*>(.*?)</a>"), QRegularExpression::DotMatchesEverythingOption);
    const static QRegularExpression seedsRegex(QStringLiteral("<td[^>]*class=\"[^\"]*seeds[^\"]*\"[^>]*>(\\d+)</td>"));
    const static QRegularExpression leechesRegex(QStringLiteral("<td[^>]*class=\"[^\"]*leeches[^\"]*\"[^>]*>(\\d+)</td>"));
    const static QRegularExpression dateRegex(QStringLiteral("<td[^>]*class=\"[^\"]*coll-date[^\"]*\"[^>]*>(.*?)</td>"));
    const static QRegularExpression sizeRegex(QStringLiteral("<td[^>]*class=\"[^\"]*size[^\"]*\"[^>]*>([0-9.]+\\s*[A-Za-z]+)"));
    const static QRegularExpression uploaderRegex(QStringLiteral("<td[^>]*class=\"[^\"]*uploader[^\"]*\"[^>]*>.*?<a[^>]*>([^<]+)</a>"));

    int index = 0;
    while (rowIt.hasNext()) {
        const QString rowHtml = rowIt.next().captured(1);
        const QRegularExpressionMatch nameMatch = nameRegex.match(rowHtml);
        if (!nameMatch.hasMatch()) continue;

        const QString path = nameMatch.captured(1);
        const QString name = decodeHtmlEntities(nameMatch.captured(2));

        int seeds = 0;
        const QRegularExpressionMatch seedsMatch = seedsRegex.match(rowHtml);
        if (seedsMatch.hasMatch()) {
            seeds = seedsMatch.captured(1).toInt();
        }

        int leeches = 0;
        const QRegularExpressionMatch leechesMatch = leechesRegex.match(rowHtml);
        if (leechesMatch.hasMatch()) {
            leeches = leechesMatch.captured(1).toInt();
        }

        QString date;
        const QRegularExpressionMatch dateMatch = dateRegex.match(rowHtml);
        if (dateMatch.hasMatch()) {
            date = decodeHtmlEntities(dateMatch.captured(1));
        }

        QString size;
        const QRegularExpressionMatch sizeMatch = sizeRegex.match(rowHtml);
        if (sizeMatch.hasMatch()) {
            size = decodeHtmlEntities(sizeMatch.captured(1));
        }

        QString uploader;
        const QRegularExpressionMatch uploaderMatch = uploaderRegex.match(rowHtml);
        if (uploaderMatch.hasMatch()) {
            uploader = decodeHtmlEntities(uploaderMatch.captured(1));
        }

        QVariantMap item;
        item[QStringLiteral("index")] = index++;
        item[QStringLiteral("name")] = name;
        item[QStringLiteral("detailPath")] = path;
        item[QStringLiteral("fullDetailUrl")] = baseUrl + path;
        item[QStringLiteral("seeds")] = seeds;
        item[QStringLiteral("leeches")] = leeches;
        item[QStringLiteral("date")] = date;
        item[QStringLiteral("size")] = size;
        item[QStringLiteral("uploader")] = uploader;
        item[QStringLiteral("magnet")] = QString();
        item[QStringLiteral("isResolving")] = false;

        m_searchResults.append(item);
    }

    if (m_searchResults.isEmpty()) {
        m_searchError = QStringLiteral("No matching torrents found.");
    } else {
        FLUX_LOG_INFO("TorrentManager", QString("Parsed %1 torrents from 1337x").arg(m_searchResults.size()));
    }
}

void TorrentManager::streamSearchResult(int index) {
    if (index < 0 || index >= m_searchResults.size()) {
        return;
    }

    QVariantMap item = m_searchResults[index].toMap();
    const QString existingMagnet = item.value(QStringLiteral("magnet")).toString();
    if (!existingMagnet.isEmpty() && existingMagnet.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)) {
        FLUX_LOG_INFO("TorrentManager", "Using cached magnet link for item " + QString::number(index));
        startTorrent(existingMagnet);
        return;
    }

    const QString fullUrl = item.value(QStringLiteral("fullDetailUrl")).toString();
    if (fullUrl.isEmpty()) {
        return;
    }

    if (m_detailReply) {
        m_detailReply->abort();
        m_detailReply->deleteLater();
        m_detailReply = nullptr;
    }

    m_isResolvingMagnet = true;
    m_resolvingIndex = index;
    m_resolvingTorrentName = item.value(QStringLiteral("name")).toString();

    item[QStringLiteral("isResolving")] = true;
    m_searchResults[index] = item;
    emit resolvingMagnetChanged();
    emit searchResultsChanged();

    FLUX_LOG_INFO("TorrentManager", "Resolving magnet link from: " + fullUrl);

    QNetworkRequest req;
    req.setUrl(QUrl(fullUrl));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"));
    req.setTransferTimeout(15000);

    m_detailReply = m_nam->get(req);
    connect(m_detailReply, &QNetworkReply::finished, this, [this, index]() {
        if (!m_detailReply) return;

        m_isResolvingMagnet = false;
        m_resolvingIndex = -1;
        m_resolvingTorrentName.clear();

        if (index >= 0 && index < m_searchResults.size()) {
            QVariantMap curItem = m_searchResults[index].toMap();
            curItem[QStringLiteral("isResolving")] = false;

            if (m_detailReply->error() == QNetworkReply::NoError) {
                const QString html = QString::fromUtf8(m_detailReply->readAll());
                const static QRegularExpression magnetRegex(QStringLiteral("href=\"(magnet:\\?[^\"]+)\""));
                const QRegularExpressionMatch magnetMatch = magnetRegex.match(html);

                if (magnetMatch.hasMatch()) {
                    QString magnet = magnetMatch.captured(1);
                    magnet.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
                    curItem[QStringLiteral("magnet")] = magnet;
                    m_searchResults[index] = curItem;

                    FLUX_LOG_INFO("TorrentManager", "Magnet resolved successfully! Starting stream...");
                    startTorrent(magnet);
                } else {
                    m_searchError = QStringLiteral("Could not find magnet link on detail page.");
                    emit searchErrorChanged();
                }
            } else if (m_detailReply->error() != QNetworkReply::OperationCanceledError) {
                m_searchError = QStringLiteral("Failed to fetch torrent details: %1").arg(m_detailReply->errorString());
                emit searchErrorChanged();
            }

            m_searchResults[index] = curItem;
        }

        m_detailReply->deleteLater();
        m_detailReply = nullptr;

        emit resolvingMagnetChanged();
        emit searchResultsChanged();
    });
}

void TorrentManager::copyMagnet(int index) {
    if (index < 0 || index >= m_searchResults.size()) {
        return;
    }

    QVariantMap item = m_searchResults[index].toMap();
    const QString existingMagnet = item.value(QStringLiteral("magnet")).toString();
    if (!existingMagnet.isEmpty() && existingMagnet.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)) {
        QGuiApplication::clipboard()->setText(existingMagnet);
        emit magnetCopied(existingMagnet);
        return;
    }

    const QString fullUrl = item.value(QStringLiteral("fullDetailUrl")).toString();
    if (fullUrl.isEmpty()) {
        return;
    }

    if (m_detailReply) {
        m_detailReply->abort();
        m_detailReply->deleteLater();
        m_detailReply = nullptr;
    }

    m_isResolvingMagnet = true;
    m_resolvingIndex = index;
    m_resolvingTorrentName = item.value(QStringLiteral("name")).toString();

    item[QStringLiteral("isResolving")] = true;
    m_searchResults[index] = item;
    emit resolvingMagnetChanged();
    emit searchResultsChanged();

    QNetworkRequest req;
    req.setUrl(QUrl(fullUrl));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"));
    req.setTransferTimeout(15000);

    m_detailReply = m_nam->get(req);
    connect(m_detailReply, &QNetworkReply::finished, this, [this, index]() {
        if (!m_detailReply) return;

        m_isResolvingMagnet = false;
        m_resolvingIndex = -1;
        m_resolvingTorrentName.clear();

        if (index >= 0 && index < m_searchResults.size()) {
            QVariantMap curItem = m_searchResults[index].toMap();
            curItem[QStringLiteral("isResolving")] = false;

            if (m_detailReply->error() == QNetworkReply::NoError) {
                const QString html = QString::fromUtf8(m_detailReply->readAll());
                const static QRegularExpression magnetRegex(QStringLiteral("href=\"(magnet:\\?[^\"]+)\""));
                const QRegularExpressionMatch magnetMatch = magnetRegex.match(html);

                if (magnetMatch.hasMatch()) {
                    QString magnet = magnetMatch.captured(1);
                    magnet.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
                    curItem[QStringLiteral("magnet")] = magnet;
                    m_searchResults[index] = curItem;

                    QGuiApplication::clipboard()->setText(magnet);
                    emit magnetCopied(magnet);
                } else {
                    m_searchError = QStringLiteral("Could not find magnet link on detail page.");
                    emit searchErrorChanged();
                }
            } else if (m_detailReply->error() != QNetworkReply::OperationCanceledError) {
                m_searchError = QStringLiteral("Failed to fetch torrent details: %1").arg(m_detailReply->errorString());
                emit searchErrorChanged();
            }

            m_searchResults[index] = curItem;
        }

        m_detailReply->deleteLater();
        m_detailReply = nullptr;

        emit resolvingMagnetChanged();
        emit searchResultsChanged();
    });
}

} // namespace Flux
