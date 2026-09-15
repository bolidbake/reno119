#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

class AppSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool amoledBlack READ amoledBlack WRITE setAmoledBlack NOTIFY amoledBlackChanged)
    Q_PROPERTY(bool backupBeforeChanges READ backupBeforeChanges WRITE setBackupBeforeChanges NOTIFY backupBeforeChangesChanged)
    Q_PROPERTY(bool gameCoversEnabled READ gameCoversEnabled WRITE setGameCoversEnabled NOTIFY gameCoversEnabledChanged)
    Q_PROPERTY(bool pursuitModeEnabled READ pursuitModeEnabled WRITE setPursuitModeEnabled NOTIFY pursuitModeEnabledChanged)
    Q_PROPERTY(QString customReShadeSource READ customReShadeSource WRITE setCustomReShadeSource NOTIFY customReShadeChanged)
    Q_PROPERTY(QString customReShadeVersion READ customReShadeVersion WRITE setCustomReShadeVersion NOTIFY customReShadeChanged)
    Q_PROPERTY(QString customReShadeUrl READ customReShadeUrl WRITE setCustomReShadeUrl NOTIFY customReShadeChanged)
    Q_PROPERTY(QString customReShadeFile READ customReShadeFile WRITE setCustomReShadeFile NOTIFY customReShadeChanged)
    Q_PROPERTY(bool customReShadeConfigured READ customReShadeConfigured NOTIFY customReShadeChanged)
    Q_PROPERTY(QString customReShadeSummary READ customReShadeSummary NOTIFY customReShadeChanged)
    Q_PROPERTY(QString settingsTransferStatus READ settingsTransferStatus NOTIFY settingsTransferStatusChanged)
    Q_PROPERTY(bool updateChecksOnStartup READ updateChecksOnStartup WRITE setUpdateChecksOnStartup NOTIFY updatePreferencesChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString themeMode() const;
    bool amoledBlack() const;
    bool backupBeforeChanges() const;
    bool gameCoversEnabled() const;
    bool pursuitModeEnabled() const;
    QString customReShadeSource() const;
    QString customReShadeVersion() const;
    QString customReShadeUrl() const;
    QString customReShadeFile() const;
    bool customReShadeConfigured() const;
    QString customReShadeSummary() const;
    QString settingsTransferStatus() const;
    bool updateChecksOnStartup() const;

    Q_INVOKABLE void setThemeMode(const QString &mode);
    Q_INVOKABLE void setAmoledBlack(bool enabled);
    Q_INVOKABLE void setBackupBeforeChanges(bool enabled);
    Q_INVOKABLE void setGameCoversEnabled(bool enabled);
    Q_INVOKABLE void setPursuitModeEnabled(bool enabled);
    Q_INVOKABLE void setCustomReShadeSource(const QString &source);
    Q_INVOKABLE void setCustomReShadeVersion(const QString &version);
    Q_INVOKABLE void setCustomReShadeUrl(const QString &url);
    Q_INVOKABLE void setCustomReShadeFile(const QString &path);
    Q_INVOKABLE QString localPathFromUrl(const QUrl &url) const;
    Q_INVOKABLE QUrl filePickerFolder(const QString &path) const;
    Q_INVOKABLE QVariantMap viewState() const;
    Q_INVOKABLE void saveViewState(const QVariantMap &state);
    Q_INVOKABLE QString gameNotes(const QString &gameId) const;
    Q_INVOKABLE bool setGameNotes(const QString &gameId, const QString &notes);
    Q_INVOKABLE bool exportConfiguration(const QUrl &fileUrl);
    Q_INVOKABLE bool importConfiguration(const QUrl &fileUrl);
    Q_INVOKABLE void setUpdateChecksOnStartup(bool enabled);
    Q_INVOKABLE QString skippedUpdateTarget(const QString &gameId, const QString &component) const;
    Q_INVOKABLE bool skipUpdateTarget(const QString &gameId, const QString &component, const QString &target);
    Q_INVOKABLE bool clearSkippedUpdateTarget(const QString &gameId, const QString &component);

signals:
    void themeModeChanged();
    void amoledBlackChanged();
    void backupBeforeChangesChanged();
    void gameCoversEnabledChanged();
    void pursuitModeEnabledChanged();
    void customReShadeChanged();
    void settingsTransferStatusChanged();
    void updatePreferencesChanged();

private:
    void writeValue(const QString &key, const QVariant &value);
    void reloadFromSettings();
    void setSettingsTransferStatus(const QString &status);
    static bool isPortableConfigurationKey(const QString &key);
    QString workspaceStatePath() const;
    bool readWorkspaceStateFile(QVariantMap *state) const;
    bool writeWorkspaceStateFile(const QVariantMap &state) const;
    bool clearWorkspaceStateFile() const;
    void migrateLegacyWorkspaceState();

    QSettings m_settings;
    QString m_themeMode = QStringLiteral("system");
    bool m_amoledBlack = false;
    bool m_backupBeforeChanges = true;
    bool m_gameCoversEnabled = true;
    bool m_pursuitModeEnabled = false;
    QString m_customReShadeSource = QStringLiteral("version");
    QString m_customReShadeVersion;
    QString m_customReShadeUrl;
    QString m_customReShadeFile;
    QString m_settingsTransferStatus;
    bool m_updateChecksOnStartup = false;
};
