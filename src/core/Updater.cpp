#include "Updater.h"
#include "Logger.h"
#include "Version.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

namespace Flux {

namespace {

// Where releases are published. Release assets are expected to be named like
//   FLUX-0.0.4-Setup.exe            (installer)
//   FLUX-0.0.4-win64-portable.zip   (portable)
const char kLatestReleaseApi[] = "https://api.github.com/repos/shifatsrm09/FLUX/releases/latest";
const char kReleasesPage[]     = "https://github.com/shifatsrm09/FLUX/releases";

QVersionNumber parseVersion(const QString &text)
{
    QString t = text.trimmed();
    if (t.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        t.remove(0, 1);
    }
    return QVersionNumber::fromString(t);   // "0.0.4-beta" -> 0.0.4
}

// PowerShell single-quoted literal for a filesystem path
QString psQuote(const QString &path)
{
    QString v = QDir::toNativeSeparators(path);
    v.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + v + QLatin1Char('\'');
}

QString humanSize(qint64 bytes)
{
    const double mb = double(bytes) / (1024.0 * 1024.0);
    return QString::number(mb, 'f', 1) + QStringLiteral(" MB");
}

} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
{
    // An "Uninstall.exe" next to FLUX.exe means the NSIS installer put us here.
    // Anything else (a zip extracted somewhere) is a portable copy.
    const QString appDir = QCoreApplication::applicationDirPath();
    m_portable = !QFile::exists(QDir(appDir).filePath(QStringLiteral("Uninstall.exe")));

    m_network.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    FLUX_LOG_INFO("Updater", QStringLiteral("FLUX %1 (%2 build)")
                                 .arg(QString::fromLatin1(FLUX_VERSION),
                                      m_portable ? QStringLiteral("portable") : QStringLiteral("installed")));

    // Leftovers from a previous update (installer, zip, extracted files)
    QTimer::singleShot(15000, this, [this]() {
        if (m_state != State::Downloading && m_state != State::Installing) {
            QDir(workDir()).removeRecursively();
        }
    });
}

Updater::~Updater()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

QString Updater::currentVersion() const
{
    return QString::fromLatin1(FLUX_VERSION);
}

QString Updater::stateName() const
{
    switch (m_state) {
    case State::Idle:        return QStringLiteral("idle");
    case State::Checking:    return QStringLiteral("checking");
    case State::UpToDate:    return QStringLiteral("upToDate");
    case State::Available:   return QStringLiteral("available");
    case State::Downloading: return QStringLiteral("downloading");
    case State::Installing:  return QStringLiteral("installing");
    case State::Error:       return QStringLiteral("error");
    }
    return QStringLiteral("idle");
}

QString Updater::progressText() const
{
    if (m_state != State::Downloading) {
        return QString();
    }
    if (m_total > 0) {
        return QStringLiteral("%1 of %2").arg(humanSize(m_received), humanSize(m_total));
    }
    return humanSize(m_received);
}

QString Updater::workDir() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("FLUX-update"));
}

bool Updater::isTrustedUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https")) {
        return false;
    }
    const QString host = url.host().toLower();
    return host == QLatin1String("github.com")
        || host.endsWith(QLatin1String(".github.com"))
        || host.endsWith(QLatin1String(".githubusercontent.com"));
}

void Updater::setState(State state)
{
    m_state = state;
    if (state != State::Error) {
        m_errorText.clear();
    }
    emit stateChanged();
}

void Updater::fail(const QString &message)
{
    FLUX_LOG_ERROR("Updater", message);
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_errorText = message;
    m_state = State::Error;
    emit stateChanged();
}

void Updater::openReleasePage() const
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesPage)));
}

// ---- Checking ---------------------------------------------------------------

void Updater::checkForUpdates(bool silent)
{
    if (m_state == State::Checking || m_state == State::Downloading || m_state == State::Installing) {
        return;
    }

    setState(State::Checking);

    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestReleaseApi))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QByteArray("FLUX-Updater/") + FLUX_VERSION);
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_network.get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        onCheckFinished(reply, silent);
    });
}

