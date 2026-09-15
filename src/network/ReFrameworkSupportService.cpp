#include "ReFrameworkSupportService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr qint64 kSupportCacheSeconds = 7 * 24 * 60 * 60;

QString supportCachePath() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/reframework-support-v1.json");
}
}

QStringList ReFrameworkSupportService::fallbackTitles() {
    // Local fallback mirrors the currently maintained upstream README list.
    // A cached/online list can extend this when local engine detection is
    // inconclusive, but RE Engine detection itself remains authoritative.
    return {
        QStringLiteral("Resident Evil 2"),
        QStringLiteral("Resident Evil 3"),
        QStringLiteral("Resident Evil 4"),
        QStringLiteral("Resident Evil 7"),
        QStringLiteral("Resident Evil Village"),
        QStringLiteral("Resident Evil Requiem"),
        QStringLiteral("Devil May Cry 5"),
        QStringLiteral("Street Fighter 6"),
        QStringLiteral("Monster Hunter Rise"),
        QStringLiteral("Monster Hunter Wilds"),
        QStringLiteral("Monster Hunter Stories 3"),
        QStringLiteral("Dragon's Dogma 2"),
        QStringLiteral("Dead Rising Deluxe Remaster"),
        QStringLiteral("Ghosts 'n Goblins Resurrection"),
        QStringLiteral("Apollo Justice: Ace Attorney Trilogy"),
        QStringLiteral("Kunitsu-Gami: Path of the Goddess"),
        QStringLiteral("Onimusha 2: Samurai's Destiny"),
        QStringLiteral("Onimusha: Way of the Sword"),
        QStringLiteral("Mega Man Star Force Legacy Collection"),
        // Common aliases used by storefront metadata.
        QStringLiteral("Resident Evil 9"),
        QStringLiteral("Resident Evil 8"),
        QStringLiteral("Dragon's Dogma II"),
        QStringLiteral("Star Force Legacy Collection"),
        QStringLiteral("Pragmata")
    };
}

QString ReFrameworkSupportService::cleanMarkdownTitle(QString value) {
    value = value.trimmed();
    value.remove(QRegularExpression(QStringLiteral(R"(^[-*+]\s+)")));
    value.replace(QRegularExpression(QStringLiteral(R"(\[([^\]]+)\]\([^\)]+\))")), QStringLiteral("\\1"));
    value.remove(QRegularExpression(QStringLiteral(R"(`)")));
    return value.trimmed();
}

ReFrameworkSupportService::ReFrameworkSupportService(QObject *parent)
    : QObject(parent), m_supportedTitles(fallbackTitles()) {
    bool cacheFresh = false;
    QFile cache(supportCachePath());
    if (cache.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(cache.readAll()).object();
        const QDateTime checked = QDateTime::fromString(root.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
        const qint64 age = checked.secsTo(QDateTime::currentDateTimeUtc());
        QStringList cachedTitles;
        for (const QJsonValue &value : root.value(QStringLiteral("titles")).toArray()) {
            const QString title = value.toString().trimmed();
            if (!title.isEmpty())
                cachedTitles << title;
        }
        if (!cachedTitles.isEmpty()) {
            mergeTitles(cachedTitles);
            m_onlineDataLoaded = true;
            cacheFresh = checked.isValid() && age >= 0 && age < kSupportCacheSeconds;
        }
    }

    if (cacheFresh) {
        m_finished = true;
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://raw.githubusercontent.com/praydog/REFramework/master/README.md")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Reno119/" RENO119_VERSION));
    request.setTransferTimeout(15000);
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_finished = true;
        if (reply->error() == QNetworkReply::NoError) {
            const QString markdown = QString::fromUtf8(reply->readAll());
            const QStringList parsed = parseReadme(markdown);
            if (!parsed.isEmpty()) {
                mergeTitles(parsed);
                m_onlineDataLoaded = true;

                QJsonArray titles;
                for (const QString &title : parsed)
                    titles.append(title);
                QJsonObject root;
                root.insert(QStringLiteral("checkedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                root.insert(QStringLiteral("titles"), titles);
                const QString path = supportCachePath();
                QDir().mkpath(QFileInfo(path).absolutePath());
                QSaveFile file(path);
                const QByteArray data = QJsonDocument(root).toJson();
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
                    file.commit();
            }
        }
        ++m_revision;
        emit supportChanged();
        reply->deleteLater();
    });
}

void ReFrameworkSupportService::shutdown() {
    const auto replies = m_net.findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
}

QStringList ReFrameworkSupportService::parseReadme(const QString &markdown) const {
    QStringList parsed;
    bool inSupportedGames = false;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.compare(QStringLiteral("## Supported Games"), Qt::CaseInsensitive) == 0) {
            inSupportedGames = true;
            continue;
        }
        if (!inSupportedGames)
            continue;
        if (line.startsWith(QStringLiteral("## ")))
            break;
        if (!line.startsWith(QLatin1Char('*')) && !line.startsWith(QLatin1Char('-')))
            continue;
        const QString title = cleanMarkdownTitle(line);
        if (!title.isEmpty())
            parsed << title;
    }
    return parsed;
}

void ReFrameworkSupportService::mergeTitles(const QStringList &titles) {
    QSet<QString> seen;
    QStringList merged;
    for (const QString &title : titles + fallbackTitles()) {
        const QString key = title.trimmed().toCaseFolded();
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        merged << title.trimmed();
    }
    m_supportedTitles = merged;
}
