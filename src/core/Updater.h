#pragma once

#include <QObject>
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QUrl>

class QNetworkReply;

namespace Flux {

// Self-updater backed by GitHub Releases (https://github.com/<owner>/<repo>/releases).
//
//  - Asks the GitHub API for the latest release and compares its tag with FLUX_VERSION.
//  - Installed build (Uninstall.exe next to FLUX.exe): downloads  FLUX-<ver>-Setup.exe
//    and runs it silently (/S) after FLUX exits, then relaunches FLUX.
//  - Portable build: downloads  FLUX-<ver>-win64-portable.zip, unpacks it after FLUX
//    exits, copies the files over the current folder, then relaunches FLUX.
//
// QML sees it as `fluxUpdater`. `state` is one of:
//   "idle" | "checking" | "upToDate" | "available" | "downloading" | "installing" | "error"
class Updater : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool portable READ isPortable CONSTANT)
    Q_PROPERTY(QString state READ stateName NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY latestChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    QString currentVersion() const;
    bool isPortable() const { return m_portable; }
    QString stateName() const;
    bool updateAvailable() const { return m_hasUpdate; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseNotes() const { return m_releaseNotes; }
    double progress() const { return m_progress; }
    QString progressText() const;
    QString errorText() const { return m_errorText; }

    // silent = startup check: never shows an error and never interrupts anything
    Q_INVOKABLE void checkForUpdates(bool silent = false);
    // Download the new version and install it (the app restarts by itself)
    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openReleasePage() const;

signals:
    void stateChanged();
    void latestChanged();
    void progressChanged();

private:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Installing, Error };

    void setState(State state);
    void fail(const QString &message);
    void onCheckFinished(QNetworkReply *reply, bool silent);
    void onDownloadFinished(QNetworkReply *reply);
    bool applyUpdate(const QString &packagePath);
    QString workDir() const;
    static bool isTrustedUrl(const QUrl &url);

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;

    State m_state = State::Idle;
    bool m_portable = false;
    bool m_hasUpdate = false;
    QString m_errorText;

    QString m_latestVersion;
    QString m_releaseNotes;
    QString m_releaseUrl;

    // Chosen release asset
    QUrl m_assetUrl;
    QString m_assetName;
    qint64 m_assetSize = 0;
    QString m_assetDigest;   // "sha256:<hex>" as reported by GitHub (may be empty)

    // Download in flight
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_received = 0;
    qint64 m_total = 0;
    double m_progress = 0.0;
};

} // namespace Flux