void Updater::onCheckFinished(QNetworkReply *reply, bool silent)
{
    reply->deleteLater();
    if (m_state != State::Checking) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString detail = reply->error() == QNetworkReply::ContentNotFoundError
            ? QStringLiteral("no releases found")
            : reply->errorString();
        FLUX_LOG_WARN("Updater", "Update check failed: " + detail);
        if (silent) {
            setState(State::Idle);
        } else {
            fail(QStringLiteral("Couldn't check for updates (%1).").arg(detail));
        }
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QVersionNumber latest = parseVersion(tag);
    const QVersionNumber current = parseVersion(QString::fromLatin1(FLUX_VERSION));

    if (latest.isNull()) {
        if (silent) {
            setState(State::Idle);
        } else {
            fail(QStringLiteral("Couldn't read the latest version from GitHub."));
        }
        return;
    }

    FLUX_LOG_INFO("Updater", QStringLiteral("Latest release %1, running %2")
                                 .arg(latest.toString(), current.toString()));

    if (QVersionNumber::compare(latest, current) <= 0) {
        m_hasUpdate = false;
        setState(silent ? State::Idle : State::UpToDate);
        return;
    }

    // Pick the package that matches how this copy of FLUX is installed
    m_assetUrl.clear();
    m_assetName.clear();
    m_assetSize = 0;
    m_assetDigest.clear();

    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString lower = name.toLower();

        const bool match = m_portable
            ? (lower.endsWith(QLatin1String(".zip")) && lower.contains(QLatin1String("portable")))
            : (lower.endsWith(QLatin1String(".exe")) && lower.contains(QLatin1String("setup")));
        if (!match) {
            continue;
        }

        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!isTrustedUrl(url)) {
            continue;
        }
        m_assetUrl = url;
        m_assetName = name;
        m_assetSize = qint64(asset.value(QStringLiteral("size")).toDouble());
        m_assetDigest = asset.value(QStringLiteral("digest")).toString().toLower();
        break;
    }

    m_latestVersion = latest.toString();
    m_releaseNotes = release.value(QStringLiteral("body")).toString().replace(QStringLiteral("\r"), QString()).trimmed();
    m_releaseUrl = release.value(QStringLiteral("html_url")).toString();
    emit latestChanged();

    if (!m_assetUrl.isValid()) {
        // A newer release exists but it has no package for this build type
        m_hasUpdate = false;
        if (silent) {
            setState(State::Idle);
        } else {
            fail(QStringLiteral("Version %1 is out, but it has no %2 package yet.")
                     .arg(m_latestVersion,
                          m_portable ? QStringLiteral("portable") : QStringLiteral("installer")));
        }
        return;
    }

    m_hasUpdate = true;
    setState(State::Available);
}

// ---- Downloading ------------------------------------------------------------

void Updater::startUpdate()
{
    if (m_state != State::Available || !m_assetUrl.isValid()) {
        return;
    }

    // Fresh scratch folder for the package
    QDir dir(workDir());
    dir.removeRecursively();
    if (!QDir().mkpath(dir.path())) {
        fail(QStringLiteral("Couldn't create a temporary folder for the update."));
        return;
    }

    m_file.setFileName(dir.filePath(QFileInfo(m_assetName).fileName()));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("Couldn't write the update file: %1").arg(m_file.errorString()));
        return;
    }

    m_hash.reset();
    m_received = 0;
    m_total = m_assetSize;
    m_progress = 0.0;
    setState(State::Downloading);
    emit progressChanged();

    QNetworkRequest request(m_assetUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("FLUX-Updater/") + FLUX_VERSION);
    request.setTransferTimeout(30000);   // gives up if the connection stalls for 30 s

    FLUX_LOG_INFO("Updater", "Downloading " + m_assetUrl.toString());

    QNetworkReply *reply = m_network.get(request);
    m_reply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_state != State::Downloading || !m_file.isOpen()) {
            return;
        }
        const QByteArray chunk = reply->readAll();
        if (m_file.write(chunk) != chunk.size()) {
            fail(QStringLiteral("Couldn't save the update (disk full?)."));
            reply->abort();
            return;
        }
        m_hash.addData(chunk);
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_received = received;
        if (total > 0) {
            m_total = total;
        }
        m_progress = m_total > 0 ? qBound(0.0, double(received) / double(m_total), 1.0) : 0.0;
        emit progressChanged();
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}

void Updater::cancel()
{
    if (m_state != State::Downloading) {
        return;
    }
    m_state = State::Available;   // so the aborted reply is ignored in onDownloadFinished
    if (m_reply) {
        m_reply->abort();
    }
    if (m_file.isOpen()) {
        m_file.close();
    }
    QDir(workDir()).removeRecursively();
    m_received = 0;
    m_progress = 0.0;
    emit progressChanged();
    emit stateChanged();
}

