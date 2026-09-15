#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QStringList>

class ReFrameworkSupportService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool finished READ finished NOTIFY supportChanged)
    Q_PROPERTY(bool onlineDataLoaded READ onlineDataLoaded NOTIFY supportChanged)
    Q_PROPERTY(int revision READ revision NOTIFY supportChanged)
public:
    explicit ReFrameworkSupportService(QObject *parent = nullptr);
    void shutdown();

    bool finished() const { return m_finished; }
    bool onlineDataLoaded() const { return m_onlineDataLoaded; }
    int revision() const { return m_revision; }
    QStringList supportedTitles() const { return m_supportedTitles; }

signals:
    void supportChanged();

private:
    static QStringList fallbackTitles();
    static QString cleanMarkdownTitle(QString value);
    QStringList parseReadme(const QString &markdown) const;
    void mergeTitles(const QStringList &titles);

    QNetworkAccessManager m_net;
    QStringList m_supportedTitles;
    bool m_finished = false;
    bool m_onlineDataLoaded = false;
    int m_revision = 0;
};
