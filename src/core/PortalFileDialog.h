#pragma once

#include <QByteArray>
#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QUrl>
#include <QVariantMap>

class PortalFileDialog final : public QObject, protected QDBusContext {
    Q_OBJECT

public:
    explicit PortalFileDialog(QObject *parent = nullptr);

    Q_INVOKABLE void openFile(const QString &requestId,
                              const QString &title,
                              const QString &currentPath = QString());
    Q_INVOKABLE void openFolder(const QString &requestId,
                                const QString &title,
                                const QString &currentPath = QString());
    Q_INVOKABLE void saveFile(const QString &requestId,
                              const QString &title,
                              const QString &currentPath = QString(),
                              const QString &suggestedName = QString());

signals:
    void accepted(const QString &requestId, const QUrl &url);
    void rejected(const QString &requestId);
    void failed(const QString &requestId, const QString &message);

private slots:
    void handlePortalResponse(uint response, const QVariantMap &results);

private:
    void startRequest(const QString &method,
                      const QString &requestId,
                      const QString &title,
                      const QString &currentPath,
                      bool directory,
                      const QString &suggestedName);
    static QByteArray portalCurrentFolder(const QString &path);

    QHash<QString, QString> m_requestIds;
};
