#include "PCGamingWikiService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr qint64 kPcgwCacheSeconds = 30LL * 24 * 60 * 60;

QString pcgwCachePath() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/pcgamingwiki-v1.json");
}
}

PCGamingWikiService::PCGamingWikiService(QObject *parent)
    : QObject(parent) {
    loadCache();
}

void PCGamingWikiService::shutdown() {
    const auto replies = m_network.findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
    m_pending.clear();
}

QString PCGamingWikiService::normalizedTitle(const QString &text) {
    QString out = text.normalized(QString::NormalizationForm_KD).toLower();
    out.remove(QChar(0x00AE)); // registered trademark
    out.remove(QChar(0x2122)); // trademark
    QString cleaned;
    cleaned.reserve(out.size());
    for (const QChar c : out) {
        if (c.isLetterOrNumber())
            cleaned += c;
    }
    return cleaned;
}

void PCGamingWikiService::setStatus(const QString &status) {
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void PCGamingWikiService::loadCache() {
    QFile file(pcgwCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject entries = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("entries")).toObject();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject obj = it.value().toObject();
        const QDateTime checked = QDateTime::fromString(obj.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
        const qint64 age = checked.secsTo(now);
        if (!checked.isValid() || age < 0 || age >= kPcgwCacheSeconds || !obj.value(QStringLiteral("resolved")).toBool())
            continue;
        m_info.insert(it.key(), obj.toVariantMap());
    }
}

void PCGamingWikiService::saveCache() const {
    QJsonObject entries;
    for (auto it = m_info.constBegin(); it != m_info.constEnd(); ++it) {
        if (!it.value().value(QStringLiteral("resolved")).toBool())
            continue;
        entries.insert(it.key(), QJsonObject::fromVariantMap(it.value()));
    }
    const QString path = pcgwCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("entries"), entries}}).toJson();
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
        file.commit();
}

QVariantMap PCGamingWikiService::info(const QString &gameKey) const {
    return m_info.value(gameKey);
}

void PCGamingWikiService::clear(const QString &gameKey) {
    if (gameKey.isEmpty())
        return;
    if (m_info.remove(gameKey) > 0) {
        saveCache();
        ++m_revision;
        emit revisionChanged();
    }
}

void PCGamingWikiService::resolve(const QString &gameKey, const QString &gameName) {
    const QString key = gameKey.trimmed();
    const QString name = gameName.trimmed();
    if (key.isEmpty() || name.isEmpty()) {
        setStatus(QStringLiteral("No game is selected for PCGamingWiki lookup."));
        return;
    }
    if (m_pending.contains(key))
        return;
    const QVariantMap cached = m_info.value(key);
    if (cached.value(QStringLiteral("resolved")).toBool() &&
        normalizedTitle(cached.value(QStringLiteral("gameName")).toString()) == normalizedTitle(name)) {
        setStatus(QStringLiteral("Using cached PCGamingWiki page for %1.").arg(name));
        return;
    }
    if (cached.value(QStringLiteral("resolved")).toBool()) {
        m_info.remove(key);
        saveCache();
    }

    QUrl url(QStringLiteral("https://www.pcgamingwiki.com/w/api.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    query.addQueryItem(QStringLiteral("generator"), QStringLiteral("search"));
    query.addQueryItem(QStringLiteral("gsrsearch"), name);
    query.addQueryItem(QStringLiteral("gsrnamespace"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("gsrlimit"), QStringLiteral("5"));
    query.addQueryItem(QStringLiteral("prop"), QStringLiteral("info"));
    query.addQueryItem(QStringLiteral("inprop"), QStringLiteral("url"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Reno119/" RENO119_VERSION " (desktop application; PCGamingWiki lookup) QtNetwork"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    m_pending.insert(key);
    m_info.insert(key, {{QStringLiteral("queried"), true},
                        {QStringLiteral("pending"), true},
                        {QStringLiteral("resolved"), false},
                        {QStringLiteral("gameName"), name}});
    ++m_revision;
    emit revisionChanged();
    setStatus(QStringLiteral("Looking up %1 on PCGamingWiki...").arg(name));
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, name] {
        m_pending.remove(key);

        QVariantMap result;
        result[QStringLiteral("queried")] = true;
        result[QStringLiteral("pending")] = false;
        result[QStringLiteral("resolved")] = false;
        result[QStringLiteral("gameName")] = name;

        if (reply->error() != QNetworkReply::NoError) {
            result[QStringLiteral("error")] = reply->errorString();
            m_info.insert(key, result);
            ++m_revision;
            emit revisionChanged();
            setStatus(QStringLiteral("PCGamingWiki lookup failed: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray pages = doc.object().value(QStringLiteral("query")).toObject().value(QStringLiteral("pages")).toArray();
        const QString target = normalizedTitle(name);

        QJsonObject chosen;
        int chosenScore = -1;
        for (const QJsonValue &value : pages) {
            const QJsonObject page = value.toObject();
            const QString title = page.value(QStringLiteral("title")).toString();
            const QString normalized = normalizedTitle(title);
            int score = 0;
            if (!target.isEmpty() && normalized == target)
                score = 1000;
            else if (!target.isEmpty() && (normalized.startsWith(target) || target.startsWith(normalized)))
                score = 500 + qMin(normalized.size(), target.size());
            else if (!target.isEmpty() && (normalized.contains(target) || target.contains(normalized)))
                score = 200 + qMin(normalized.size(), target.size());
            if (score > chosenScore) {
                chosen = page;
                chosenScore = score;
            }
        }

        const QString title = chosen.value(QStringLiteral("title")).toString();
        const QString fullUrl = chosen.value(QStringLiteral("fullurl")).toString();
        if (!title.isEmpty() && !fullUrl.isEmpty()) {
            result[QStringLiteral("resolved")] = true;
            result[QStringLiteral("title")] = title;
            result[QStringLiteral("url")] = fullUrl;
            result[QStringLiteral("pageId")] = chosen.value(QStringLiteral("pageid")).toInt();
            result[QStringLiteral("checkedUtc")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            setStatus(QStringLiteral("Resolved PCGamingWiki page: %1").arg(title));
        } else {
            result[QStringLiteral("error")] = QStringLiteral("No matching PCGamingWiki page was found.");
            setStatus(QStringLiteral("No matching PCGamingWiki page was found for %1.").arg(name));
        }

        m_info.insert(key, result);
        if (result.value(QStringLiteral("resolved")).toBool())
            saveCache();
        ++m_revision;
        emit revisionChanged();
        reply->deleteLater();
    });
}
