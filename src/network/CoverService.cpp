#include "CoverService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>


namespace {
constexpr qint64 kNegativeArtworkCacheSeconds = 7LL * 24 * 60 * 60;
}

CoverService::CoverService(QObject *parent)
    : QObject(parent) {
    QDir().mkpath(cachePath());
    QDir().mkpath(bannerCachePath());
    loadNegativeCache();
}

void CoverService::shutdown() {
    const auto replies = m_network.findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
    m_pending.clear();
    m_bannerPending.clear();
}

void CoverService::setLastStatus(const QString &status) {
    if (m_lastStatus == status)
        return;
    m_lastStatus = status;
    emit lastStatusChanged();
}

void CoverService::loadNegativeCache() {
    QFile file(QDir(cacheRoot()).filePath(QStringLiteral("artwork-missing-v1.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto load = [&now](const QJsonObject &obj, QSet<QString> &missing, QHash<QString, QDateTime> &checked) {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QDateTime stamp = QDateTime::fromString(it.value().toString(), Qt::ISODate);
            const qint64 age = stamp.secsTo(now);
            if (!stamp.isValid() || age < 0 || age >= kNegativeArtworkCacheSeconds)
                continue;
            missing.insert(it.key());
            checked.insert(it.key(), stamp);
        }
    };
    load(root.value(QStringLiteral("covers")).toObject(), m_missing, m_missingCheckedUtc);
    load(root.value(QStringLiteral("banners")).toObject(), m_bannerMissing, m_bannerMissingCheckedUtc);
}

void CoverService::saveNegativeCache() const {
    QJsonObject covers;
    QJsonObject banners;
    for (auto it = m_missingCheckedUtc.constBegin(); it != m_missingCheckedUtc.constEnd(); ++it)
        covers.insert(it.key(), it.value().toString(Qt::ISODate));
    for (auto it = m_bannerMissingCheckedUtc.constBegin(); it != m_bannerMissingCheckedUtc.constEnd(); ++it)
        banners.insert(it.key(), it.value().toString(Qt::ISODate));
    const QString path = QDir(cacheRoot()).filePath(QStringLiteral("artwork-missing-v1.json"));
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("covers"), covers},
                                                      {QStringLiteral("banners"), banners}}).toJson();
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
        file.commit();
}

void CoverService::rememberMissing(const QString &appId, bool banner) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    (banner ? m_bannerMissing : m_missing).insert(appId);
    (banner ? m_bannerMissingCheckedUtc : m_missingCheckedUtc).insert(appId, now);
    saveNegativeCache();
}

void CoverService::forgetMissing(const QString &appId, bool banner) {
    (banner ? m_bannerMissing : m_missing).remove(appId);
    (banner ? m_bannerMissingCheckedUtc : m_missingCheckedUtc).remove(appId);
    saveNegativeCache();
}

QString CoverService::cacheRoot() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.cache/reno119");
    return base;
}

QString CoverService::cachePath() const {
    return QDir(cacheRoot()).filePath(QStringLiteral("covers"));
}

QString CoverService::bannerCachePath() const {
    return QDir(cacheRoot()).filePath(QStringLiteral("banners"));
}

QString CoverService::coverFilePath(const QString &appId) const {
    return QDir(cachePath()).filePath(appId + QStringLiteral(".jpg"));
}

QString CoverService::bannerFilePath(const QString &appId) const {
    return QDir(bannerCachePath()).filePath(appId + QStringLiteral(".jpg"));
}

QStringList CoverService::candidateUrls(const QString &appId) const {
    return {
        QStringLiteral("https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/library_600x900_2x.jpg").arg(appId),
        QStringLiteral("https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/library_600x900.jpg").arg(appId),
        QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg").arg(appId),
        QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900.jpg").arg(appId)
    };
}

QStringList CoverService::candidateBannerUrls(const QString &appId) const {
    return {
        QStringLiteral("https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/%1/library_hero.jpg").arg(appId),
        QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_hero.jpg").arg(appId),
        QStringLiteral("https://shared.fastly.steamstatic.com/steam/apps/%1/header.jpg").arg(appId),
        QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/header.jpg").arg(appId)
    };
}

