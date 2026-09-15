#pragma once

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QStringList>

class CoverService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString lastStatus READ lastStatus NOTIFY lastStatusChanged)

public:
    explicit CoverService(QObject *parent = nullptr);
    void shutdown();

    int revision() const { return m_revision; }
    QString lastStatus() const { return m_lastStatus; }

    Q_INVOKABLE QString coverSource(const QString &appId);
    Q_INVOKABLE QString bannerSource(const QString &appId);
    Q_INVOKABLE QString cachePath() const;
    Q_INVOKABLE QString bannerCachePath() const;
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void clearCover(const QString &appId);
    Q_INVOKABLE void clearBanner(const QString &appId);
    Q_INVOKABLE void clearArtwork(const QString &appId);
    Q_INVOKABLE void refreshCover(const QString &appId);
    Q_INVOKABLE void refreshBanner(const QString &appId);
    Q_INVOKABLE void refreshArtwork(const QString &appId);
    Q_INVOKABLE void refreshAll(const QStringList &appIds);

signals:
    void revisionChanged();
    void lastStatusChanged();

private:
    QString cacheRoot() const;
    QString coverFilePath(const QString &appId) const;
    QString bannerFilePath(const QString &appId) const;
    QStringList candidateUrls(const QString &appId) const;
    QStringList candidateBannerUrls(const QString &appId) const;
    QString findLocalSteamCover(const QString &appId) const;
    QString findLocalSteamBanner(const QString &appId) const;
    bool importLocalSteamCover(const QString &appId, const QString &destination);
    bool importLocalSteamBanner(const QString &appId, const QString &destination);
    void startRequest(const QString &appId,
                      const QString &destination,
                      const QStringList &urls,
                      int urlIndex,
                      bool banner = false);
    void finishRequest(const QString &appId,
                       const QString &destination,
                       const QByteArray &data,
                       bool banner = false);
    void setLastStatus(const QString &status);
    void loadNegativeCache();
    void saveNegativeCache() const;
    void rememberMissing(const QString &appId, bool banner);
    void forgetMissing(const QString &appId, bool banner);

    QNetworkAccessManager m_network;
    QSet<QString> m_pending;
    QSet<QString> m_missing;
    QSet<QString> m_bannerPending;
    QSet<QString> m_bannerMissing;
    QHash<QString, QDateTime> m_missingCheckedUtc;
    QHash<QString, QDateTime> m_bannerMissingCheckedUtc;
    int m_revision = 0;
    QString m_lastStatus;
};
