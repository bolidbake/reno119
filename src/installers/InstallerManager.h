#pragma once
#include <QSet>

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QVariantList>
#include <QUrl>
#include <functional>

class AppSettings;
class GameModel;
class RenoDxCatalogService;
class QNetworkReply;
struct GameInfo;

class InstallerManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int updateRevision READ updateRevision NOTIFY updateInfoChanged)
    Q_PROPERTY(int cacheRevision READ cacheRevision NOTIFY cacheInfoChanged)
    Q_PROPERTY(QString renoDxStatus READ renoDxStatus NOTIFY renoDxStatusChanged)
    Q_PROPERTY(QString renoDxStatusAppId READ renoDxStatusAppId NOTIFY renoDxStatusChanged)
    Q_PROPERTY(int inlineStatusRevision READ inlineStatusRevision NOTIFY inlineStatusChanged)
    Q_PROPERTY(bool bulkUpdateBusy READ bulkUpdateBusy NOTIFY bulkUpdateChanged)
    Q_PROPERTY(int bulkUpdateChecked READ bulkUpdateChecked NOTIFY bulkUpdateChanged)
    Q_PROPERTY(int bulkUpdateTotal READ bulkUpdateTotal NOTIFY bulkUpdateChanged)
    Q_PROPERTY(int bulkUpdateAvailableGames READ bulkUpdateAvailableGames NOTIFY bulkUpdateChanged)
    Q_PROPERTY(int bulkUpdateAvailableComponents READ bulkUpdateAvailableComponents NOTIFY bulkUpdateChanged)
    Q_PROPERTY(QString bulkUpdateSummary READ bulkUpdateSummary NOTIFY bulkUpdateChanged)
    Q_PROPERTY(QVariantList updateResults READ updateResults NOTIFY updateQueueChanged)
    Q_PROPERTY(QString updateCacheStatus READ updateCacheStatus NOTIFY bulkUpdateChanged)
    Q_PROPERTY(QString lastUpdateCheck READ lastUpdateCheck NOTIFY bulkUpdateChanged)
    Q_PROPERTY(bool updateQueueBusy READ updateQueueBusy NOTIFY updateQueueChanged)
    Q_PROPERTY(int updateQueueRemaining READ updateQueueRemaining NOTIFY updateQueueChanged)