void Updater::onDownloadFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (m_state != State::Downloading) {
        return;   // cancelled or already failed
    }

    // Anything still buffered
    const QByteArray rest = reply->readAll();
    if (!rest.isEmpty() && m_file.isOpen()) {
        m_file.write(rest);
        m_hash.addData(rest);
    }
    const QString packagePath = m_file.fileName();
    m_file.close();

    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("Download failed: %1").arg(reply->errorString()));
        QFile::remove(packagePath);
        return;
    }

    // Integrity: size and (when GitHub provides one) SHA-256
    if (m_assetSize > 0 && QFileInfo(packagePath).size() != m_assetSize) {
        fail(QStringLiteral("The download was incomplete. Please try again."));
        QFile::remove(packagePath);
        return;
    }
    if (m_assetDigest.startsWith(QLatin1String("sha256:"))) {
        const QString expected = m_assetDigest.mid(7);
        const QString actual = QString::fromLatin1(m_hash.result().toHex());
        if (expected != actual) {
            fail(QStringLiteral("The downloaded file failed its integrity check."));
            QFile::remove(packagePath);
            return;
        }
        FLUX_LOG_INFO("Updater", "SHA-256 verified");
    }

    m_progress = 1.0;
    emit progressChanged();
    setState(State::Installing);

    if (!applyUpdate(packagePath)) {
        return;   // fail() already ran
    }

    // Give the UI a moment to show "Installing…", then quit so the helper can replace our files
    QTimer::singleShot(1200, this, []() { QCoreApplication::quit(); });
}

// ---- Applying ---------------------------------------------------------------
//
// FLUX can't overwrite its own running files, so a small hidden PowerShell script
// does the work after we exit:
//   installed: run FLUX-x.y.z-Setup.exe silently (elevated, into the same folder)
//   portable:  unzip the package and copy it over the current folder
// and then starts FLUX again. Its log is %TEMP%\FLUX-update.log.

bool Updater::applyUpdate(const QString &packagePath)
{
    const QString appExe = QCoreApplication::applicationFilePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("FLUX-update.log"));
    const QString scriptPath = QDir(workDir()).filePath(QStringLiteral("apply-update.ps1"));

    QString script = QStringLiteral(R"PS($ErrorActionPreference = 'Continue'
$log    = %1
$appPid = %2
$appExe = %3
$appDir = %4
function Log($m) { try { Add-Content -LiteralPath $log -Value ((Get-Date -Format 's') + '  ' + $m) } catch {} }
Log '--- FLUX update started'
try { Wait-Process -Id $appPid -Timeout 60 -ErrorAction Stop } catch { }
Get-Process -Name 'FLUX' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $appExe } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
)PS")
        .arg(psQuote(logPath))
        .arg(QCoreApplication::applicationPid())
        .arg(psQuote(appExe))
        .arg(psQuote(appDir));

    if (m_portable) {
        const QString stage = QDir(workDir()).filePath(QStringLiteral("extracted"));
        script += QStringLiteral(R"PS($zip   = %1
$stage = %2
try {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $stage)
    $exe = Get-ChildItem -LiteralPath $stage -Filter 'FLUX.exe' -Recurse -File | Select-Object -First 1
    if (-not $exe) { throw 'FLUX.exe was not found in the package' }
    Log ('copying files from ' + $exe.DirectoryName)
    & robocopy.exe $exe.DirectoryName $appDir /E /IS /IT /R:10 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw ('robocopy failed with code ' + $LASTEXITCODE) }
    Log 'portable update applied'
} catch { Log ('portable update failed: ' + $_.Exception.Message) }
)PS")
                      .arg(psQuote(packagePath), psQuote(stage));
    } else {
        script += QStringLiteral(R"PS($setup = %1
try {
    Log 'running installer'
    $p = Start-Process -FilePath $setup -ArgumentList ('/S /D=' + $appDir) -Verb RunAs -Wait -PassThru
    Log ('installer exit code ' + $p.ExitCode)
} catch { Log ('installer failed: ' + $_.Exception.Message) }
)PS")
                      .arg(psQuote(packagePath));
    }

    script += QStringLiteral(R"PS(Log 'starting FLUX'
try { Start-Process -FilePath $appExe -WorkingDirectory $appDir } catch { Log ('could not restart FLUX: ' + $_.Exception.Message) }
)PS");

    QFile out(scriptPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("Couldn't prepare the update: %1").arg(out.errorString()));
        return false;
    }
    // UTF-8 with BOM so Windows PowerShell 5 reads non-ASCII paths correctly
    out.write("\xEF\xBB\xBF");
    out.write(script.replace(QStringLiteral("\n"), QStringLiteral("\r\n")).toUtf8());
    out.close();

    const QStringList args{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-File"), QDir::toNativeSeparators(scriptPath)
    };
    if (!QProcess::startDetached(QStringLiteral("powershell.exe"), args)) {
        fail(QStringLiteral("Couldn't start the update helper (PowerShell)."));
        return false;
    }

    FLUX_LOG_INFO("Updater", QStringLiteral("Update helper started (%1); exiting to apply version %2")
                                 .arg(m_portable ? QStringLiteral("portable") : QStringLiteral("installer"),
                                      m_latestVersion));
    return true;
}

} // namespace Flux
