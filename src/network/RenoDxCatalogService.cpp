#include "RenoDxCatalogService.h"

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
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr auto kRenoDxWikiUrl = "https://raw.githubusercontent.com/wiki/clshortfuse/renodx/Mods.md";
constexpr auto kRhiManifestUrl = "https://raw.githubusercontent.com/RankFTW/RHI/main/manifest.json";
constexpr auto kRhiEngineFilesBase = "https://raw.githubusercontent.com/RankFTW/RHI/main/engine-files/";
constexpr auto kUserAgent = "Reno119/" RENO119_VERSION;
constexpr qint64 kRhiCacheSeconds = 24LL * 60 * 60;
constexpr qint64 kRenoDxRevisionCacheSeconds = 6LL * 60 * 60;

QString renoDxRevisionCachePath() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/renodx-revisions-v1.json");
}

QString rhiCachePath() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/rhi-metadata-v1.json");
}

QString sanitizeRhiGameNote(QString note) {
    // RHI notes are authored for RHI's own table layout. Reno119 surfaces the
    // useful game-specific guidance, but table-navigation instructions such
    // as "install REFramework from the row above" do not map to our UI.
    note.remove(QRegularExpression(
        QStringLiteral(R"((?:\r?\n\s*)*(?:please\s+)?install\s+(?:it|re\s*framework)\s+from\s+(?:the\s+)?(?:re\s*framework\s+)?row\s+above[.!]?\s*)"),
        QRegularExpression::CaseInsensitiveOption));
    note.replace(QRegularExpression(QStringLiteral(R"([ \t]+\n)")), QStringLiteral("\n"));
    note.replace(QRegularExpression(QStringLiteral(R"(\n{3,})")), QStringLiteral("\n\n"));
    return note.trimmed();
}
}

RenoDxCatalogService::RenoDxCatalogService(QObject *parent) : QObject(parent) {
    QFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/renodx-catalog-v1.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const auto cached = QJsonDocument::fromJson(file.readAll()).object();
        parseRenoDxWiki(cached.value(QStringLiteral("markdown")).toString());
        m_renoDxRecommendedReShade = cached.value(QStringLiteral("recommended")).toString();
        m_catalogFailed = cached.value(QStringLiteral("refreshFailed")).toBool();
        m_catalogChecked = QDateTime::fromString(cached.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
        m_renoDxCatalogReady = !m_renoDxEntries.isEmpty();
        m_renoDxCatalogFinished = m_renoDxCatalogReady;
    }
    loadRenoDxRevisionCache();
    loadRhiCache();
    refreshCatalog();
}

void RenoDxCatalogService::shutdown() {
    const auto replies = m_net.findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
    m_catalogRefreshing = false;
    m_rhiRefreshing = false;
}

bool RenoDxCatalogService::rhiCacheFresh() const {
    const qint64 age = m_rhiChecked.secsTo(QDateTime::currentDateTimeUtc());
    return m_rhiManifestReady && m_rhiChecked.isValid() && age >= 0 && age < kRhiCacheSeconds;
}

