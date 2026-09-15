#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QVariantMap>

class PCGamingWikiService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit PCGamingWikiService(QObject *parent = nullptr);
    void shutdown();

    int revision() const { return m_revision; }
    QString status() const { return m_status; }

    Q_INVOKABLE QVariantMap info(const QString &gameKey) const;
    Q_INVOKABLE void resolve(const QString &gameKey, const QString &gameName);
    Q_INVOKABLE void clear(const QString &gameKey);

signals:
    void revisionChanged();
    void statusChanged();

private:
    static QString normalizedTitle(const QString &text);
    void setStatus(const QString &status);
    void loadCache();
    void saveCache() const;

    QNetworkAccessManager m_network;
    QHash<QString, QVariantMap> m_info;
    QSet<QString> m_pending;
    int m_revision = 0;
    QString m_status = QStringLiteral("Ready");
};