public:
    InstallerManager(GameModel *games, RenoDxCatalogService *catalog, AppSettings *settings, QObject *parent = nullptr);
    void shutdown();

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    double progress() const { return m_progress; }
    int updateRevision() const { return m_updateRevision; }
    int cacheRevision() const { return m_cacheRevision; }
    QString renoDxStatus() const { return m_renoDxStatus; }
    QString renoDxStatusAppId() const { return m_renoDxStatusAppId; }
    int inlineStatusRevision() const { return m_inlineStatusRevision; }
    bool bulkUpdateBusy() const { return m_bulkUpdateBusy; }
    int bulkUpdateChecked() const { return m_bulkUpdateChecked; }
    int bulkUpdateTotal() const { return m_bulkUpdateTotal; }
    int bulkUpdateAvailableGames() const { return m_bulkUpdateAvailableGames; }
    int bulkUpdateAvailableComponents() const { return m_bulkUpdateAvailableComponents; }
    QString bulkUpdateSummary() const { return m_bulkUpdateSummary; }
    QVariantList updateResults() const { return m_updateResults; }
    QString updateCacheStatus() const;
    QString lastUpdateCheck() const { return m_lastUpdateCheck; }
    bool updateQueueBusy() const { return m_updateQueueActive; }
    int updateQueueRemaining() const { return m_updateQueue.size(); }

    Q_INVOKABLE void installReShade(int row, const QString &channel);
    Q_INVOKABLE void installReShadeOverExternal(int row, const QString &channel);
    Q_INVOKABLE void installReShade64(int row, const QString &channel);
    Q_INVOKABLE void installReShade64OverExternal(int row, const QString &channel);
    Q_INVOKABLE void uninstallReShade(int row);
    Q_INVOKABLE void uninstallReShade64(int row);
    Q_INVOKABLE void installRenoDx(int row);
    Q_INVOKABLE void installRenoDxConfirmed(int row, const QString &confirmedUrl);
    Q_INVOKABLE void installRenoDxOverExternal(int row);
    Q_INVOKABLE void installRenoDxOverExternalConfirmed(int row, const QString &confirmedUrl);
    Q_INVOKABLE void uninstallRenoDx(int row);
    Q_INVOKABLE void installReFramework(int row);
    Q_INVOKABLE void installReFrameworkOverExternal(int row);
    Q_INVOKABLE void uninstallReFramework(int row);
    Q_INVOKABLE QString externalReFrameworkOverwritePreview(int row) const;
    Q_INVOKABLE QVariantMap reFrameworkHotkey(int row) const;
    Q_INVOKABLE bool setReFrameworkHotkey(int row, int virtualKey);
    Q_INVOKABLE void restoreLastBackup(int row);
    Q_INVOKABLE QVariantList backupHistory(int row) const;
    Q_INVOKABLE bool restoreBackup(int row, const QString &backupDir);
    Q_INVOKABLE void checkUpdates(int row);
    Q_INVOKABLE void checkAllUpdates(bool forceRefresh = false);
    Q_INVOKABLE QVariantMap updateInfo(int row) const;
    Q_INVOKABLE QVariantList updateCenterItems() const;
    Q_INVOKABLE void retryFailedUpdates(bool allowNonExactRenoDx);
    Q_INVOKABLE void updateGames(const QVariantList &rows, bool allowNonExactRenoDx);
    Q_INVOKABLE bool skipUpdate(int row, const QString &component);
    Q_INVOKABLE bool clearSkippedUpdate(int row, const QString &component);
    Q_INVOKABLE QVariantList updateHistory(int row) const;
    Q_INVOKABLE bool rollbackUpdate(int row, const QString &backupDir);
    Q_INVOKABLE QVariantList recoveryHistory(int row) const;
    Q_INVOKABLE QVariantMap recoveryPreview(int row, const QString &path) const;
    Q_INVOKABLE bool restoreRecovery(int row, const QString &path, const QString &token);
    Q_INVOKABLE QVariantMap verificationInfo(int row) const;
    Q_INVOKABLE void markVerified(int row);
    Q_INVOKABLE QString recoveryFolder(int row) const;
    Q_INVOKABLE void openRecoveryFolder(int row);
    Q_INVOKABLE QString redactDiagnostics(const QString &text) const;
    Q_INVOKABLE void openGameFolder(int row);
    Q_INVOKABLE void openPrefixFolder(int row);
    Q_INVOKABLE void openRenoDxEngineIniFolder(int row);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QString debugSummary(int row) const;
    Q_INVOKABLE QVariantMap renoDxResolutionInfo(int row) const;
    Q_INVOKABLE QVariantMap renoDxTweaksInfo(int row) const;
    Q_INVOKABLE QVariantMap previewRenoDxTweaks(int row, bool restoring) const;
    Q_INVOKABLE void applyRenoDxTweaks(int row, const QString &token, bool acknowledged);
    Q_INVOKABLE void restoreRenoDxTweaks(int row, const QString &token, bool acknowledged);
    Q_INVOKABLE bool saveDiagnosticsReport(const QUrl &fileUrl, const QString &contents);
    Q_INVOKABLE QString recommendedReShadeVersion(int row) const;
    Q_INVOKABLE QString customReShadeSummary() const;
    Q_INVOKABLE QString cachePath() const;
    Q_INVOKABLE QString cacheSizeString() const;
    Q_INVOKABLE void clearDownloadCache();
    Q_INVOKABLE QString externalReShadeOverwritePreview(int row) const;
    Q_INVOKABLE QString externalReShade64OverwritePreview(int row) const;
    Q_INVOKABLE QVariantMap inlineStatus(int row, const QString &component) const;

signals:
    void busyChanged();
    void statusChanged();
    void progressChanged();
    void updateInfoChanged();
    void cacheInfoChanged();
    void renoDxStatusChanged();
    void inlineStatusChanged();
    void bulkUpdateChanged();
    void updateQueueChanged();

