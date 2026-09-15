#include "PortalFileDialog.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace {
constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kFileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr auto kRequestInterface = "org.freedesktop.portal.Request";
}

PortalFileDialog::PortalFileDialog(QObject *parent)
    : QObject(parent) {
}

void PortalFileDialog::openFile(const QString &requestId,
                                const QString &title,
                                const QString &currentPath) {
    startRequest(QStringLiteral("OpenFile"), requestId, title, currentPath, false, {});
}

void PortalFileDialog::openFolder(const QString &requestId,
                                  const QString &title,
                                  const QString &currentPath) {
    startRequest(QStringLiteral("OpenFile"), requestId, title, currentPath, true, {});
}

void PortalFileDialog::saveFile(const QString &requestId,
                                const QString &title,
                                const QString &currentPath,
                                const QString &suggestedName) {
    startRequest(QStringLiteral("SaveFile"), requestId, title, currentPath, false, suggestedName);
}

QByteArray PortalFileDialog::portalCurrentFolder(const QString &path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return {};

    const QFileInfo info(trimmed);
    QString folderPath;
    if (info.exists() && info.isDir())
        folderPath = info.absoluteFilePath();
    else
        folderPath = info.absolutePath();

    if (folderPath.isEmpty())
        return {};

    QByteArray encoded = QFile::encodeName(folderPath);
    encoded.append('\0');
    return encoded;
}

void PortalFileDialog::startRequest(const QString &method,
                                    const QString &requestId,
                                    const QString &title,
                                    const QString &currentPath,
                                    bool directory,
                                    const QString &suggestedName) {
    if (requestId.trimmed().isEmpty())
        return;

    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(requestId, QStringLiteral("The desktop portal is unavailable because the session D-Bus is not connected."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("handle_token"),
                   QStringLiteral("reno119_%1").arg(QUuid::createUuid().toString(QUuid::Id128)));

    if (directory)
        options.insert(QStringLiteral("directory"), true);

    const QByteArray folder = portalCurrentFolder(currentPath);
    if (!folder.isEmpty())
        options.insert(QStringLiteral("current_folder"), folder);

    if (method == QStringLiteral("SaveFile") && !suggestedName.trimmed().isEmpty())
        options.insert(QStringLiteral("current_name"), suggestedName.trimmed());

    auto *portal = new QDBusInterface(QString::fromLatin1(kPortalService),
                                      QString::fromLatin1(kPortalPath),
                                      QString::fromLatin1(kFileChooserInterface),
                                      bus,
                                      this);
    if (!portal->isValid()) {
        portal->deleteLater();
        emit failed(requestId, QStringLiteral("The XDG desktop portal FileChooser interface is unavailable."));
        return;
    }

    QDBusPendingCall call = portal->asyncCall(method, QString(), title, options);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, portal, requestId](QDBusPendingCallWatcher *) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        portal->deleteLater();

        if (reply.isError()) {
            emit failed(requestId, reply.error().message());
            return;
        }

        const QString requestPath = reply.value().path();
        if (requestPath.isEmpty()) {
            emit failed(requestId, QStringLiteral("The desktop portal returned an empty request handle."));
            return;
        }

        m_requestIds.insert(requestPath, requestId);
        const bool connected = QDBusConnection::sessionBus().connect(
            QString::fromLatin1(kPortalService),
            requestPath,
            QString::fromLatin1(kRequestInterface),
            QStringLiteral("Response"),
            this,
            SLOT(handlePortalResponse(uint,QVariantMap)));
        if (!connected) {
            m_requestIds.remove(requestPath);
            emit failed(requestId, QStringLiteral("Could not listen for the desktop portal file chooser response."));
        }
    });
}

void PortalFileDialog::handlePortalResponse(uint response, const QVariantMap &results) {
    const QString requestPath = calledFromDBus() ? message().path() : QString();
    const QString requestId = m_requestIds.take(requestPath);
    if (requestId.isEmpty())
        return;

    QDBusConnection::sessionBus().disconnect(
        QString::fromLatin1(kPortalService),
        requestPath,
        QString::fromLatin1(kRequestInterface),
        QStringLiteral("Response"),
        this,
        SLOT(handlePortalResponse(uint,QVariantMap)));

    if (response != 0) {
        emit rejected(requestId);
        return;
    }

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty()) {
        emit failed(requestId, QStringLiteral("The desktop portal did not return a selected file or folder."));
        return;
    }

    emit accepted(requestId, QUrl(uris.constFirst()));
}