void RenoDxCatalogService::loadRhiCache() {
    QFile file(rhiCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_rhiManifestJson = root.value(QStringLiteral("manifestJson")).toString().toUtf8();
    m_rhiChecked = QDateTime::fromString(root.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
    if (m_rhiManifestJson.isEmpty())
        return;

    parseRhiManifest(m_rhiManifestJson, false);
    const QJsonObject profiles = root.value(QStringLiteral("profiles")).toObject();
    for (auto it = profiles.begin(); it != profiles.end(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject obj = it.value().toObject();
        const QString fileName = obj.value(QStringLiteral("file")).toString();
        const QString text = obj.value(QStringLiteral("text")).toString();
        const QDateTime checked = QDateTime::fromString(obj.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
        const qint64 age = checked.secsTo(QDateTime::currentDateTimeUtc());
        if (!text.isEmpty() && !fileName.isEmpty() && m_rhiEngineIniFiles.value(it.key()) == fileName &&
            checked.isValid() && age >= 0 && age < kRhiCacheSeconds) {
            m_rhiEngineIniProfiles.insert(it.key(), text);
            m_rhiEngineIniProfileChecked.insert(it.key(), checked);
        }
    }
    m_rhiManifestFinished = m_rhiManifestReady;
}

void RenoDxCatalogService::saveRhiCache() const {
    if (m_rhiManifestJson.isEmpty())
        return;
    QJsonObject profiles;
    for (auto it = m_rhiEngineIniProfiles.constBegin(); it != m_rhiEngineIniProfiles.constEnd(); ++it) {
        const QString fileName = m_rhiEngineIniFiles.value(it.key());
        if (fileName.isEmpty() || it.value().isEmpty())
            continue;
        profiles.insert(it.key(), QJsonObject{{QStringLiteral("file"), fileName},
                                              {QStringLiteral("text"), it.value()},
                                              {QStringLiteral("checkedUtc"), m_rhiEngineIniProfileChecked.value(it.key()).toString(Qt::ISODate)}});
    }
    QJsonObject root;
    root.insert(QStringLiteral("checkedUtc"), m_rhiChecked.toString(Qt::ISODate));
    root.insert(QStringLiteral("manifestJson"), QString::fromUtf8(m_rhiManifestJson));
    root.insert(QStringLiteral("profiles"), profiles);
    const QString path = rhiCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(root).toJson();
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
        file.commit();
}

void RenoDxCatalogService::refreshRhiManifest(bool forceRefresh) {
    if (m_rhiRefreshing || (!forceRefresh && rhiCacheFresh()))
        return;
    m_rhiRefreshing = true;
    m_rhiManifestFinished = false;
    emit catalogChanged();

    QNetworkRequest request(QUrl(QString::fromLatin1(kRhiManifestUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setTransferTimeout(15000);
    auto *reply = m_net.get(request);
    QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray json = reply->readAll();
            QJsonParseError error{};
            const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                m_rhiManifestJson = json;
                m_rhiChecked = QDateTime::currentDateTimeUtc();
                parseRhiManifest(json, true);
                saveRhiCache();
            }
        }
        m_rhiRefreshing = false;
        m_rhiManifestFinished = true;
        emit catalogChanged();
        reply->deleteLater();
    });
}

void RenoDxCatalogService::refreshCatalog(bool forceRefresh) {
    refreshRhiManifest(forceRefresh);
    if (m_catalogRefreshing || (!forceRefresh && m_renoDxCatalogReady && !catalogStale())) return;
    m_catalogRefreshing = true;
    emit catalogChanged();
    QNetworkRequest request{QUrl(QString::fromLatin1(kRenoDxWikiUrl))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setTransferTimeout(15000);
    auto *reply = m_net.get(request);
    QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        bool accepted = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QString markdown = QString::fromUtf8(reply->readAll());
            const auto previous = m_renoDxEntries;
            parseRenoDxWiki(markdown);
            if (!m_renoDxEntries.isEmpty()) {
                accepted = true;
                const QRegularExpression rx(R"(Install\s+\[?ReShade\s+([0-9]+(?:\.[0-9]+){1,3})\+?\s+with\s+full\s+add-on\s+support)", QRegularExpression::CaseInsensitiveOption);
                const auto match = rx.match(markdown);
                m_renoDxRecommendedReShade = match.hasMatch() ? match.captured(1) : QString();
                m_catalogChecked = QDateTime::currentDateTimeUtc();
                const QJsonObject cached{{QStringLiteral("markdown"), markdown},
                    {QStringLiteral("recommended"), m_renoDxRecommendedReShade},
                    {QStringLiteral("checkedUtc"), catalogCheckedUtc()}};
                const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
                QDir().mkpath(dir);
                QSaveFile file(dir + QStringLiteral("/renodx-catalog-v1.json"));
                const auto data = QJsonDocument(cached).toJson();
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) file.commit();
            } else m_renoDxEntries = previous;
        }
        if (!accepted) {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/renodx-catalog-v1.json");
            QFile old(path);
            if (old.open(QIODevice::ReadOnly)) {
                auto cached = QJsonDocument::fromJson(old.readAll()).object();
                old.close();
                cached.insert(QStringLiteral("refreshFailed"), true);
                QSaveFile file(path);
                const auto data = QJsonDocument(cached).toJson();
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) file.commit();
            }
        }
        m_catalogFailed = !accepted;
        m_renoDxCatalogReady = !m_renoDxEntries.isEmpty();
        m_renoDxCatalogFinished = true;
        m_catalogRefreshing = false;
        emit catalogChanged();
        reply->deleteLater();
    });
}

