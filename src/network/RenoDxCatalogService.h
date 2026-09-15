#pragma once

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include "RenoDxTitleMatcher.h"

class RenoDxCatalogService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool catalogRefreshing READ catalogRefreshing NOTIFY catalogChanged)
    Q_PROPERTY(bool catalogStale READ catalogStale NOTIFY catalogChanged)
    Q_PROPERTY(QString catalogCheckedUtc READ catalogCheckedUtc NOTIFY catalogChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY catalogChanged)
    Q_PROPERTY(bool renoDxCatalogReady READ renoDxCatalogReady NOTIFY catalogChanged)
    Q_PROPERTY(bool renoDxCatalogFinished READ renoDxCatalogFinished NOTIFY catalogChanged)
    Q_PROPERTY(bool rhiManifestReady READ rhiManifestReady NOTIFY catalogChanged)
    Q_PROPERTY(bool rhiManifestFinished READ rhiManifestFinished NOTIFY catalogChanged)
public:
    explicit RenoDxCatalogService(QObject *parent = nullptr);
    void shutdown();

    Q_INVOKABLE void refreshCatalog(bool forceRefresh = false);
    bool catalogRefreshing() const { return m_catalogRefreshing || m_rhiRefreshing; }
    bool catalogStale() const { const auto age = m_catalogChecked.secsTo(QDateTime::currentDateTimeUtc()); return m_catalogFailed || !m_catalogChecked.isValid() || age < 0 || age >= 21600; }
    QString catalogCheckedUtc() const { return m_catalogChecked.toString(Qt::ISODate); }
    bool ready() const { return m_renoDxCatalogReady; }
    bool renoDxCatalogReady() const { return m_renoDxCatalogReady; }
    bool renoDxCatalogFinished() const { return m_renoDxCatalogFinished; }
    bool rhiManifestReady() const { return m_rhiManifestReady; }
    bool rhiManifestFinished() const { return m_rhiManifestFinished; }

    QString renoDxSnapshot(const QString &gameName) const;
    QVariantMap renoDxSnapshotInfo(const QString &gameName) const;
    QString recommendedReShadeVersion(const QString &gameName = QString()) const;

    QVariantMap rhiRenoDxIniOverrides(const QString &gameName) const;
    QVariantMap rhiUeExtendedCompatibility(const QString &gameName) const;
    QString rhiEngineIniProfileText(const QString &gameName) const;
    QString rhiEngineIniProfileFile(const QString &gameName) const;
    QString rhiEngineIniPathOverride(const QString &gameName) const;
    Q_INVOKABLE QString rhiGameNote(const QString &gameName) const;
    bool rhiNativeHdrGame(const QString &gameName) const;

signals:
    void catalogChanged();

private:
    void parseRenoDxWiki(const QString &markdown);
    void parseRhiManifest(const QByteArray &json, bool fetchMissingProfiles);
    void fetchRhiEngineIniProfile(const QString &normalizedGameKey, const QString &fileName);
    void loadRhiCache();
    void saveRhiCache() const;
    void refreshRhiManifest(bool forceRefresh = false);
    bool rhiCacheFresh() const;

    QNetworkAccessManager m_net;
    QDateTime m_catalogChecked;
    QDateTime m_rhiChecked;
    bool m_catalogRefreshing = false;
    bool m_catalogFailed = false;
    bool m_renoDxCatalogReady = false;
    bool m_renoDxCatalogFinished = false;
    bool m_rhiManifestReady = false;
    bool m_rhiManifestFinished = false;
    bool m_rhiRefreshing = false;
    QVector<RenoDxCatalogEntry> m_renoDxEntries;
    QString m_renoDxRecommendedReShade;
    QByteArray m_rhiManifestJson;

    QHash<QString, QVariantMap> m_rhiRenoDxIniOverrides;
    QHash<QString, QVariantMap> m_rhiUeCompatibility;
    QHash<QString, QString> m_rhiEngineIniFiles;
    QHash<QString, QString> m_rhiEngineIniProfiles;
    QHash<QString, QDateTime> m_rhiEngineIniProfileChecked;
    QHash<QString, QString> m_rhiEngineIniPathOverrides;
    QHash<QString, QString> m_rhiGameNotes;
    QSet<QString> m_rhiNativeHdrGames;
};