QString CoverService::findLocalSteamCover(const QString &appId) const {
    const QString home = QDir::homePath();
    const QStringList cacheRoots = {
        home + QStringLiteral("/.local/share/Steam/appcache/librarycache"),
        home + QStringLiteral("/.steam/steam/appcache/librarycache"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam/appcache/librarycache")
    };

    QString best;
    qint64 bestSize = 0;
    auto consider = [&](const QString &path) {
        const QFileInfo info(path);
        if (!info.isFile() || info.size() <= 1024)
            return;
        if (info.size() > bestSize) {
            best = info.absoluteFilePath();
            bestSize = info.size();
        }
    };

    for (const QString &root : cacheRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        consider(dir.filePath(appId + QStringLiteral("_library_600x900_2x.jpg")));
        consider(dir.filePath(appId + QStringLiteral("_library_600x900.jpg")));
        consider(dir.filePath(appId + QStringLiteral("/library_600x900_2x.jpg")));
        consider(dir.filePath(appId + QStringLiteral("/library_600x900.jpg")));
        consider(dir.filePath(appId + QStringLiteral("/library_capsule.jpg")));

        const QString nested = dir.filePath(appId);
        if (QDir(nested).exists()) {
            QDirIterator it(nested,
                            {QStringLiteral("*library_600x900*.jpg"),
                             QStringLiteral("library_capsule.jpg"),
                             QStringLiteral("*library_600x900*.png")},
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext())
                consider(it.next());
        }
    }
    return best;
}

QString CoverService::findLocalSteamBanner(const QString &appId) const {
    const QString home = QDir::homePath();
    const QStringList cacheRoots = {
        home + QStringLiteral("/.local/share/Steam/appcache/librarycache"),
        home + QStringLiteral("/.steam/steam/appcache/librarycache"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam/appcache/librarycache")
    };

    QString best;
    int bestPriority = -1;
    qint64 bestSize = 0;
    auto consider = [&](const QString &path, int priority) {
        const QFileInfo info(path);
        if (!info.isFile() || info.size() <= 1024)
            return;
        if (info.fileName().contains(QStringLiteral("blur"), Qt::CaseInsensitive))
            return;
        if (priority > bestPriority || (priority == bestPriority && info.size() > bestSize)) {
            best = info.absoluteFilePath();
            bestPriority = priority;
            bestSize = info.size();
        }
    };

    for (const QString &root : cacheRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        // Prefer Steam's wide Library Hero art. Header art is a useful
        // fallback when a hero has not been cached for the title.
        consider(dir.filePath(appId + QStringLiteral("_library_hero.jpg")), 3);
        consider(dir.filePath(appId + QStringLiteral("_library_hero.png")), 3);
        consider(dir.filePath(appId + QStringLiteral("_header.jpg")), 2);
        consider(dir.filePath(appId + QStringLiteral("/library_hero.jpg")), 3);
        consider(dir.filePath(appId + QStringLiteral("/library_hero.png")), 3);
        consider(dir.filePath(appId + QStringLiteral("/header.jpg")), 2);

        const QString nested = dir.filePath(appId);
        if (QDir(nested).exists()) {
            QDirIterator it(nested,
                            {QStringLiteral("*library_hero*.jpg"),
                             QStringLiteral("*library_hero*.png"),
                             QStringLiteral("*header*.jpg"),
                             QStringLiteral("*header*.png")},
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                const QString fileName = QFileInfo(path).fileName();
                consider(path, fileName.contains(QStringLiteral("library_hero"), Qt::CaseInsensitive) ? 3 : 2);
            }
        }
    }
    return best;
}

static bool copyArtworkFile(const QString &source, const QString &destination) {
    QFile src(source);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = src.readAll();
    if (data.size() <= 1024)
        return false;

    QDir().mkpath(QFileInfo(destination).absolutePath());
    QSaveFile out(destination);
    out.setDirectWriteFallback(true);
    return out.open(QIODevice::WriteOnly) && out.write(data) == data.size() && out.commit();
}

bool CoverService::importLocalSteamCover(const QString &appId, const QString &destination) {
    const QString source = findLocalSteamCover(appId);
    if (source.isEmpty() || !copyArtworkFile(source, destination))
        return false;

    m_pending.remove(appId);
    forgetMissing(appId, false);
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Loaded Steam library cover for AppID %1 from the local Steam cache.").arg(appId));
    return true;
}

bool CoverService::importLocalSteamBanner(const QString &appId, const QString &destination) {
    const QString source = findLocalSteamBanner(appId);
    if (source.isEmpty() || !copyArtworkFile(source, destination))
        return false;

    m_bannerPending.remove(appId);
    forgetMissing(appId, true);
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Loaded Steam banner for AppID %1 from the local Steam cache.").arg(appId));
    return true;
}

QString CoverService::coverSource(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty() || cleanId.startsWith(QStringLiteral("custom:")))
        return {};

    const QString path = coverFilePath(cleanId);
    const QFileInfo info(path);
    if (info.exists() && info.size() > 1024)
        return QUrl::fromLocalFile(path).toString();

    if (importLocalSteamCover(cleanId, path))
        return QUrl::fromLocalFile(path).toString();

    if (m_missing.contains(cleanId))
        return {};

    if (!m_pending.contains(cleanId)) {
        m_pending.insert(cleanId);
        startRequest(cleanId, path, candidateUrls(cleanId), 0, false);
    }
    return {};
}

QString CoverService::bannerSource(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty() || cleanId.startsWith(QStringLiteral("custom:")))
        return {};

    const QString path = bannerFilePath(cleanId);
    const QFileInfo info(path);
    if (info.exists() && info.size() > 1024)
        return QUrl::fromLocalFile(path).toString();

    if (importLocalSteamBanner(cleanId, path))
        return QUrl::fromLocalFile(path).toString();

    if (m_bannerMissing.contains(cleanId))
        return {};

    if (!m_bannerPending.contains(cleanId)) {
        m_bannerPending.insert(cleanId);
        startRequest(cleanId, path, candidateBannerUrls(cleanId), 0, true);
    }
    return {};
}