void RenoDxCatalogService::parseRenoDxWiki(const QString &markdown) {
    m_renoDxEntries.clear();

    static const QRegularExpression addonUrl(
        R"(\]\((https?://[^\)\s]+\.addon(?:64|32)(?:\?[^\)\s]*)?)\))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sourceBadge(
        R"(https://img\.shields\.io/github/last-commit/[^\)\s]+)",
        QRegularExpression::CaseInsensitiveOption);

    QSet<QString> seenKeys;
    const QStringList lines = markdown.split('\n');
    for (const QString &line : lines) {
        if (!line.startsWith('|') || !line.contains(QStringLiteral(".addon"), Qt::CaseInsensitive))
            continue;

        const QStringList columns = line.split('|');
        if (columns.size() < 4)
            continue;

        const QString rawTitle = columns.at(1).trimmed();
        const QString key = RenoDxTitleMatcher::normalizeGameName(rawTitle);
        if (key.isEmpty() || seenKeys.contains(key))
            continue;

        const auto match = addonUrl.match(line);
        if (!match.hasMatch())
            continue;

        QString sourceOwner;
        QString sourceRepo;
        QString sourcePath;
        const auto badgeMatch = sourceBadge.match(line);
        if (badgeMatch.hasMatch()) {
            const QUrl badgeUrl(badgeMatch.captured(0));
            const QStringList segments = badgeUrl.path().split('/', Qt::SkipEmptyParts);
            if (segments.size() >= 4 &&
                segments.at(0).compare(QStringLiteral("github"), Qt::CaseInsensitive) == 0 &&
                segments.at(1).compare(QStringLiteral("last-commit"), Qt::CaseInsensitive) == 0) {
                sourceOwner = segments.at(2);
                sourceRepo = segments.at(3);
                QUrlQuery query(badgeUrl);
                sourcePath = query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded).trimmed();
            }
        }

        seenKeys.insert(key);
        m_renoDxEntries.push_back({RenoDxTitleMatcher::cleanDisplayTitle(rawTitle), match.captured(1),
                                   sourceOwner, sourceRepo, sourcePath});
    }
}

void RenoDxCatalogService::parseRhiManifest(const QByteArray &json, bool fetchMissingProfiles) {
    const auto previousFiles = m_rhiEngineIniFiles;
    const auto previousProfiles = m_rhiEngineIniProfiles;
    const auto previousProfileChecked = m_rhiEngineIniProfileChecked;

    m_rhiRenoDxIniOverrides.clear();
    m_rhiUeCompatibility.clear();
    m_rhiEngineIniFiles.clear();
    m_rhiEngineIniProfiles.clear();
    m_rhiEngineIniProfileChecked.clear();
    m_rhiEngineIniPathOverrides.clear();
    m_rhiGameNotes.clear();
    m_rhiNativeHdrGames.clear();

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return;

    m_rhiManifestReady = true;
    const QJsonObject root = doc.object();

    const QJsonArray nativeHdr = root.value(QStringLiteral("nativeHdrGames")).toArray();
    for (const QJsonValue &value : nativeHdr) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(value.toString());
        if (!key.isEmpty())
            m_rhiNativeHdrGames.insert(key);
    }

    const QJsonObject pathOverrides = root.value(QStringLiteral("engineIniPathOverrides")).toObject();
    for (auto it = pathOverrides.begin(); it != pathOverrides.end(); ++it) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(it.key());
        const QString value = it.value().toString().trimmed();
        if (!key.isEmpty() && !value.isEmpty())
            m_rhiEngineIniPathOverrides.insert(key, value);
    }

    const QJsonObject compatibility = root.value(QStringLiteral("ueExtendedCompatibility")).toObject();
    for (auto it = compatibility.begin(); it != compatibility.end(); ++it) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(it.key());
        if (!key.isEmpty() && it.value().isObject())
            m_rhiUeCompatibility.insert(key, it.value().toObject().toVariantMap());
    }

    const QJsonObject iniOverrides = root.value(QStringLiteral("renodxIniOverrides")).toObject();
    for (auto it = iniOverrides.begin(); it != iniOverrides.end(); ++it) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(it.key());
        if (!key.isEmpty() && it.value().isObject())
            m_rhiRenoDxIniOverrides.insert(key, it.value().toObject().toVariantMap());
    }

    const QJsonObject engineFiles = root.value(QStringLiteral("engineIniFiles")).toObject();
    for (auto it = engineFiles.begin(); it != engineFiles.end(); ++it) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(it.key());
        const QString fileName = it.value().toString().trimmed();
        if (key.isEmpty() || fileName.isEmpty())
            continue;
        m_rhiEngineIniFiles.insert(key, fileName);
        const QDateTime profileChecked = previousProfileChecked.value(key);
        const qint64 profileAge = profileChecked.secsTo(QDateTime::currentDateTimeUtc());
        if (previousFiles.value(key) == fileName && !previousProfiles.value(key).isEmpty() &&
            profileChecked.isValid() && profileAge >= 0 && profileAge < kRhiCacheSeconds) {
            m_rhiEngineIniProfiles.insert(key, previousProfiles.value(key));
            m_rhiEngineIniProfileChecked.insert(key, profileChecked);
        }
    }

    const QJsonObject gameNotes = root.value(QStringLiteral("gameNotes")).toObject();
    for (auto it = gameNotes.begin(); it != gameNotes.end(); ++it) {
        const QString key = RenoDxTitleMatcher::normalizeGameName(it.key());
        if (key.isEmpty() || !it.value().isObject())
            continue;
        const QString note = sanitizeRhiGameNote(
            it.value().toObject().value(QStringLiteral("notes")).toString());
        if (!note.isEmpty())
            m_rhiGameNotes.insert(key, note);
    }

    if (fetchMissingProfiles) {
        for (auto it = m_rhiEngineIniFiles.constBegin(); it != m_rhiEngineIniFiles.constEnd(); ++it) {
            if (!m_rhiEngineIniProfiles.contains(it.key()))
                fetchRhiEngineIniProfile(it.key(), it.value());
        }
    }
}

