#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <QHash>

class GameModel;

class OptiScalerIntegration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int statusRevision READ statusRevision NOTIFY statusRevisionChanged)

public:
    explicit OptiScalerIntegration(GameModel *games, QObject *parent = nullptr);

    int revision() const { return m_revision; }
    QString status() const { return m_status; }
    int statusRevision() const { return m_statusRevision; }

    Q_INVOKABLE QVariantMap analyze(int row) const;
    Q_INVOKABLE QVariantMap preview(int row, const QString &proxyChoice, const QString &methodChoice) const;
    Q_INVOKABLE QVariantMap appliedChoices(int row) const;
    Q_INVOKABLE bool apply(int row, const QString &proxyChoice, const QString &methodChoice);
    Q_INVOKABLE bool fixSetup(int row);
    Q_INVOKABLE bool revert(int row);
    Q_INVOKABLE bool saveWorkingConfiguration(int row);
    Q_INVOKABLE bool restoreWorkingConfiguration(int row);
    Q_INVOKABLE QStringList history(int row) const;
    Q_INVOKABLE QVariantList restorePoints(int row) const;
    Q_INVOKABLE bool restorePoint(int row, const QString &snapshotDir);
    Q_INVOKABLE QVariantMap inlineStatus(int row) const;
    Q_INVOKABLE QVariantMap overlayHotkey(int row) const;
    Q_INVOKABLE bool setOverlayHotkey(int row, int virtualKey);

signals:
    void revisionChanged();
    void statusChanged();
    void statusRevisionChanged();

private:
    struct Plan {
        bool detected = false;
        bool canApply = false;
        bool hasFileChanges = false;
        QString exeDir;
        QString iniPath;
        QString currentOptiProxy;
        QString targetOptiProxy;
        QString currentReShadePath;
        QString targetReShadePath;
        QString method;
        QString currentLoadReShade;
        QString targetLoadReShade;
        QString wineOverrides;
        QString steamLaunchOptions;
        QString summary;
        QStringList changes;
        QStringList touchedRelativePaths;
    };

    QVariantMap analysisMap(int row) const;
    Plan makePlan(int row, const QString &proxyChoice, const QString &methodChoice) const;
    QString exeDirFor(int row) const;
    QString integrationStatePath(int row) const;
    QString appStatePath(int row) const;
    QVariantMap readIntegrationState(int row) const;
    bool writeIntegrationState(int row, const QVariantMap &state) const;
    QString readManagedReShadePath(int row) const;
    QString readReno119ReShade64Path(int row) const;
    bool writeManagedReShadePath(int row, const QString &relativePath) const;
    QString findOptiIni(const QString &exeDir) const;
    QString detectOptiProxy(int row, const QString &exeDir, const QString &reshadePath) const;
    QString resolveProxy(const QString &choice, const QString &currentProxy, const QString &reshadePath, const QString &exeDir) const;
    QString resolveMethod(const QString &choice, const QString &targetProxy, const QString &reshadePath) const;
    QString readIniValue(const QString &path, const QString &section, const QString &key) const;
    bool writeIniValue(const QString &path, const QString &section, const QString &key, const QString &value) const;
    QString createTransactionSnapshot(int row, const QStringList &relativePaths) const;
    bool restoreTransactionSnapshot(int row, const QString &snapshotDir) const;
    bool moveRelative(const QString &exeDir, const QString &from, const QString &to) const;
    static QString normalizeProxyChoice(const QString &choice);
    static QString normalizeMethodChoice(const QString &choice);
    static QString proxyChoiceFromIndex(int index);
    static QString methodChoiceFromIndex(int index);
    static int proxyChoiceIndex(const QString &choice);
    static int methodChoiceIndex(const QString &choice);
    static QString proxyBaseName(const QString &filename);
    static QString hashFile(const QString &path);
    static QStringList supportedProxies();
    static void appendHistory(QVariantMap &state, const QString &entry);
    void beginStatusForRow(int row);
    void setStatus(const QString &status);
    static QString statusLevel(const QString &status);
    void bumpRevision();

    GameModel *m_games = nullptr;
    int m_revision = 0;
    QString m_status = QStringLiteral("Ready");
    int m_statusRevision = 0;
    QString m_activeStatusAppId;
    QHash<QString, QVariantMap> m_inlineStatus;
};