void CoverService::startRequest(const QString &appId,
                                const QString &destination,
                                const QStringList &urls,
                                int urlIndex,
                                bool banner) {
    QSet<QString> &pending = banner ? m_bannerPending : m_pending;

    if (urlIndex < 0 || urlIndex >= urls.size()) {
        pending.remove(appId);
        rememberMissing(appId, banner);
        setLastStatus(banner
                          ? QStringLiteral("No Steam banner was found for AppID %1.").arg(appId)
                          : QStringLiteral("No Steam library cover was found for AppID %1.").arg(appId));
        return;
    }

    QNetworkRequest request{QUrl(urls.at(urlIndex))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Reno119/" RENO119_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, appId, destination, urls, urlIndex, banner] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const QByteArray contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().toLower();
        const bool imageLike = contentType.startsWith("image/") || data.startsWith("\xff\xd8") || data.startsWith("\x89PNG");
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && data.size() > 1024 && imageLike;
        reply->deleteLater();

        if (ok) {
            finishRequest(appId, destination, data, banner);
            return;
        }

        startRequest(appId, destination, urls, urlIndex + 1, banner);
    });
}

void CoverService::finishRequest(const QString &appId,
                                 const QString &destination,
                                 const QByteArray &data,
                                 bool banner) {
    QDir().mkpath(QFileInfo(destination).absolutePath());
    QSaveFile file(destination);
    file.setDirectWriteFallback(true);
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
        ++m_revision;
        emit revisionChanged();
        setLastStatus(banner
                          ? QStringLiteral("Refreshed Steam banner for AppID %1.").arg(appId)
                          : QStringLiteral("Refreshed Steam cover for AppID %1.").arg(appId));
    }
    (banner ? m_bannerPending : m_pending).remove(appId);
    forgetMissing(appId, banner);
}