void RenoDxCatalogService::fetchRhiEngineIniProfile(const QString &normalizedGameKey, const QString &fileName) {
    const QUrl url(QString::fromLatin1(kRhiEngineFilesBase) + QString::fromUtf8(QUrl::toPercentEncoding(fileName, "/")));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setTransferTimeout(15000);
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedGameKey] {
        if (reply->error() == QNetworkReply::NoError) {
            const QString text = QString::fromUtf8(reply->readAll()).trimmed();
            if (!text.isEmpty()) {
                m_rhiEngineIniProfiles.insert(normalizedGameKey, text);
                m_rhiEngineIniProfileChecked.insert(normalizedGameKey, QDateTime::currentDateTimeUtc());
                saveRhiCache();
            }
        }
        emit catalogChanged();
        reply->deleteLater();
    });
}

void RenoDxCatalogService::loadRenoDxRevisionCache() {
    QFile file(renoDxRevisionCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject())
        m_renoDxRevisionCache = doc.object();
}

void RenoDxCatalogService::saveRenoDxRevisionCache() const {
    const QString path = renoDxRevisionCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(m_renoDxRevisionCache).toJson();
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
        file.commit();
}

bool RenoDxCatalogService::renoDxRevisionCacheFresh(const QString &sourceKey) const {
    if (sourceKey.isEmpty())
        return false;
    const QJsonObject entry = m_renoDxRevisionCache.value(sourceKey).toObject();
    const QString revision = entry.value(QStringLiteral("revision")).toString();
    const QDateTime checked = QDateTime::fromString(entry.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
    const qint64 age = checked.secsTo(QDateTime::currentDateTimeUtc());
    return !revision.isEmpty() && checked.isValid() && age >= 0 && age < kRenoDxRevisionCacheSeconds &&
           !entry.value(QStringLiteral("refreshFailed")).toBool();
}

QString RenoDxCatalogService::renoDxSourceRevision(const QString &gameName) const {
    const auto match = RenoDxTitleMatcher::resolve(gameName, m_renoDxEntries);
    const QString sourceKey = match.sourceKey();
    if (!match.matched() || sourceKey.isEmpty() || !renoDxRevisionCacheFresh(sourceKey))
        return {};
    return m_renoDxRevisionCache.value(sourceKey).toObject().value(QStringLiteral("revision")).toString();
}

void RenoDxCatalogService::refreshRenoDxSourceRevision(const QString &gameName, bool forceRefresh,
                                                       std::function<void()> done) {
    const auto match = RenoDxTitleMatcher::resolve(gameName, m_renoDxEntries);
    const QString sourceKey = match.sourceKey();
    if (!match.matched() || sourceKey.isEmpty() || match.sourceOwner.isEmpty() || match.sourceRepo.isEmpty()) {
        if (done)
            done();
        return;
    }
    if (!forceRefresh && renoDxRevisionCacheFresh(sourceKey)) {
        if (done)
            done();
        return;
    }

    const QString owner = QString::fromUtf8(QUrl::toPercentEncoding(match.sourceOwner));
    const QString repo = QString::fromUtf8(QUrl::toPercentEncoding(match.sourceRepo));
    QUrl api(QStringLiteral("https://api.github.com/repos/%1/%2/commits").arg(owner, repo));
    QUrlQuery query;
    if (!match.sourcePath.isEmpty())
        query.addQueryItem(QStringLiteral("path"), match.sourcePath);
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("1"));
    api.setQuery(query);

    QNetworkRequest request(api);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(15000);
    auto *reply = m_net.get(request);
    QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, sourceKey, match, done = std::move(done)]() mutable {
        QJsonObject entry = m_renoDxRevisionCache.value(sourceKey).toObject();
        bool accepted = false;
        if (reply->error() == QNetworkReply::NoError) {
            QJsonParseError error{};
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isArray() && !doc.array().isEmpty()) {
                const QString revision = doc.array().first().toObject().value(QStringLiteral("sha")).toString().trimmed();
                if (!revision.isEmpty()) {
                    entry.insert(QStringLiteral("revision"), revision);
                    entry.insert(QStringLiteral("checkedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                    entry.insert(QStringLiteral("refreshFailed"), false);
                    entry.insert(QStringLiteral("owner"), match.sourceOwner);
                    entry.insert(QStringLiteral("repo"), match.sourceRepo);
                    entry.insert(QStringLiteral("path"), match.sourcePath);
                    accepted = true;
                }
            }
        }
        if (!accepted)
            entry.insert(QStringLiteral("refreshFailed"), true);
        m_renoDxRevisionCache.insert(sourceKey, entry);
        saveRenoDxRevisionCache();
        emit catalogChanged();
        reply->deleteLater();
        if (done)
            done();
    });
}