private:
    void setBusy(bool value);
    void setStatus(const QString &text);
    void setProgress(double value);
    void setRenoDxStatus(int row, const QString &text);
    void beginInlineOperation(int row, const QString &component);
    void clearInlineOperation();
    void recordInlineStatus(const QString &text);
    static QString inlineStatusLevel(const QString &text);
    QString rootCacheBase() const;
    QString proxyNameFor(int row) const;
    QString cacheDir() const;
    QString versionCacheDir(const QString &version) const;
    QString backupsBaseDir() const;
    QString statePathForRow(int row) const;
    void beginReShadeInstall(int row, const QString &channel, bool allowExternalOverwrite, bool asReShade64);
    bool deployReShadeDll(int row, const QString &stagedDll, const QString &version, const QString &channel);
    void fetchReShadeHome(int row);
    void installReShadeVersion(int row, const QString &version, const QUrl &url, const QString &channel);
    void downloadReShadeInstaller(int row, const QString &version, const QUrl &url, const QString &channel);
    void installCustomReShade(int row);
    bool extractReShade(const QString &installerPath, const QString &outputDir, QString &error);
    void beginRenoDxInstall(int row, bool allowExternalOverwrite, const QString &confirmedNonExactUrl);
    void downloadRenoDx(int row, const QUrl &url, bool allowExternalOverwrite);
    void beginReFrameworkInstall(int row, bool allowExternalOverwrite);
    void downloadReFramework(int row, const QString &version, const QUrl &url, const QString &takeoverBackup);
    bool extractReFramework(const QString &archivePath, const QString &outputDir, QString &error);
    bool writeState(int row, const QJsonObject &patch);
    QJsonObject readState(int row) const;
    bool ensureReShadeIni(const QString &exeDir);
    QString desiredRenoDxUrlFor(int row) const;
    QString desiredRenoDxUrlForGame(const GameInfo &game) const;
    QString setupFingerprint(int row) const;
    bool validateTweakReview(int row, bool restoring, const QString &token, bool acknowledged);
    QVariantMap buildRenoDxTweakPlan(const GameInfo &game) const;
    QString resolveEngineIniPath(const GameInfo &game, const QVariantMap &plan, QStringList *candidates = nullptr) const;
    bool mergeIniChanges(const QString &path, const QVariantList &changes, QString &error) const;
    bool createRenoDxTweakSnapshot(int row, const QStringList &targets, QString &snapshotDir, QString &error);
    bool restoreRenoDxTweakSnapshot(int row, const QString &snapshotDir, QString &error);
    QJsonObject readStateForGame(const GameInfo &game) const;
    QVariantMap evaluateUpdateInfo(const GameInfo &game, const QJsonObject &state,
                                   const QString &latestReShade, const QString &latestReFramework) const;
    void fetchLatestVersions(std::function<void(const QString &, const QString &)> done,
                             bool forceRefresh = false,
                             const QSet<QString> &requiredSources = {});
    void loadReleaseCache();
    void applyReleaseCache();
    bool releaseCacheFresh(const QString &source) const;
    int rowForAppId(const QString &appId) const;
    QString updateTargetFor(const QVariantMap &info, const QString &component) const;
    void enqueueUpdate(const QString &appId, const QString &component);
    void processNextQueuedUpdate();
    bool startQueuedUpdate(int row, const QString &component);
    QString createBackup(int row, const QString &reason, bool force = false) const;
    QString createExternalReShadeBackup(int row) const;
    QString createExternalReShade64Backup(int row) const;
    QString createExternalReFrameworkBackup(int row) const;
    bool restoreBackupDirectory(int row, const QString &dir) const;
    void clearExternalReShadeOverwriteContext();
    void recordUpdateInfo(const QString &appId, const QVariantMap &info);
    void logLine(const QString &line) const;

    static QString humanSize(qint64 bytes);
    static bool copyFileEnsuringParent(const QString &src, const QString &dst);
    static bool copyDirectoryContents(const QString &srcDir, const QString &dstDir);

    GameModel *m_games;
    RenoDxCatalogService *m_catalog;
    AppSettings *m_settings;
    QNetworkAccessManager m_net;
    bool m_busy = false;
    QString m_status = "Ready";
    double m_progress = 0;
    int m_updateRevision = 0;
    int m_cacheRevision = 0;
    QString m_renoDxStatus;
    QString m_renoDxStatusAppId;
    int m_inlineStatusRevision = 0;
    bool m_bulkUpdateBusy = false;
    int m_bulkUpdateChecked = 0;
    int m_bulkUpdateTotal = 0;
    int m_bulkUpdateAvailableGames = 0;
    int m_bulkUpdateAvailableComponents = 0;
    QString m_bulkUpdateSummary;
    QString m_latestReFrameworkUrl;
    QString m_latestReFrameworkAssetUrl;
    QString m_latestReFrameworkPublishedUtc;
    QString m_latestOptiScalerVersion;
    QString m_latestOptiScalerUrl;
    QString m_latestOptiScalerPublishedUtc;
    QList<QPair<QString, QString>> m_updateQueue;
    bool m_updateQueueActive = false;
    bool m_updateQueueAllowNonExactRenoDx = false;
    QHash<QString, QString> m_updateQueueConfirmedRenoDxUrls;
    QVariantList m_updateResults;
    QVariantMap m_activeUpdateResult;
    bool m_activeUpdateSucceeded = false;
    QString m_lastUpdateCheck;
    QJsonObject m_releaseCache;
    QSet<QString> m_failedReleaseSources;
    bool m_releaseFetchBusy = false;
    QList<std::function<void(const QString &, const QString &)>> m_releaseCallbacks;
    QHash<QString, QVariantMap> m_inlineStatus;
    QString m_activeInlineAppId;
    QString m_activeInlineComponent;
    QHash<QString, QVariantMap> m_updateInfo;
    bool m_externalReShadeOverwriteActive = false;
    QString m_externalReShadeOverwriteAppId;
    QString m_externalReShadeProxy;
    QString m_externalReShadeBackupDir;
    bool m_installAsReShade64 = false;
};