void CoverService::clearCover(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty())
        return;
    QFile::remove(coverFilePath(cleanId));
    m_missing.insert(cleanId);
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Cleared cached cover for AppID %1.").arg(cleanId));
}

void CoverService::clearBanner(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty())
        return;
    QFile::remove(bannerFilePath(cleanId));
    m_bannerMissing.insert(cleanId);
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Cleared cached banner for AppID %1.").arg(cleanId));
}

void CoverService::clearArtwork(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty())
        return;
    QFile::remove(coverFilePath(cleanId));
    QFile::remove(bannerFilePath(cleanId));
    m_missing.insert(cleanId);
    m_bannerMissing.insert(cleanId);
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Cleared cached Steam artwork for AppID %1.").arg(cleanId));
}

void CoverService::refreshCover(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty() || cleanId.startsWith(QStringLiteral("custom:"))) {
        setLastStatus(QStringLiteral("Custom programs do not have Steam cover artwork to refresh."));
        return;
    }

    QFile::remove(coverFilePath(cleanId));
    forgetMissing(cleanId, false);
    m_pending.remove(cleanId);
    ++m_revision;
    emit revisionChanged();

    const QString destination = coverFilePath(cleanId);
    if (importLocalSteamCover(cleanId, destination))
        return;

    m_pending.insert(cleanId);
    setLastStatus(QStringLiteral("Refreshing Steam cover for AppID %1...").arg(cleanId));
    startRequest(cleanId, destination, candidateUrls(cleanId), 0, false);
}

void CoverService::refreshBanner(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty() || cleanId.startsWith(QStringLiteral("custom:"))) {
        setLastStatus(QStringLiteral("Custom programs do not have Steam banner artwork to refresh."));
        return;
    }

    QFile::remove(bannerFilePath(cleanId));
    forgetMissing(cleanId, true);
    m_bannerPending.remove(cleanId);
    ++m_revision;
    emit revisionChanged();

    const QString destination = bannerFilePath(cleanId);
    if (importLocalSteamBanner(cleanId, destination))
        return;

    m_bannerPending.insert(cleanId);
    setLastStatus(QStringLiteral("Refreshing Steam banner for AppID %1...").arg(cleanId));
    startRequest(cleanId, destination, candidateBannerUrls(cleanId), 0, true);
}

void CoverService::refreshArtwork(const QString &appId) {
    const QString cleanId = appId.trimmed();
    if (cleanId.isEmpty() || cleanId.startsWith(QStringLiteral("custom:"))) {
        setLastStatus(QStringLiteral("Custom programs do not have Steam artwork to refresh."));
        return;
    }
    refreshCover(cleanId);
    refreshBanner(cleanId);
}

void CoverService::refreshAll(const QStringList &appIds) {
    QStringList ids;
    for (const QString &raw : appIds) {
        const QString id = raw.trimmed();
        if (!id.isEmpty() && !id.startsWith(QStringLiteral("custom:")) && !ids.contains(id))
            ids << id;
    }
    if (ids.isEmpty()) {
        setLastStatus(QStringLiteral("No Steam games are available to refresh."));
        return;
    }

    setLastStatus(QStringLiteral("Refreshing Steam artwork for %1 games...").arg(ids.size()));
    for (int i = 0; i < ids.size(); ++i) {
        const QString id = ids.at(i);
        QTimer::singleShot(i * 175, this, [this, id] { refreshArtwork(id); });
    }
}

void CoverService::clearCache() {
    for (const QString &path : {cachePath(), bannerCachePath()}) {
        QDir dir(path);
        const QStringList files = dir.entryList({QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")}, QDir::Files);
        for (const QString &name : files)
            dir.remove(name);
    }
    m_missing.clear();
    m_bannerMissing.clear();
    m_missingCheckedUtc.clear();
    m_bannerMissingCheckedUtc.clear();
    QFile::remove(QDir(cacheRoot()).filePath(QStringLiteral("artwork-missing-v1.json")));
    ++m_revision;
    emit revisionChanged();
    setLastStatus(QStringLiteral("Cleared Reno119's Steam artwork cache."));
}