QVariantMap RenoDxCatalogService::renoDxSnapshotInfo(const QString &gameName) const {
    const auto match = RenoDxTitleMatcher::resolve(gameName, m_renoDxEntries);
    QVariantMap out;
    out.insert(QStringLiteral("url"), match.url);
    out.insert(QStringLiteral("title"), match.title);
    out.insert(QStringLiteral("method"), RenoDxTitleMatcher::methodDisplayName(match.method));
    out.insert(QStringLiteral("exact"), match.exact());
    out.insert(QStringLiteral("requiresConfirmation"), match.requiresConfirmation());
    out.insert(QStringLiteral("sourceOwner"), match.sourceOwner);
    out.insert(QStringLiteral("sourceRepo"), match.sourceRepo);
    out.insert(QStringLiteral("sourcePath"), match.sourcePath);
    out.insert(QStringLiteral("sourceKey"), match.sourceKey());
    out.insert(QStringLiteral("sourceRevision"), renoDxSourceRevision(gameName));
    return out;
}

QString RenoDxCatalogService::renoDxSnapshot(const QString &gameName) const {
    return renoDxSnapshotInfo(gameName).value(QStringLiteral("url")).toString();
}

QString RenoDxCatalogService::recommendedReShadeVersion(const QString &) const {
    return m_renoDxRecommendedReShade;
}

QVariantMap RenoDxCatalogService::rhiRenoDxIniOverrides(const QString &gameName) const {
    return m_rhiRenoDxIniOverrides.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

QVariantMap RenoDxCatalogService::rhiUeExtendedCompatibility(const QString &gameName) const {
    return m_rhiUeCompatibility.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

QString RenoDxCatalogService::rhiEngineIniProfileText(const QString &gameName) const {
    return m_rhiEngineIniProfiles.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

QString RenoDxCatalogService::rhiEngineIniProfileFile(const QString &gameName) const {
    return m_rhiEngineIniFiles.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

QString RenoDxCatalogService::rhiEngineIniPathOverride(const QString &gameName) const {
    return m_rhiEngineIniPathOverrides.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

QString RenoDxCatalogService::rhiGameNote(const QString &gameName) const {
    return m_rhiGameNotes.value(RenoDxTitleMatcher::normalizeGameName(gameName));
}

bool RenoDxCatalogService::rhiNativeHdrGame(const QString &gameName) const {
    return m_rhiNativeHdrGames.contains(RenoDxTitleMatcher::normalizeGameName(gameName));
}
