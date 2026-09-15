#include "GameModel.h"
#include "ModDetectionService.h"
#include "../graphics/PeParser.h"
#include "../steam/SteamScanner.h"
#include "../library/HeroicScanner.h"
#include "../library/LutrisScanner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUuid>
#include <QUrl>
#include <QtConcurrent>
#include <algorithm>
#include <utility>

namespace {
struct UpdateFlags {
    bool checked = false;
    bool reshade = false;
    bool renodx = false;
    bool reframework = false;
};

QString detectReShadeVersionFromLog(const QString &exeDir) {
    QFile f(exeDir + QStringLiteral("/ReShade.log"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString text = QString::fromUtf8(f.read(131072));
    const QRegularExpression rx(QStringLiteral(R"(ReShade[^0-9]*([0-9]+(?:\.[0-9]+){1,3}))"),
                                QRegularExpression::CaseInsensitiveOption);
    const auto match = rx.match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString displayNameFor(const GameInfo &game) {
    const QString nickname = game.nickname.trimmed();
    return nickname.isEmpty() ? game.name : nickname;
}

QString normalizedGameKey(const QString &value) {
    const QString normalized = value.normalized(QString::NormalizationForm_KD).toLower();
    QString out;
    out.reserve(normalized.size());
    for (const QChar ch : normalized) {
        if (ch.isLetterOrNumber())
            out.append(ch);
    }
    return out;
}

QVariantMap basicDiagnosticFor(const GameInfo &g) {
    QVariantMap result;
    QString level = QStringLiteral("ok");
    QString summary = QStringLiteral("Ready");

    const bool exeExists = !g.exePath.trimmed().isEmpty() && QFileInfo(g.exePath).isFile();
    // Scanner and executable-override paths are PE-validated before assignment.
    // Keep row painting cheap; the on-demand diagnostic view performs a fresh PE check.
    const bool exeValid = exeExists;
    const bool prefixValid = !g.protonPrefix.trimmed().isEmpty() &&
                             QDir(QDir(g.protonPrefix).filePath(QStringLiteral("drive_c"))).exists();

    if (!exeExists) {
        level = QStringLiteral("error");
        summary = QStringLiteral("Executable unresolved");
    } else if (!exeValid) {
        level = QStringLiteral("error");
        summary = QStringLiteral("Executable is not a valid Windows PE");
    } else if (!prefixValid) {
        level = QStringLiteral("warning");
        summary = QStringLiteral("Wine/Proton prefix unresolved");
    } else if (g.integrityWarning) {
        level = QStringLiteral("warning");
        summary = QStringLiteral("Managed-file integrity needs attention");
    }

    result.insert(QStringLiteral("level"), level);
    result.insert(QStringLiteral("summary"), summary);
    return result;
}

QVariantMap healthInfoForGame(const GameInfo &g) {
    const QVariantMap diagnostic = basicDiagnosticFor(g);
    const QString diagLevel = diagnostic.value(QStringLiteral("level")).toString();
    if (diagLevel == QStringLiteral("error"))
        return {{QStringLiteral("level"), QStringLiteral("error")},
                {QStringLiteral("summary"), QStringLiteral("Needs setup")},
                {QStringLiteral("target"), QStringLiteral("diagnostics")}};
    if (diagLevel == QStringLiteral("warning") || g.integrityWarning)
        return {{QStringLiteral("level"), QStringLiteral("warning")},
                {QStringLiteral("summary"), QStringLiteral("Needs attention")},
                {QStringLiteral("target"), QStringLiteral("diagnostics")}};
    if (g.updateChecked && (g.reshadeUpdateAvailable || g.renodxUpdateAvailable || g.reframeworkUpdateAvailable))
        return {{QStringLiteral("level"), QStringLiteral("warning")},
                {QStringLiteral("summary"), QStringLiteral("Update available")},
                {QStringLiteral("target"), QStringLiteral("updates")}};
    if (g.reshadeExternal || g.reshade64External || g.renodxExternal || g.reframeworkExternal)
        return {{QStringLiteral("level"), QStringLiteral("info")},
                {QStringLiteral("summary"), QStringLiteral("External files detected")},
                {QStringLiteral("target"), QStringLiteral("components")}};
    return {{QStringLiteral("level"), QStringLiteral("ok")},
            {QStringLiteral("summary"), QStringLiteral("Ready")},
            {QStringLiteral("target"), QStringLiteral("none")}};
}

int overrideCountForGame(const GameInfo &g) {
    int count = 0;
    if (g.exeOverridden) ++count;
    if (g.prefixOverridden) ++count;
    if (g.graphicsApiOverridden) ++count;
    if (!g.nickname.trimmed().isEmpty()) ++count;
    if (g.artworkOverridden) ++count;
    return count;
}

void appendDetectionChanges(GameInfo &g, const QString &oldExe, const QString &oldPrefix, const QString &oldApi) {
    QStringList changes;
    const auto changed = [](const QString &oldValue, const QString &newValue) {
        return !oldValue.trimmed().isEmpty() && !newValue.trimmed().isEmpty() && oldValue != newValue;
    };
    if (changed(oldExe, g.detectedExePath))
        changes << QStringLiteral("Executable: %1 -> %2").arg(oldExe, g.detectedExePath);
    if (changed(oldPrefix, g.detectedProtonPrefix))
        changes << QStringLiteral("Prefix: %1 -> %2").arg(oldPrefix, g.detectedProtonPrefix);
    if (changed(oldApi, g.detectedGraphicsApi))
        changes << QStringLiteral("Graphics API: %1 -> %2").arg(oldApi, g.detectedGraphicsApi);
    if (changes.isEmpty())
        return;
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    for (const QString &change : changes)
        g.detectionHistory.prepend(stamp + QStringLiteral(" · ") + change);
    while (g.detectionHistory.size() > 12)
        g.detectionHistory.removeLast();
}

QJsonObject gameToCacheJson(const GameInfo &g) {
    QJsonObject o;
    const auto put = [&](const char *key, const QString &value) { o.insert(QString::fromLatin1(key), value); };
    const auto putb = [&](const char *key, bool value) { o.insert(QString::fromLatin1(key), value); };
    const auto puti = [&](const char *key, int value) { o.insert(QString::fromLatin1(key), value); };

    put("name", g.name); put("nickname", g.nickname); put("appId", g.appId); put("source", g.source);
    put("installPath", g.installPath); put("steamLibrary", g.steamLibrary); put("protonPrefix", g.protonPrefix);
    put("detectedProtonPrefix", g.detectedProtonPrefix);
    put("exePath", g.exePath); put("detectedExePath", g.detectedExePath); put("graphicsApi", g.graphicsApi);
    put("detectedGraphicsApi", g.detectedGraphicsApi);
    put("architecture", g.architecture); put("engine", g.engine); put("coverArtPath", g.coverArtPath);
    put("bannerArtPath", g.bannerArtPath); put("originalCoverArtPath", g.originalCoverArtPath);
    put("originalBannerArtPath", g.originalBannerArtPath); put("alternateGroupId", g.alternateGroupId);
    put("reshadeVersion", g.reshadeVersion); put("reshadeChannel", g.reshadeChannel); put("reshadeProxy", g.reshadeProxy);
    put("reshade64Version", g.reshade64Version); put("reshade64Channel", g.reshade64Channel);
    put("renodxFile", g.renodxFile); put("reframeworkVersion", g.reframeworkVersion);
    put("integritySummary", g.integritySummary);

    putb("artworkOverridden", g.artworkOverridden); putb("customProgram", g.customProgram); putb("favorite", g.favorite);
    putb("hidden", g.hidden); putb("prefixOverridden", g.prefixOverridden); putb("exeOverridden", g.exeOverridden);
    putb("graphicsApiOverridden", g.graphicsApiOverridden); putb("reshadeInstalled", g.reshadeInstalled);
    putb("reshadeManaged", g.reshadeManaged); putb("reshadeExternal", g.reshadeExternal);
    putb("reshade64Installed", g.reshade64Installed); putb("reshade64Managed", g.reshade64Managed);
    putb("reshade64External", g.reshade64External); putb("renodxInstalled", g.renodxInstalled);
    putb("renodxManaged", g.renodxManaged); putb("renodxExternal", g.renodxExternal);
    putb("updateChecked", g.updateChecked); putb("reshadeUpdateAvailable", g.reshadeUpdateAvailable);
    putb("renodxUpdateAvailable", g.renodxUpdateAvailable); putb("reframeworkUpdateAvailable", g.reframeworkUpdateAvailable);
    putb("reframeworkSupported", g.reframeworkSupported); putb("reframeworkInstalled", g.reframeworkInstalled);
    putb("reframeworkManaged", g.reframeworkManaged); putb("reframeworkExternal", g.reframeworkExternal);
    putb("optiScalerInstalled", g.optiScalerInstalled); putb("integrityWarning", g.integrityWarning);
    puti("duplicateCandidateCount", g.duplicateCandidateCount); puti("linkedAlternateCount", g.linkedAlternateCount);

    o.insert(QStringLiteral("importDiagnostics"), QJsonArray::fromStringList(g.importDiagnostics));
    o.insert(QStringLiteral("detectionHistory"), QJsonArray::fromStringList(g.detectionHistory));
    o.insert(QStringLiteral("sourceMetadata"), QJsonObject::fromVariantMap(g.sourceMetadata));
    return o;
}

GameInfo gameFromCacheJson(const QJsonObject &o) {
    GameInfo g;
    const auto str = [&](const char *key) { return o.value(QString::fromLatin1(key)).toString(); };
    const auto boolean = [&](const char *key) { return o.value(QString::fromLatin1(key)).toBool(); };
    const auto integer = [&](const char *key) { return o.value(QString::fromLatin1(key)).toInt(); };

    g.name=str("name"); g.nickname=str("nickname"); g.appId=str("appId"); g.source=str("source");
    g.installPath=str("installPath"); g.steamLibrary=str("steamLibrary"); g.protonPrefix=str("protonPrefix");
    g.detectedProtonPrefix=str("detectedProtonPrefix");
    g.exePath=str("exePath"); g.detectedExePath=str("detectedExePath"); g.graphicsApi=str("graphicsApi");
    g.detectedGraphicsApi=str("detectedGraphicsApi");
    g.architecture=str("architecture"); g.engine=str("engine"); g.coverArtPath=str("coverArtPath");
    g.bannerArtPath=str("bannerArtPath"); g.originalCoverArtPath=str("originalCoverArtPath");
    g.originalBannerArtPath=str("originalBannerArtPath"); g.alternateGroupId=str("alternateGroupId");
    g.reshadeVersion=str("reshadeVersion"); g.reshadeChannel=str("reshadeChannel"); g.reshadeProxy=str("reshadeProxy");
    g.reshade64Version=str("reshade64Version"); g.reshade64Channel=str("reshade64Channel");
    g.renodxFile=str("renodxFile"); g.reframeworkVersion=str("reframeworkVersion");
    g.integritySummary=str("integritySummary");

    g.artworkOverridden=boolean("artworkOverridden"); g.customProgram=boolean("customProgram");
    g.favorite=boolean("favorite"); g.hidden=boolean("hidden"); g.prefixOverridden=boolean("prefixOverridden");
    g.exeOverridden=boolean("exeOverridden"); g.graphicsApiOverridden=boolean("graphicsApiOverridden");
    g.reshadeInstalled=boolean("reshadeInstalled"); g.reshadeManaged=boolean("reshadeManaged");
    g.reshadeExternal=boolean("reshadeExternal"); g.reshade64Installed=boolean("reshade64Installed");
    g.reshade64Managed=boolean("reshade64Managed"); g.reshade64External=boolean("reshade64External");
    g.renodxInstalled=boolean("renodxInstalled"); g.renodxManaged=boolean("renodxManaged");
    g.renodxExternal=boolean("renodxExternal"); g.updateChecked=boolean("updateChecked");
    g.reshadeUpdateAvailable=boolean("reshadeUpdateAvailable"); g.renodxUpdateAvailable=boolean("renodxUpdateAvailable");
    g.reframeworkUpdateAvailable=boolean("reframeworkUpdateAvailable"); g.reframeworkSupported=boolean("reframeworkSupported");
    g.reframeworkInstalled=boolean("reframeworkInstalled"); g.reframeworkManaged=boolean("reframeworkManaged");
    g.reframeworkExternal=boolean("reframeworkExternal"); g.optiScalerInstalled=boolean("optiScalerInstalled");
    g.integrityWarning=boolean("integrityWarning");
    g.duplicateCandidateCount=integer("duplicateCandidateCount"); g.linkedAlternateCount=integer("linkedAlternateCount");

    for (const QJsonValue &v : o.value(QStringLiteral("importDiagnostics")).toArray())
        if (v.isString()) g.importDiagnostics << v.toString();
    for (const QJsonValue &v : o.value(QStringLiteral("detectionHistory")).toArray())
        if (v.isString()) g.detectionHistory << v.toString();
    g.sourceMetadata = o.value(QStringLiteral("sourceMetadata")).toObject().toVariantMap();
    if (g.source.isEmpty()) g.source = QStringLiteral("Steam");
    if (g.detectedProtonPrefix.isEmpty()) g.detectedProtonPrefix = g.protonPrefix;
    if (g.detectedGraphicsApi.isEmpty()) g.detectedGraphicsApi = g.graphicsApi;
    if (g.graphicsApi.isEmpty()) g.graphicsApi = QStringLiteral("Unknown");
    if (g.architecture.isEmpty()) g.architecture = QStringLiteral("Unknown");
    if (g.engine.isEmpty()) g.engine = QStringLiteral("Unknown");
    return g;
}

QString readLooseIniKey(const QString &path, const QString &key) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString text = QString::fromUtf8(file.readAll());
    const QRegularExpression rx(QStringLiteral("(?im)^\\s*%1\\s*=\\s*([^\\r\\n;]+)")
                                    .arg(QRegularExpression::escape(key)));
    const auto match = rx.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString normalizePrefixRoot(QString path) {
    path = QDir::cleanPath(path.trimmed());
    if (path.isEmpty()) return {};
    QFileInfo info(path);
    if (info.fileName().compare(QStringLiteral("drive_c"), Qt::CaseInsensitive) == 0)
        path = info.dir().absolutePath();
    if (!QDir(path).exists()) return {};
    if (!QDir(QDir(path).filePath(QStringLiteral("drive_c"))).exists()) return {};
    return QDir(path).absolutePath();
}

QString normalizeGraphicsApi(QString api) {
    api = api.trimmed();
    const QString lower = api.toLower();
    if (lower.isEmpty() || lower == QStringLiteral("auto")) return {};
    if (lower == QStringLiteral("directx 9") || lower == QStringLiteral("dx9")) return QStringLiteral("DirectX 9");
    if (lower == QStringLiteral("directx 10") || lower == QStringLiteral("dx10")) return QStringLiteral("DirectX 10");
    if (lower == QStringLiteral("directx 11") || lower == QStringLiteral("dx11")) return QStringLiteral("DirectX 11");
    if (lower == QStringLiteral("directx 12") || lower == QStringLiteral("dx12")) return QStringLiteral("DirectX 12");
    if (lower == QStringLiteral("vulkan")) return QStringLiteral("Vulkan");
    if (lower == QStringLiteral("opengl") || lower == QStringLiteral("open gl")) return QStringLiteral("OpenGL");
    return {};
}
}


GameModel::GameModel(QObject *parent)
    : QAbstractListModel(parent),
      m_settings(QStringLiteral("Reno119"), QStringLiteral("Reno119")) {
    m_favoritesFirst = m_settings.value(QStringLiteral("library/favoritesFirst"), true).toBool();
    m_scanSteamEnabled = m_settings.value(QStringLiteral("library/scanSteam"), true).toBool();
    m_scanHeroicEnabled = m_settings.value(QStringLiteral("library/scanHeroic"), true).toBool();
    m_scanLutrisEnabled = m_settings.value(QStringLiteral("library/scanLutris"), true).toBool();
    m_heroicRootOverride = m_settings.value(QStringLiteral("library/heroicRootOverride")).toString();
    m_lutrisDataRootOverride = m_settings.value(QStringLiteral("library/lutrisDataRootOverride")).toString();
    m_lutrisConfigRootOverride = m_settings.value(QStringLiteral("library/lutrisConfigRootOverride")).toString();

    m_cacheWriteTimer.setSingleShot(true);
    m_cacheWriteTimer.setInterval(900);
    connect(&m_cacheWriteTimer, &QTimer::timeout, this, [this] {
        if (!m_scanning && !m_allGames.isEmpty())
            saveLibraryCache();
    });
#ifndef RENO119_TESTING
    connect(this, &GameModel::revisionChanged, this, &GameModel::scheduleLibraryCacheWrite);

    const bool restoredCache = loadLibraryCache();
#endif

    connect(&m_watcher, &QFutureWatcher<QVector<GameInfo>>::finished, this, [this] {
        if (m_refreshPending) {
            m_refreshPending = false;
            m_scanning = false;
            emit scanningChanged();
            refresh();
            return;
        }

        QHash<QString, UpdateFlags> previousUpdates;
        QHash<QString, GameInfo> previousGames;
        for (const auto &game : m_allGames) {
            previousUpdates.insert(game.appId,
                                   UpdateFlags{game.updateChecked,
                                               game.reshadeUpdateAvailable,
                                               game.renodxUpdateAvailable,
                                               game.reframeworkUpdateAvailable});
            previousGames.insert(game.appId, game);
        }

        m_allGames = m_watcher.result();
        if (m_cachedSnapshot) {
            m_cachedSnapshot = false;
            emit cachedSnapshotChanged();
        }
        const auto custom = loadCustomPrograms();
        for (const auto &game : custom)
            m_allGames.push_back(game);

        for (auto &game : m_allGames) {
            game.originalCoverArtPath = game.coverArtPath;
            game.originalBannerArtPath = game.bannerArtPath;
            if (game.detectedProtonPrefix.isEmpty())
                game.detectedProtonPrefix = game.protonPrefix;
            if (game.detectedGraphicsApi.isEmpty())
                game.detectedGraphicsApi = game.graphicsApi.isEmpty() ? QStringLiteral("Unknown") : game.graphicsApi;
            const auto previous = previousGames.constFind(game.appId);
            if (previous != previousGames.cend()) {
                game.detectionHistory = previous.value().detectionHistory;
                appendDetectionChanges(game, previous.value().detectedExePath,
                                        previous.value().detectedProtonPrefix,
                                        previous.value().detectedGraphicsApi);
            }
        }

        applyExecutableOverrides();
        rebuildDuplicateMetadata();
        QSet<QString> availableIds;
        for (const auto &game : std::as_const(m_allGames))
            availableIds.insert(game.appId);
        const int oldBulkCount = m_bulkSelection.size();
        m_bulkSelection.intersect(availableIds);
        if (m_bulkSelection.size() != oldBulkCount)
            emit bulkSelectionChanged();
        for (auto &game : m_allGames) {
            const auto it = previousUpdates.constFind(game.appId);
            if (it != previousUpdates.cend()) {
                game.updateChecked = it.value().checked;
                game.reshadeUpdateAvailable = it.value().reshade;
                game.renodxUpdateAvailable = it.value().renodx;
                game.reframeworkUpdateAvailable = it.value().reframework;
            }
        }
        sortGames(m_allGames);
        emit totalCountChanged();
        applyFilter();
        m_scanning = false;
        emit scanningChanged();
        saveLibraryCache();
    });
#ifndef RENO119_TESTING
    if (restoredCache)
        QTimer::singleShot(400, this, &GameModel::refresh);
    else
        refresh();
#endif
}

#ifdef RENO119_TESTING
void GameModel::setGamesForTesting(const QVector<GameInfo> &games) {
    beginResetModel();
    m_allGames = games;
    m_games = games;
    endResetModel();
    m_filterMode = QStringLiteral("all");
    m_searchText.clear();
    m_sourceFilter = QStringLiteral("all");
}
#endif

int GameModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_games.size();
}

QString GameModel::wineOverridesFor(const GameInfo &g) {
    QStringList dlls;

    QString reshadeDll;
    bool managedProxyRecorded = false;
    bool managedProxyIsIndirect = false;
    if (!g.exePath.isEmpty()) {
        QFile stateFile(QFileInfo(g.exePath).absolutePath() + QStringLiteral("/.reno119-state.json"));
        if (stateFile.open(QIODevice::ReadOnly)) {
            const auto state = QJsonDocument::fromJson(stateFile.readAll()).object();
            reshadeDll = state.value(QStringLiteral("reshadeProxy")).toString();
            managedProxyRecorded = !reshadeDll.isEmpty();
            if (reshadeDll.contains('/') || reshadeDll.contains('\\') ||
                reshadeDll.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0 ||
                reshadeDll.compare(QStringLiteral("ReShade.asi"), Qt::CaseInsensitive) == 0) {
                managedProxyIsIndirect = true;
                reshadeDll.clear();
            } else if (reshadeDll.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)) {
                reshadeDll.chop(4);
            }
        }
    }

    if (!managedProxyRecorded && g.reshadeExternal && !g.reshadeProxy.isEmpty()) {
        QString externalProxy = g.reshadeProxy;
        if (externalProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) != 0 &&
            externalProxy.compare(QStringLiteral("ReShade32.dll"), Qt::CaseInsensitive) != 0 &&
            externalProxy.compare(QStringLiteral("ReShade.asi"), Qt::CaseInsensitive) != 0) {
            if (externalProxy.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
                externalProxy.chop(4);
            reshadeDll = externalProxy;
        }
    }

    if (!managedProxyRecorded && reshadeDll.isEmpty() && g.reshadeInstalled && !g.reshade64Installed) {
        if (g.graphicsApi == QStringLiteral("DirectX 9"))
            reshadeDll = QStringLiteral("d3d9");
        else if (g.graphicsApi == QStringLiteral("OpenGL"))
            reshadeDll = QStringLiteral("opengl32");
        else if (g.graphicsApi.startsWith(QStringLiteral("DirectX")))
            reshadeDll = QStringLiteral("dxgi");
    }

    if (!managedProxyIsIndirect && !reshadeDll.isEmpty())
        dlls << reshadeDll;

    // REFramework is injected through dinput8.dll on Proton. Keep it in the
    // same suggested override expression as ReShade. Reno119 never writes Steam
    // launch options automatically; this value is advisory/copy-only.
    if (g.reframeworkInstalled)
        dlls << QStringLiteral("dinput8");

    dlls.removeDuplicates();
    if (dlls.isEmpty())
        return {};

    QStringList overrides;
    for (const QString &dll : dlls)
        overrides << QStringLiteral("%1=n,b").arg(dll);
    return QStringLiteral("WINEDLLOVERRIDES=\"%1\"").arg(overrides.join(';'));
}

QString GameModel::launchOptionsFor(const GameInfo &g) {
    const QString env = wineOverridesFor(g);
    return env.isEmpty() ? QString() : env + QStringLiteral(" %command%");
}

QVariant GameModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_games.size())
        return {};
    const auto &g = m_games[index.row()];
    switch (role) {
    case NameRole: return displayNameFor(g);
    case OriginalNameRole: return g.name;
    case NicknameRole: return g.nickname;
    case AppIdRole: return g.appId;
    case SourceRole: return g.source;
    case CustomProgramRole: return g.customProgram;
    case FavoriteRole: return g.favorite;
    case DuplicateCandidateCountRole: return g.duplicateCandidateCount;
    case LinkedAlternateCountRole: return g.linkedAlternateCount;
    case InstallPathRole: return g.installPath;
    case SteamLibraryRole: return g.steamLibrary;
    case ProtonPrefixRole: return g.protonPrefix;
    case DetectedProtonPrefixRole: return g.detectedProtonPrefix;
    case PrefixOverriddenRole: return g.prefixOverridden;
    case ExePathRole: return g.exePath;
    case GraphicsApiRole: return g.graphicsApi;
    case DetectedGraphicsApiRole: return g.detectedGraphicsApi;
    case GraphicsApiOverriddenRole: return g.graphicsApiOverridden;
    case ArchitectureRole: return g.architecture;
    case EngineRole: return g.engine;
    case CoverArtSourceRole: return g.coverArtPath.isEmpty() ? QString() : QUrl::fromLocalFile(g.coverArtPath).toString();
    case BannerArtSourceRole: return g.bannerArtPath.isEmpty() ? QString() : QUrl::fromLocalFile(g.bannerArtPath).toString();
    case CoverArtPathRole: return g.coverArtPath;
    case BannerArtPathRole: return g.bannerArtPath;
    case ArtworkOverriddenRole: return g.artworkOverridden;
    case HiddenRole: return g.hidden;
    case ExeOverriddenRole: return g.exeOverridden;
    case DetectedExePathRole: return g.detectedExePath;
    case ReShadeInstalledRole: return g.reshadeInstalled;
    case ReShadeManagedRole: return g.reshadeManaged;
    case ReShadeExternalRole: return g.reshadeExternal;
    case ReShadeVersionRole: return g.reshadeVersion;
    case ReShadeChannelRole: return g.reshadeChannel;
    case ReShadeProxyRole: return g.reshadeProxy;
    case ReShade64InstalledRole: return g.reshade64Installed;
    case ReShade64ManagedRole: return g.reshade64Managed;
    case ReShade64ExternalRole: return g.reshade64External;
    case ReShade64VersionRole: return g.reshade64Version;
    case ReShade64ChannelRole: return g.reshade64Channel;
    case RenoDxInstalledRole: return g.renodxInstalled;
    case RenoDxManagedRole: return g.renodxManaged;
    case RenoDxExternalRole: return g.renodxExternal;
    case RenoDxFileRole: return g.renodxFile;
    case UpdateCheckedRole: return g.updateChecked;
    case ReShadeUpdateAvailableRole: return g.reshadeUpdateAvailable;
    case RenoDxUpdateAvailableRole: return g.renodxUpdateAvailable;
    case ReFrameworkUpdateAvailableRole: return g.reframeworkUpdateAvailable;
    case ReFrameworkSupportedRole: return g.reframeworkSupported;
    case ReFrameworkInstalledRole: return g.reframeworkInstalled;
    case ReFrameworkManagedRole: return g.reframeworkManaged;
    case ReFrameworkExternalRole: return g.reframeworkExternal;
    case ReFrameworkVersionRole: return g.reframeworkVersion;
    case OptiScalerInstalledRole: return g.optiScalerInstalled;
    case IntegrityWarningRole: return g.integrityWarning;
    case IntegritySummaryRole: return g.integritySummary;
    case DiagnosticLevelRole: return basicDiagnosticFor(g).value(QStringLiteral("level"));
    case DiagnosticSummaryRole: return basicDiagnosticFor(g).value(QStringLiteral("summary"));
    case HealthLevelRole: return healthInfoForGame(g).value(QStringLiteral("level"));
    case HealthSummaryRole: return healthInfoForGame(g).value(QStringLiteral("summary"));
    case HealthTargetRole: return healthInfoForGame(g).value(QStringLiteral("target"));
    case OverrideCountRole: return overrideCountForGame(g);
    case DetectionChangeCountRole: return g.detectionHistory.size();
    case DetectionHistoryRole: return g.detectionHistory;
    case BulkSelectedRole: return m_bulkSelection.contains(g.appId);
    case LaunchOptionsRole: return launchOptionsFor(g);
    case WineOverridesRole: return wineOverridesFor(g);
    default: return {};
    }
}

QHash<int, QByteArray> GameModel::roleNames() const {
    return {
        {NameRole, "name"},
        {OriginalNameRole, "originalName"},
        {NicknameRole, "nickname"},
        {AppIdRole, "appId"},
        {SourceRole, "source"},
        {CustomProgramRole, "customProgram"},
        {FavoriteRole, "favorite"},
        {DuplicateCandidateCountRole, "duplicateCandidateCount"},
        {LinkedAlternateCountRole, "linkedAlternateCount"},
        {InstallPathRole, "installPath"},
        {SteamLibraryRole, "steamLibrary"},
        {ProtonPrefixRole, "protonPrefix"},
        {DetectedProtonPrefixRole, "detectedProtonPrefix"},
        {PrefixOverriddenRole, "prefixOverridden"},
        {ExePathRole, "exePath"},
        {GraphicsApiRole, "graphicsApi"},
        {DetectedGraphicsApiRole, "detectedGraphicsApi"},
        {GraphicsApiOverriddenRole, "graphicsApiOverridden"},
        {ArchitectureRole, "architecture"},
        {EngineRole, "engine"},
        {CoverArtSourceRole, "coverArtSource"},
        {BannerArtSourceRole, "bannerArtSource"},
        {CoverArtPathRole, "coverArtPath"},
        {BannerArtPathRole, "bannerArtPath"},
        {ArtworkOverriddenRole, "artworkOverridden"},
        {HiddenRole, "hidden"},
        {ExeOverriddenRole, "exeOverridden"},
        {DetectedExePathRole, "detectedExePath"},
        {ReShadeInstalledRole, "reshadeInstalled"},
        {ReShadeManagedRole, "reshadeManaged"},
        {ReShadeExternalRole, "reshadeExternal"},
        {ReShadeVersionRole, "reshadeVersion"},
        {ReShadeChannelRole, "reshadeChannel"},
        {ReShadeProxyRole, "reshadeProxy"},
        {ReShade64InstalledRole, "reshade64Installed"},
        {ReShade64ManagedRole, "reshade64Managed"},
        {ReShade64ExternalRole, "reshade64External"},
        {ReShade64VersionRole, "reshade64Version"},
        {ReShade64ChannelRole, "reshade64Channel"},
        {RenoDxInstalledRole, "renodxInstalled"},
        {RenoDxManagedRole, "renodxManaged"},
        {RenoDxExternalRole, "renodxExternal"},
        {RenoDxFileRole, "renodxFile"},
        {UpdateCheckedRole, "updateChecked"},
        {ReShadeUpdateAvailableRole, "reshadeUpdateAvailable"},
        {RenoDxUpdateAvailableRole, "renodxUpdateAvailable"},
        {ReFrameworkUpdateAvailableRole, "reframeworkUpdateAvailable"},
        {ReFrameworkSupportedRole, "reframeworkSupported"},
        {ReFrameworkInstalledRole, "reframeworkInstalled"},
        {ReFrameworkManagedRole, "reframeworkManaged"},
        {ReFrameworkExternalRole, "reframeworkExternal"},
        {ReFrameworkVersionRole, "reframeworkVersion"},
        {OptiScalerInstalledRole, "optiScalerInstalled"},
        {IntegrityWarningRole, "integrityWarning"},
        {IntegritySummaryRole, "integritySummary"},
        {DiagnosticLevelRole, "diagnosticLevel"},
        {DiagnosticSummaryRole, "diagnosticSummary"},
        {HealthLevelRole, "healthLevel"},
        {HealthSummaryRole, "healthSummary"},
        {HealthTargetRole, "healthTarget"},
        {OverrideCountRole, "overrideCount"},
        {DetectionChangeCountRole, "detectionChangeCount"},
        {DetectionHistoryRole, "detectionHistory"},
        {BulkSelectedRole, "bulkSelected"},
        {LaunchOptionsRole, "launchOptions"},
        {WineOverridesRole, "wineOverrides"}
    };
}

QString GameModel::libraryCachePath() const {
    QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty())
        root = QDir::homePath() + QStringLiteral("/.cache/reno119");
    QDir().mkpath(root);
    return QDir(root).filePath(QStringLiteral("library-snapshot-v1.json"));
}

bool GameModel::loadLibraryCache() {
    QFile file(libraryCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    const int cacheSchema = root.value(QStringLiteral("schema")).toInt();
    if (cacheSchema != 1 && cacheSchema != 2)
        return false;

    QVector<GameInfo> cached;
    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    cached.reserve(games.size());
    for (const QJsonValue &value : games) {
        if (!value.isObject())
            continue;
        GameInfo game = gameFromCacheJson(value.toObject());
        if (game.appId.isEmpty() || game.name.isEmpty())
            continue;
        if ((!m_scanSteamEnabled && game.source.compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0) ||
            (!m_scanHeroicEnabled && game.source.compare(QStringLiteral("Heroic"), Qt::CaseInsensitive) == 0) ||
            (!m_scanLutrisEnabled && game.source.compare(QStringLiteral("Lutris"), Qt::CaseInsensitive) == 0))
            continue;
        cached.push_back(std::move(game));
    }
    if (cached.isEmpty())
        return false;

    m_allGames = std::move(cached);
    m_cachedSnapshot = true;
    emit cachedSnapshotChanged();
    sortGames(m_allGames);
    emit totalCountChanged();
    applyFilter();
    return true;
}

void GameModel::saveLibraryCache() const {
    if (m_allGames.isEmpty())
        return;
    QJsonArray games;
    for (const GameInfo &game : m_allGames)
        games.append(gameToCacheJson(game));
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 2);
    root.insert(QStringLiteral("games"), games);

    QSaveFile file(libraryCachePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

void GameModel::scheduleLibraryCacheWrite() {
    if (m_allGames.isEmpty())
        return;
    if (m_scanning)
        return;
    m_cacheWriteTimer.start();
}

void GameModel::refresh() {
    if (m_scanning) {
        m_refreshPending = true;
        return;
    }
    const bool scanSteam = m_scanSteamEnabled;
    const bool scanHeroic = m_scanHeroicEnabled;
    const bool scanLutris = m_scanLutrisEnabled;
    const QString heroicRoot = m_heroicRootOverride;
    const QString lutrisDataRoot = m_lutrisDataRootOverride;
    const QString lutrisConfigRoot = m_lutrisConfigRootOverride;

    m_scanning = true;
    emit scanningChanged();
    m_watcher.setFuture(QtConcurrent::run([scanSteam, scanHeroic, scanLutris, heroicRoot, lutrisDataRoot, lutrisConfigRoot] {
        QVector<GameInfo> games;
        if (scanSteam)
            games = SteamScanner::scan();
        if (scanHeroic) {
            const auto heroic = HeroicScanner::scan(heroicRoot);
            games.reserve(games.size() + heroic.size());
            for (const auto &game : heroic)
                games.push_back(game);
        }
        if (scanLutris) {
            const auto lutris = LutrisScanner::scan(lutrisDataRoot, lutrisConfigRoot);
            games.reserve(games.size() + lutris.size());
            for (const auto &game : lutris)
                games.push_back(game);
        }
        return games;
    }));
}

void GameModel::requestRefresh() {
    if (m_scanning)
        m_refreshPending = true;
    else
        refresh();
}

void GameModel::setFavoritesFirst(bool enabled) {
    if (m_favoritesFirst == enabled)
        return;
    m_favoritesFirst = enabled;
    m_settings.setValue(QStringLiteral("library/favoritesFirst"), enabled);
    emit librarySettingsChanged();
    applyFilter();
}

void GameModel::setScanSteamEnabled(bool enabled) {
    if (m_scanSteamEnabled == enabled) return;
    m_scanSteamEnabled = enabled;
    m_settings.setValue(QStringLiteral("library/scanSteam"), enabled);
    emit librarySettingsChanged();
    requestRefresh();
}

void GameModel::setScanHeroicEnabled(bool enabled) {
    if (m_scanHeroicEnabled == enabled) return;
    m_scanHeroicEnabled = enabled;
    m_settings.setValue(QStringLiteral("library/scanHeroic"), enabled);
    emit librarySettingsChanged();
    requestRefresh();
}

void GameModel::setScanLutrisEnabled(bool enabled) {
    if (m_scanLutrisEnabled == enabled) return;
    m_scanLutrisEnabled = enabled;
    m_settings.setValue(QStringLiteral("library/scanLutris"), enabled);
    emit librarySettingsChanged();
    requestRefresh();
}

void GameModel::setHeroicRootOverride(const QString &path) {
    const QString normalized = path.trimmed();
    if (m_heroicRootOverride == normalized) return;
    m_heroicRootOverride = normalized;
    if (normalized.isEmpty()) m_settings.remove(QStringLiteral("library/heroicRootOverride"));
    else m_settings.setValue(QStringLiteral("library/heroicRootOverride"), normalized);
    emit librarySettingsChanged();
    if (m_scanHeroicEnabled) requestRefresh();
}

void GameModel::setLutrisDataRootOverride(const QString &path) {
    const QString normalized = path.trimmed();
    if (m_lutrisDataRootOverride == normalized) return;
    m_lutrisDataRootOverride = normalized;
    if (normalized.isEmpty()) m_settings.remove(QStringLiteral("library/lutrisDataRootOverride"));
    else m_settings.setValue(QStringLiteral("library/lutrisDataRootOverride"), normalized);
    emit librarySettingsChanged();
    if (m_scanLutrisEnabled) requestRefresh();
}

void GameModel::setLutrisConfigRootOverride(const QString &path) {
    const QString normalized = path.trimmed();
    if (m_lutrisConfigRootOverride == normalized) return;
    m_lutrisConfigRootOverride = normalized;
    if (normalized.isEmpty()) m_settings.remove(QStringLiteral("library/lutrisConfigRootOverride"));
    else m_settings.setValue(QStringLiteral("library/lutrisConfigRootOverride"), normalized);
    emit librarySettingsChanged();
    if (m_scanLutrisEnabled) requestRefresh();
}

const GameInfo *GameModel::game(int row) const {
    return (row >= 0 && row < m_games.size()) ? &m_games[row] : nullptr;
}

QVariantMap GameModel::gameAt(int row) const {
    QVariantMap m;
    const auto *g = game(row);
    if (!g)
        return m;
    m["name"] = displayNameFor(*g);
    m["originalName"] = g->name;
    m["nickname"] = g->nickname;
    m["appId"] = g->appId;
    m["source"] = g->source;
    m["customProgram"] = g->customProgram;
    m["favorite"] = g->favorite;
    m["duplicateCandidateCount"] = g->duplicateCandidateCount;
    m["linkedAlternateCount"] = g->linkedAlternateCount;
    m["alternateGroupId"] = g->alternateGroupId;
    m["installPath"] = g->installPath;
    m["steamLibrary"] = g->steamLibrary;
    m["protonPrefix"] = g->protonPrefix;
    m["detectedProtonPrefix"] = g->detectedProtonPrefix;
    m["prefixOverridden"] = g->prefixOverridden;
    m["exePath"] = g->exePath;
    m["detectedExePath"] = g->detectedExePath;
    m["graphicsApi"] = g->graphicsApi;
    m["detectedGraphicsApi"] = g->detectedGraphicsApi;
    m["graphicsApiOverridden"] = g->graphicsApiOverridden;
    m["architecture"] = g->architecture;
    m["engine"] = g->engine;
    m["coverArtPath"] = g->coverArtPath;
    m["bannerArtPath"] = g->bannerArtPath;
    m["originalCoverArtPath"] = g->originalCoverArtPath;
    m["originalBannerArtPath"] = g->originalBannerArtPath;
    m["artworkOverridden"] = g->artworkOverridden;
    m["coverArtSource"] = g->coverArtPath.isEmpty() ? QString() : QUrl::fromLocalFile(g->coverArtPath).toString();
    m["bannerArtSource"] = g->bannerArtPath.isEmpty() ? QString() : QUrl::fromLocalFile(g->bannerArtPath).toString();
    m["hidden"] = g->hidden;
    m["exeOverridden"] = g->exeOverridden;
    m["reshadeInstalled"] = g->reshadeInstalled;
    m["reshadeManaged"] = g->reshadeManaged;
    m["reshadeExternal"] = g->reshadeExternal;
    m["reshadeVersion"] = g->reshadeVersion;
    m["reshadeChannel"] = g->reshadeChannel;
    m["reshadeProxy"] = g->reshadeProxy;
    m["reshade64Installed"] = g->reshade64Installed;
    m["reshade64Managed"] = g->reshade64Managed;
    m["reshade64External"] = g->reshade64External;
    m["reshade64Version"] = g->reshade64Version;
    m["reshade64Channel"] = g->reshade64Channel;
    m["renodxInstalled"] = g->renodxInstalled;
    m["renodxManaged"] = g->renodxManaged;
    m["renodxExternal"] = g->renodxExternal;
    m["renodxFile"] = g->renodxFile;
    m["updateChecked"] = g->updateChecked;
    m["reshadeUpdateAvailable"] = g->reshadeUpdateAvailable;
    m["renodxUpdateAvailable"] = g->renodxUpdateAvailable;
    m["reframeworkUpdateAvailable"] = g->reframeworkUpdateAvailable;
    m["reframeworkSupported"] = g->reframeworkSupported;
    m["reframeworkInstalled"] = g->reframeworkInstalled;
    m["reframeworkManaged"] = g->reframeworkManaged;
    m["reframeworkExternal"] = g->reframeworkExternal;
    m["reframeworkVersion"] = g->reframeworkVersion;
    m["integrityWarning"] = g->integrityWarning;
    m["integritySummary"] = g->integritySummary;
    const QVariantMap diagnostics = basicDiagnosticFor(*g);
    const QVariantMap health = healthInfoForGame(*g);
    m["diagnosticLevel"] = diagnostics.value(QStringLiteral("level"));
    m["diagnosticSummary"] = diagnostics.value(QStringLiteral("summary"));
    m["healthLevel"] = health.value(QStringLiteral("level"));
    m["healthSummary"] = health.value(QStringLiteral("summary"));
    m["healthTarget"] = health.value(QStringLiteral("target"));
    m["overrideCount"] = overrideCountForGame(*g);
    m["detectionChangeCount"] = g->detectionHistory.size();
    m["detectionHistory"] = g->detectionHistory;
    m["bulkSelected"] = m_bulkSelection.contains(g->appId);
    m["launchOptions"] = launchOptionsFor(*g);
    m["wineOverrides"] = wineOverridesFor(*g);
    return m;
}

QVariantMap GameModel::diagnosticInfoFor(const GameInfo &g) const {
    QVariantMap result;
    QStringList problems;
    QStringList warnings;

    const bool installExists = !g.installPath.trimmed().isEmpty() && QDir(g.installPath).exists();
    const bool exeExists = !g.exePath.trimmed().isEmpty() && QFileInfo(g.exePath).isFile();
    bool exeValid = false;
    PeParser::Result activePe;
    QStringList graphicsEvidence;
    if (exeExists) {
        activePe = PeParser::inspect(g.exePath);
        exeValid = activePe.valid;
        if (activePe.valid && activePe.graphicsApi != QStringLiteral("Unknown")) {
            graphicsEvidence << QStringLiteral("PE import table detected %1 from the active executable.").arg(activePe.graphicsApi);
        } else if (activePe.valid) {
            QStringList fallbackEvidence;
            const QString fallbackApi = PeParser::detectGraphicsApiFallback(g.exePath, &fallbackEvidence);
            graphicsEvidence << fallbackEvidence;
            if (fallbackEvidence.isEmpty())
                graphicsEvidence << QStringLiteral("No decisive graphics-runtime evidence was found beside the active executable.");
            else
                graphicsEvidence << QStringLiteral("Fallback result: %1.").arg(fallbackApi);
        }
    }

    const bool prefixExists = !g.protonPrefix.trimmed().isEmpty() && QDir(g.protonPrefix).exists();
    const bool prefixLooksValid = prefixExists && QDir(QDir(g.protonPrefix).filePath(QStringLiteral("drive_c"))).exists();

    if (!installExists)
        warnings << QStringLiteral("Install directory is missing or unavailable.");
    if (!exeExists)
        problems << QStringLiteral("Windows executable is unresolved.");
    else if (!exeValid)
        problems << QStringLiteral("Resolved executable is not a valid Windows PE file.");

    if (!prefixExists) {
        if (g.source.compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0)
            warnings << QStringLiteral("Steam Proton prefix is unresolved after checking known Steam libraries. A custom STEAM_COMPAT_DATA_PATH may require manual review.");
        else
            warnings << QStringLiteral("Wine/Proton prefix is unresolved.");
    } else if (!prefixLooksValid) {
        warnings << QStringLiteral("Prefix directory exists but does not contain drive_c.");
    }

    QVariantList proxyScan;
    QStringList proxyWarnings;
    int unmanagedCount = 0;
    int unidentifiedCount = 0;
    if (exeExists) {
        const QString exeDir = QFileInfo(g.exePath).absolutePath();
        QJsonObject state;
        QFile stateFile(QDir(exeDir).filePath(QStringLiteral(".reno119-state.json")));
        if (stateFile.open(QIODevice::ReadOnly))
            state = QJsonDocument::fromJson(stateFile.readAll()).object();

        QJsonObject optiState;
        QFile optiStateFile(QDir(exeDir).filePath(QStringLiteral(".reno119-optiscaler.json")));
        if (optiStateFile.open(QIODevice::ReadOnly))
            optiState = QJsonDocument::fromJson(optiStateFile.readAll()).object();

        const QString ownedReShade = state.value(QStringLiteral("reshadeProxy")).toString();
        const QString ownedReShade64 = state.value(QStringLiteral("reshade64File")).toString();
        const QString ownedReFramework = state.value(QStringLiteral("reframeworkFile")).toString();
        const QString ownedOpti = optiState.value(QStringLiteral("optiProxy")).toString();

        QStringList candidates = ModDetectionService::reShadeProxyNames();
        candidates.append(ModDetectionService::optiScalerProxyNames());
        candidates << QStringLiteral("dinput8.dll")
                   << QStringLiteral("dsound.dll")
                   << QStringLiteral("xinput1_3.dll");
        candidates.removeDuplicates();

        for (const QString &fileName : std::as_const(candidates)) {
            const QString path = QDir(exeDir).filePath(fileName);
            if (!QFileInfo(path).isFile())
                continue;

            const bool managedReShade = fileName.compare(ownedReShade, Qt::CaseInsensitive) == 0 ||
                                        fileName.compare(ownedReShade64, Qt::CaseInsensitive) == 0;
            const bool managedReFramework = fileName.compare(ownedReFramework, Qt::CaseInsensitive) == 0;
            const bool managedOpti = fileName.compare(ownedOpti, Qt::CaseInsensitive) == 0;
            const bool managed = managedReShade || managedReFramework || managedOpti;

            QString classification;
            if (managedReShade)
                classification = QStringLiteral("Managed ReShade");
            else if (managedReFramework)
                classification = QStringLiteral("Managed REFramework");
            else if (managedOpti)
                classification = QStringLiteral("Managed OptiScaler");
            else if (ModDetectionService::looksLikeOptiScalerBinary(path))
                classification = QStringLiteral("External OptiScaler");
            else if (ModDetectionService::looksLikeReShadeBinary(path))
                classification = QStringLiteral("External ReShade");
            else if (ModDetectionService::looksLikeReFrameworkBinary(path))
                classification = QStringLiteral("External REFramework / compatible loader");
            else
                classification = QStringLiteral("Unidentified proxy / loader");

            const bool unidentified = classification.startsWith(QStringLiteral("Unidentified"));
            if (!managed)
                ++unmanagedCount;
            if (unidentified) {
                ++unidentifiedCount;
                proxyWarnings << QStringLiteral("%1 is present but Reno119 cannot identify its owner. It will not be overwritten automatically.").arg(fileName);
            }

            QVariantMap entry;
            entry.insert(QStringLiteral("file"), fileName);
            entry.insert(QStringLiteral("path"), path);
            entry.insert(QStringLiteral("classification"), classification);
            entry.insert(QStringLiteral("managed"), managed);
            entry.insert(QStringLiteral("severity"), unidentified ? QStringLiteral("warning") : QStringLiteral("info"));
            proxyScan << entry;
        }
    }

    if (unidentifiedCount > 0)
        warnings << QStringLiteral("%1 unidentified proxy/loader DLL%2 need review.")
                        .arg(unidentifiedCount)
                        .arg(unidentifiedCount == 1 ? QString() : QStringLiteral("s"));
    if (g.integrityWarning)
        warnings << QStringLiteral("Reno119-managed component integrity needs attention.");

    QString level = QStringLiteral("ok");
    if (!problems.isEmpty())
        level = QStringLiteral("error");
    else if (!warnings.isEmpty())
        level = QStringLiteral("warning");

    QString summary = QStringLiteral("Ready");
    if (!problems.isEmpty())
        summary = problems.first();
    else if (!warnings.isEmpty())
        summary = warnings.first();

    result.insert(QStringLiteral("level"), level);
    result.insert(QStringLiteral("summary"), summary);
    result.insert(QStringLiteral("problems"), problems);
    result.insert(QStringLiteral("warnings"), warnings);
    result.insert(QStringLiteral("importNotes"), g.importDiagnostics);
    result.insert(QStringLiteral("sourceMetadata"), g.sourceMetadata);
    result.insert(QStringLiteral("installPathOk"), installExists);
    result.insert(QStringLiteral("executableOk"), exeExists && exeValid);
    result.insert(QStringLiteral("prefixOk"), prefixLooksValid);
    result.insert(QStringLiteral("installPath"), g.installPath);
    result.insert(QStringLiteral("executable"), g.exePath);
    result.insert(QStringLiteral("detectedExecutable"), g.detectedExePath);
    result.insert(QStringLiteral("executableOverridden"), g.exeOverridden);
    result.insert(QStringLiteral("prefix"), g.protonPrefix);
    result.insert(QStringLiteral("detectedPrefix"), g.detectedProtonPrefix);
    result.insert(QStringLiteral("prefixOverridden"), g.prefixOverridden);
    result.insert(QStringLiteral("graphicsApi"), g.graphicsApi);
    result.insert(QStringLiteral("detectedGraphicsApi"), g.detectedGraphicsApi);
    result.insert(QStringLiteral("graphicsApiOverridden"), g.graphicsApiOverridden);
    result.insert(QStringLiteral("graphicsApiEvidence"), graphicsEvidence);
    result.insert(QStringLiteral("proxyScan"), proxyScan);
    result.insert(QStringLiteral("proxyWarnings"), proxyWarnings);
    const QVariantMap health = healthInfoForGame(g);
    result.insert(QStringLiteral("unmanagedProxyCount"), unmanagedCount);
    result.insert(QStringLiteral("unidentifiedProxyCount"), unidentifiedCount);
    result.insert(QStringLiteral("healthLevel"), health.value(QStringLiteral("level")));
    result.insert(QStringLiteral("healthSummary"), health.value(QStringLiteral("summary")));
    result.insert(QStringLiteral("healthTarget"), health.value(QStringLiteral("target")));
    result.insert(QStringLiteral("overrideCount"), overrideCountForGame(g));
    result.insert(QStringLiteral("detectionHistory"), g.detectionHistory);
    return result;
}

QVariantMap GameModel::diagnosticInfo(int row) const {
    const auto *g = game(row);
    return g ? diagnosticInfoFor(*g) : QVariantMap{};
}

bool GameModel::clearDetectionHistory(int row) {
    if (row < 0 || row >= m_games.size())
        return false;
    auto &g = m_games[row];
    if (g.detectionHistory.isEmpty())
        return true;
    g.detectionHistory.clear();
    syncGameBackToAllGames(g);
    ++m_revision;
    emit dataChanged(index(row), index(row), {DetectionChangeCountRole, DetectionHistoryRole, HealthLevelRole, HealthSummaryRole, HealthTargetRole});
    emit revisionChanged();
    return true;
}

QStringList GameModel::steamAppIds() const {
    QStringList ids;
    for (const auto &g : m_allGames) {
        if (g.source.compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0 && !g.appId.isEmpty())
            ids << g.appId;
    }
    ids.removeDuplicates();
    return ids;
}

QString GameModel::overrideKey(const QString &appId) const {
    return QStringLiteral("games/%1/executableOverride").arg(appId);
}

QString GameModel::prefixOverrideKey(const QString &appId) const {
    return QStringLiteral("games/%1/prefixOverride").arg(appId);
}

QString GameModel::graphicsApiOverrideKey(const QString &appId) const {
    return QStringLiteral("games/%1/graphicsApiOverride").arg(appId);
}

QString GameModel::hiddenKey(const QString &appId) const {
    return QStringLiteral("games/%1/hidden").arg(appId);
}

QString GameModel::favoriteKey(const QString &appId) const {
    return QStringLiteral("games/%1/favorite").arg(appId);
}

QString GameModel::nicknameKey(const QString &appId) const {
    return QStringLiteral("games/%1/nickname").arg(appId);
}

QString GameModel::artworkOverrideKey(const QString &appId) const {
    return QStringLiteral("games/%1/artworkOverride").arg(appId);
}

QString GameModel::alternateGroupKey(const QString &appId) const {
    return QStringLiteral("games/%1/alternateGroup").arg(appId);
}

QString GameModel::uiChoiceKey(const QString &appId, const QString &name) const {
    return QStringLiteral("games/%1/ui/%2").arg(appId, name);
}

int GameModel::uiChoiceIndex(int row, const QString &name, int defaultValue, int maxValue) const {
    const auto *g = game(row);
    if (!g)
        return defaultValue;
    const int value = m_settings.value(uiChoiceKey(g->appId, name), defaultValue).toInt();
    return qBound(0, value, maxValue);
}

void GameModel::setUiChoiceIndex(int row, const QString &name, int value, int maxValue) {
    const auto *g = game(row);
    if (!g)
        return;
    m_settings.setValue(uiChoiceKey(g->appId, name), qBound(0, value, maxValue));
}

int GameModel::reShadeChoiceIndex(int row) const {
    const auto *g = game(row);
    if (!g)
        return 0;

    // The selector represents the last successfully installed channel, not an
    // uncommitted UI choice. If ReShade is currently installed, the state file
    // is authoritative. If it was later removed, fall back to the last channel
    // that Reno119 successfully installed for this game.
    if (!g->exePath.isEmpty()) {
        QFile stateFile(QFileInfo(g->exePath).absolutePath() + QStringLiteral("/.reno119-state.json"));
        if (stateFile.open(QIODevice::ReadOnly)) {
            const QString channel = QJsonDocument::fromJson(stateFile.readAll()).object()
                                        .value(QStringLiteral("reshadeChannel")).toString().toLower();
            if (channel == QStringLiteral("latest"))
                return 1;
            if (channel == QStringLiteral("custom"))
                return 2;
            if (channel == QStringLiteral("recommended"))
                return 0;
        }
    }

    const int remembered = m_settings.value(uiChoiceKey(g->appId, QStringLiteral("reshadeLastInstalledChoice")), 0).toInt();
    return qBound(0, remembered, 2);
}

void GameModel::recordReShadeInstalledChannel(int row, const QString &channel) {
    const auto *g = game(row);
    if (!g)
        return;

    const QString normalized = channel.trimmed().toLower();
    int index = 0;
    if (normalized == QStringLiteral("latest"))
        index = 1;
    else if (normalized == QStringLiteral("custom"))
        index = 2;

    m_settings.setValue(uiChoiceKey(g->appId, QStringLiteral("reshadeLastInstalledChoice")), index);
}

int GameModel::optiProxyChoiceIndex(int row) const {
    return uiChoiceIndex(row, QStringLiteral("optiProxyAppliedChoice"), 0, 9);
}

void GameModel::setOptiProxyChoiceIndex(int row, int index) {
    setUiChoiceIndex(row, QStringLiteral("optiProxyAppliedChoice"), index, 9);
}

int GameModel::optiMethodChoiceIndex(int row) const {
    return uiChoiceIndex(row, QStringLiteral("optiMethodAppliedChoice"), 0, 3);
}

void GameModel::setOptiMethodChoiceIndex(int row, int index) {
    setUiChoiceIndex(row, QStringLiteral("optiMethodAppliedChoice"), index, 3);
}

QString GameModel::executableOverride(int row) const {
    const auto *g = game(row);
    if (!g)
        return {};
    return m_settings.value(overrideKey(g->appId)).toString();
}

void GameModel::syncGameBackToAllGames(const GameInfo &game) {
    for (auto &allGame : m_allGames) {
        if (allGame.appId == game.appId) {
            allGame = game;
            return;
        }
    }
}

void GameModel::refreshInstallState(GameInfo &g) {
    g.reshadeInstalled = false;
    g.reshadeManaged = false;
    g.reshadeExternal = false;
    g.reshadeVersion.clear();
    g.reshadeChannel.clear();
    g.reshadeProxy.clear();
    g.reshade64Installed = false;
    g.reshade64Managed = false;
    g.reshade64External = false;
    g.reshade64Version.clear();
    g.reshade64Channel.clear();
    g.renodxInstalled = false;
    g.renodxManaged = false;
    g.renodxExternal = false;
    g.renodxFile.clear();
    g.reframeworkInstalled = false;
    g.reframeworkManaged = false;
    g.reframeworkExternal = false;
    g.reframeworkVersion.clear();
    g.optiScalerInstalled = false;
    g.integrityWarning = false;
    g.integritySummary.clear();
    g.reframeworkSupported = reFrameworkSupportedFor(g);
    if (g.exePath.isEmpty())
        return;

    const QString exeDir = QFileInfo(g.exePath).absolutePath();
    const QStringList optiIniCandidates = QDir(exeDir).entryList({QStringLiteral("OptiScaler.ini"), QStringLiteral("optiscaler.ini")}, QDir::Files, QDir::Name);
    g.optiScalerInstalled = !optiIniCandidates.isEmpty() ||
                            QFileInfo::exists(exeDir + QStringLiteral("/OptiScaler.dll")) ||
                            QFileInfo::exists(exeDir + QStringLiteral("/.reno119-optiscaler.json"));
    QJsonObject state;
    QFile stateFile(exeDir + QStringLiteral("/.reno119-state.json"));
    if (stateFile.open(QIODevice::ReadOnly))
        state = QJsonDocument::fromJson(stateFile.readAll()).object();

    QStringList integrityIssues;
    const auto expectManagedFile = [&](const QString &stateKey, const QString &relativePrefix, const QString &label) {
        const QString fileName = state.value(stateKey).toString();
        if (fileName.isEmpty())
            return;
        const QString path = exeDir + QLatin1Char('/') + relativePrefix + fileName;
        if (!QFileInfo::exists(path))
            integrityIssues << QStringLiteral("%1 is recorded as managed but is missing: %2").arg(label, fileName);
    };
    expectManagedFile(QStringLiteral("reshadeProxy"), QString(), QStringLiteral("ReShade proxy"));
    expectManagedFile(QStringLiteral("reshade64File"), QString(), QStringLiteral("ReShade64"));
    expectManagedFile(QStringLiteral("renodxFile"), QStringLiteral("reshade-addons/"), QStringLiteral("RenoDX addon"));
    expectManagedFile(QStringLiteral("reframeworkFile"), QString(), QStringLiteral("REFramework"));

    QFile optiStateFile(exeDir + QStringLiteral("/.reno119-optiscaler.json"));
    if (optiStateFile.open(QIODevice::ReadOnly)) {
        const QJsonObject optiState = QJsonDocument::fromJson(optiStateFile.readAll()).object();
        const QString expectedProxy = optiState.value(QStringLiteral("optiProxy")).toString();
        const QString expectedReshade = optiState.value(QStringLiteral("reshadePath")).toString();
        const QString expectedLoad = optiState.value(QStringLiteral("expectedLoadReshade")).toString();
        if (!expectedProxy.isEmpty() && !QFileInfo::exists(exeDir + QLatin1Char('/') + expectedProxy))
            integrityIssues << QStringLiteral("OptiScaler integration expects %1, but that proxy is missing.").arg(expectedProxy);
        if (!expectedReshade.isEmpty() && !QFileInfo::exists(exeDir + QLatin1Char('/') + expectedReshade))
            integrityIssues << QStringLiteral("OptiScaler integration expects %1, but that ReShade file is missing.").arg(expectedReshade);
        if (!expectedLoad.isEmpty()) {
            QString iniPath;
            const QStringList iniCandidates = QDir(exeDir).entryList({QStringLiteral("OptiScaler.ini"), QStringLiteral("optiscaler.ini")}, QDir::Files, QDir::Name);
            if (!iniCandidates.isEmpty())
                iniPath = exeDir + QLatin1Char('/') + iniCandidates.first();
            if (iniPath.isEmpty()) {
                integrityIssues << QStringLiteral("OptiScaler integration state exists, but OptiScaler.ini is missing.");
            } else {
                const QString currentLoad = readLooseIniKey(iniPath, QStringLiteral("LoadReshade"));
                if (currentLoad.compare(expectedLoad, Qt::CaseInsensitive) != 0)
                    integrityIssues << QStringLiteral("OptiScaler LoadReshade changed from Reno119's applied value (%1 → %2).")
                                           .arg(expectedLoad, currentLoad.isEmpty() ? QStringLiteral("missing") : currentLoad);
            }
        }
    }

    const QString ownedProxy = state.value(QStringLiteral("reshadeProxy")).toString();
    const bool legacyOwnedReShade64 = ownedProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0;
    if (!ownedProxy.isEmpty() && !legacyOwnedReShade64 && QFileInfo::exists(exeDir + QStringLiteral("/") + ownedProxy)) {
        g.reshadeInstalled = true;
        g.reshadeManaged = true;
        g.reshadeVersion = state.value(QStringLiteral("reshadeVersion")).toString();
        g.reshadeChannel = state.value(QStringLiteral("reshadeChannel")).toString();
        g.reshadeProxy = ownedProxy;
    } else {
        const bool reshadeArtifacts = QFileInfo::exists(exeDir + QStringLiteral("/ReShade.ini")) ||
                                      QDir(exeDir + QStringLiteral("/reshade-shaders")).exists();
        if (reshadeArtifacts) {
            const QString proxy = ModDetectionService::firstReShadeProxy(exeDir);
            if (!proxy.isEmpty()) {
                g.reshadeInstalled = true;
                g.reshadeExternal = true;
                g.reshadeProxy = proxy;
                g.reshadeVersion = detectReShadeVersionFromLog(exeDir);
                g.reshadeChannel = QStringLiteral("external");
            }
        }
    }

    QString ownedReShade64 = state.value(QStringLiteral("reshade64File")).toString();
    if (ownedReShade64.isEmpty() && legacyOwnedReShade64)
        ownedReShade64 = ownedProxy;
    const QString reshade64Path = exeDir + QStringLiteral("/ReShade64.dll");
    if (!ownedReShade64.isEmpty() && QFileInfo::exists(exeDir + QStringLiteral("/") + ownedReShade64)) {
        g.reshade64Installed = true;
        g.reshade64Managed = true;
        g.reshade64Version = state.value(QStringLiteral("reshade64Version")).toString();
        if (g.reshade64Version.isEmpty() && legacyOwnedReShade64)
            g.reshade64Version = state.value(QStringLiteral("reshadeVersion")).toString();
        g.reshade64Channel = state.value(QStringLiteral("reshade64Channel")).toString();
        if (g.reshade64Channel.isEmpty() && legacyOwnedReShade64)
            g.reshade64Channel = state.value(QStringLiteral("reshadeChannel")).toString();
    } else if (QFileInfo::exists(reshade64Path) && ModDetectionService::looksLikeReShadeBinary(reshade64Path)) {
        g.reshade64Installed = true;
        g.reshade64External = true;
        g.reshade64Version = detectReShadeVersionFromLog(exeDir);
        g.reshade64Channel = QStringLiteral("external");
    }

    const QString ownedAddon = state.value(QStringLiteral("renodxFile")).toString();
    if (!ownedAddon.isEmpty() && QFileInfo::exists(exeDir + QStringLiteral("/reshade-addons/") + ownedAddon)) {
        g.renodxInstalled = true;
        g.renodxManaged = true;
        g.renodxFile = ownedAddon;
    } else {
        QDir addonDir(exeDir + QStringLiteral("/reshade-addons"));
        const QStringList addons = addonDir.entryList({QStringLiteral("*renodx*.addon64"), QStringLiteral("*renodx*.addon32")}, QDir::Files, QDir::Name);
        if (!addons.isEmpty()) {
            g.renodxInstalled = true;
            g.renodxExternal = true;
            g.renodxFile = addons.first();
        }
    }

    const QString ownedRef = state.value(QStringLiteral("reframeworkFile")).toString();
    const QString refPath = exeDir + QStringLiteral("/dinput8.dll");
    if (!ownedRef.isEmpty() && QFileInfo::exists(exeDir + QStringLiteral("/") + ownedRef)) {
        g.reframeworkInstalled = true;
        g.reframeworkManaged = true;
        g.reframeworkVersion = state.value(QStringLiteral("reframeworkVersion")).toString();
    } else if (QFileInfo::exists(refPath) && ModDetectionService::looksLikeReFrameworkBinary(refPath)) {
        g.reframeworkInstalled = true;
        g.reframeworkExternal = true;
    }

    g.integrityWarning = !integrityIssues.isEmpty();
    g.integritySummary = integrityIssues.join(QStringLiteral("\n"));
}

bool GameModel::reFrameworkSupportedFor(const GameInfo &g) const {
    if (SteamScanner::supportsReFramework(g.name, g.engine))
        return true;
    const QString gameKey = normalizedGameKey(g.name);
    if (gameKey.isEmpty())
        return false;
    for (const QString &title : m_reFrameworkSupportedTitles) {
        const QString supportedKey = normalizedGameKey(title);
        if (supportedKey.size() >= 6 && (gameKey.contains(supportedKey) || supportedKey.contains(gameKey)))
            return true;
    }
    return false;
}

void GameModel::setReFrameworkSupportedTitles(const QStringList &titles) {
    m_reFrameworkSupportedTitles = titles;
    bool changed = false;
    for (auto &game : m_allGames) {
        const bool supported = reFrameworkSupportedFor(game);
        if (game.reframeworkSupported != supported) {
            game.reframeworkSupported = supported;
            changed = true;
        }
    }
    for (auto &game : m_games) {
        const bool supported = reFrameworkSupportedFor(game);
        if (game.reframeworkSupported != supported) {
            game.reframeworkSupported = supported;
            changed = true;
        }
    }
    if (changed) {
        ++m_revision;
        if (!m_games.isEmpty())
            emit dataChanged(index(0), index(m_games.size() - 1), {ReFrameworkSupportedRole});
        emit revisionChanged();
    }
}

void GameModel::applyExecutableOverrides() {
    for (auto &g : m_allGames) {
        if (g.detectedExePath.isEmpty())
            g.detectedExePath = g.exePath;
        if (g.detectedProtonPrefix.isEmpty())
            g.detectedProtonPrefix = g.protonPrefix;
        if (g.detectedGraphicsApi.isEmpty())
            g.detectedGraphicsApi = g.graphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.graphicsApi;

        g.hidden = m_settings.value(hiddenKey(g.appId), false).toBool();
        g.favorite = m_settings.value(favoriteKey(g.appId), false).toBool();
        g.nickname = m_settings.value(nicknameKey(g.appId)).toString().trimmed();
        g.alternateGroupId = m_settings.value(alternateGroupKey(g.appId)).toString().trimmed();
        g.artworkOverridden = false;
        g.prefixOverridden = false;
        g.graphicsApiOverridden = false;

        // Always begin from the latest automatic resolution, then layer Reno119
        // user overrides on top. This keeps diagnostics able to show both values.
        g.protonPrefix = g.detectedProtonPrefix;
        g.exePath = g.detectedExePath;
        g.graphicsApi = g.detectedGraphicsApi;

        const QString storedPrefix = m_settings.value(prefixOverrideKey(g.appId)).toString().trimmed();
        if (!storedPrefix.isEmpty()) {
            const QString normalizedPrefix = normalizePrefixRoot(storedPrefix);
            if (!normalizedPrefix.isEmpty()) {
                g.protonPrefix = normalizedPrefix;
                g.prefixOverridden = true;
            } else {
                m_settings.remove(prefixOverrideKey(g.appId));
            }
        }

        const QString overridePath = m_settings.value(overrideKey(g.appId)).toString().trimmed();
        if (!overridePath.isEmpty() && QFileInfo::exists(overridePath)) {
            const auto pe = PeParser::inspect(overridePath);
            if (pe.valid) {
                g.exePath = QFileInfo(overridePath).absoluteFilePath();
                QString autoApi = pe.graphicsApi;
                if (autoApi == QStringLiteral("Unknown"))
                    autoApi = PeParser::detectGraphicsApiFallback(overridePath);
                g.detectedGraphicsApi = autoApi.isEmpty() ? QStringLiteral("Unknown") : autoApi;
                g.graphicsApi = g.detectedGraphicsApi;
                g.architecture = pe.architecture;
                g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
                g.reframeworkSupported = reFrameworkSupportedFor(g);
                g.exeOverridden = true;
            } else {
                g.exePath = g.detectedExePath;
                g.exeOverridden = false;
            }
        } else {
            g.exePath = g.detectedExePath;
            g.exeOverridden = false;
        }

        const QString storedApi = normalizeGraphicsApi(m_settings.value(graphicsApiOverrideKey(g.appId)).toString());
        if (!storedApi.isEmpty()) {
            g.graphicsApi = storedApi;
            g.graphicsApiOverridden = true;
        } else {
            g.graphicsApi = g.detectedGraphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.detectedGraphicsApi;
        }

        const QString artworkPath = m_settings.value(artworkOverrideKey(g.appId)).toString().trimmed();
        if (!artworkPath.isEmpty()) {
            const QFileInfo artworkInfo(artworkPath);
            if (artworkInfo.isFile()) {
                g.coverArtPath = artworkInfo.absoluteFilePath();
                g.bannerArtPath = g.coverArtPath;
                g.artworkOverridden = true;
            } else {
                m_settings.remove(artworkOverrideKey(g.appId));
            }
        }
        refreshInstallState(g);
    }
}

bool GameModel::setExecutableOverride(int row, const QString &path) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString normalized = QFileInfo(path.trimmed()).absoluteFilePath();
    if (!QFileInfo::exists(normalized))
        return false;

    const auto pe = PeParser::inspect(normalized);
    if (!pe.valid)
        return false;

    auto &g = m_games[row];
    m_settings.setValue(overrideKey(g.appId), normalized);
    if (g.detectedExePath.isEmpty())
        g.detectedExePath = g.exePath;
    g.exePath = normalized;
    QString autoApi = pe.graphicsApi;
    if (autoApi == QStringLiteral("Unknown"))
        autoApi = PeParser::detectGraphicsApiFallback(normalized);
    g.detectedGraphicsApi = autoApi.isEmpty() ? QStringLiteral("Unknown") : autoApi;
    g.graphicsApi = g.detectedGraphicsApi;
    const QString apiOverride = normalizeGraphicsApi(m_settings.value(graphicsApiOverrideKey(g.appId)).toString());
    if (!apiOverride.isEmpty()) {
        g.graphicsApi = apiOverride;
        g.graphicsApiOverridden = true;
    } else {
        g.graphicsApiOverridden = false;
    }
    g.architecture = pe.architecture;
    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
    g.reframeworkSupported = reFrameworkSupportedFor(g);
    g.exeOverridden = true;
    refreshInstallState(g);
    syncGameBackToAllGames(g);

    const QModelIndex idx = index(row);
    ++m_revision;
    emit dataChanged(idx, idx);
    emit revisionChanged();
    return true;
}

void GameModel::clearExecutableOverride(int row) {
    if (row < 0 || row >= m_games.size())
        return;

    auto &g = m_games[row];
    m_settings.remove(overrideKey(g.appId));
    g.exePath = g.detectedExePath;
    if (!g.detectedExePath.isEmpty()) {
        const auto pe = PeParser::inspect(g.detectedExePath);
        if (pe.valid) {
            QString autoApi = pe.graphicsApi;
            if (autoApi == QStringLiteral("Unknown"))
                autoApi = PeParser::detectGraphicsApiFallback(g.detectedExePath);
            g.detectedGraphicsApi = autoApi.isEmpty() ? QStringLiteral("Unknown") : autoApi;
            g.architecture = pe.architecture;
        }
    }
    g.graphicsApi = g.detectedGraphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.detectedGraphicsApi;
    const QString apiOverride = normalizeGraphicsApi(m_settings.value(graphicsApiOverrideKey(g.appId)).toString());
    if (!apiOverride.isEmpty()) {
        g.graphicsApi = apiOverride;
        g.graphicsApiOverridden = true;
    } else {
        g.graphicsApiOverridden = false;
    }
    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
    g.reframeworkSupported = reFrameworkSupportedFor(g);
    g.exeOverridden = false;
    refreshInstallState(g);
    syncGameBackToAllGames(g);

    const QModelIndex idx = index(row);
    ++m_revision;
    emit dataChanged(idx, idx);
    emit revisionChanged();
}

QString GameModel::prefixOverride(int row) const {
    const auto *g = game(row);
    return g ? m_settings.value(prefixOverrideKey(g->appId)).toString() : QString();
}

bool GameModel::setPrefixOverride(int row, const QString &path) {
    if (row < 0 || row >= m_games.size())
        return false;
    const QString normalized = normalizePrefixRoot(path);
    if (normalized.isEmpty())
        return false;

    const QString appId = m_games[row].appId;
    m_settings.setValue(prefixOverrideKey(appId), normalized);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;
    return rescanGame(row);
}

void GameModel::clearPrefixOverride(int row) {
    if (row < 0 || row >= m_games.size())
        return;
    m_settings.remove(prefixOverrideKey(m_games[row].appId));
    rescanGame(row);
}

QString GameModel::graphicsApiOverride(int row) const {
    const auto *g = game(row);
    return g ? normalizeGraphicsApi(m_settings.value(graphicsApiOverrideKey(g->appId)).toString()) : QString();
}

bool GameModel::setGraphicsApiOverride(int row, const QString &api) {
    if (row < 0 || row >= m_games.size())
        return false;
    const QString normalized = normalizeGraphicsApi(api);
    if (normalized.isEmpty()) {
        clearGraphicsApiOverride(row);
        return api.trimmed().isEmpty() || api.trimmed().compare(QStringLiteral("Auto"), Qt::CaseInsensitive) == 0;
    }

    auto &g = m_games[row];
    m_settings.setValue(graphicsApiOverrideKey(g.appId), normalized);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;
    g.graphicsApi = normalized;
    g.graphicsApiOverridden = true;
    syncGameBackToAllGames(g);
    ++m_revision;
    emit dataChanged(index(row), index(row), {GraphicsApiRole, GraphicsApiOverriddenRole});
    emit revisionChanged();
    return true;
}

void GameModel::clearGraphicsApiOverride(int row) {
    if (row < 0 || row >= m_games.size())
        return;
    auto &g = m_games[row];
    m_settings.remove(graphicsApiOverrideKey(g.appId));
    g.graphicsApi = g.detectedGraphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.detectedGraphicsApi;
    g.graphicsApiOverridden = false;
    syncGameBackToAllGames(g);
    ++m_revision;
    emit dataChanged(index(row), index(row), {GraphicsApiRole, GraphicsApiOverriddenRole});
    emit revisionChanged();
}

bool GameModel::rescanGame(int row) {
    if (row < 0 || row >= m_games.size())
        return false;

    auto &g = m_games[row];
    const QString previousDetectedExe = g.detectedExePath;
    const QString previousDetectedPrefix = g.detectedProtonPrefix;
    const QString previousDetectedApi = g.detectedGraphicsApi;
    QString autoPrefix = g.detectedProtonPrefix;

    // Steam prefixes are cheap to re-resolve by AppID without rescanning the
    // entire launcher library. This specifically handles moved compatdata.
    if (g.source.compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0) {
        QStringList prefixCandidates;
        QString resolvedLibrary;
        autoPrefix = SteamScanner::findProtonPrefix(g.appId, SteamScanner::knownLibraries(), g.steamLibrary,
                                                     &prefixCandidates, &resolvedLibrary);
        g.sourceMetadata.insert(QStringLiteral("prefixCandidates"), prefixCandidates);
        g.sourceMetadata.insert(QStringLiteral("resolvedPrefix"), autoPrefix);
        g.sourceMetadata.insert(QStringLiteral("resolvedPrefixLibrary"), resolvedLibrary);
    } else if (autoPrefix.isEmpty()) {
        QString configured = g.sourceMetadata.value(QStringLiteral("configuredPrefix")).toString().trimmed();
        if (configured.startsWith(QStringLiteral("~/")))
            configured.replace(0, 1, QDir::homePath());
        const QString normalizedConfigured = normalizePrefixRoot(configured);
        if (!normalizedConfigured.isEmpty())
            autoPrefix = normalizedConfigured;
    }
    g.detectedProtonPrefix = autoPrefix;

    const QString storedPrefix = m_settings.value(prefixOverrideKey(g.appId)).toString().trimmed();
    const QString manualPrefix = normalizePrefixRoot(storedPrefix);
    if (!manualPrefix.isEmpty()) {
        g.protonPrefix = manualPrefix;
        g.prefixOverridden = true;
    } else {
        if (!storedPrefix.isEmpty()) m_settings.remove(prefixOverrideKey(g.appId));
        g.protonPrefix = g.detectedProtonPrefix;
        g.prefixOverridden = false;
    }

    QString scanRoot = g.sourceMetadata.value(QStringLiteral("launcherInstallPath")).toString().trimmed();
    if (scanRoot.isEmpty() && g.customProgram) {
        const QString configuredLauncher = g.sourceMetadata.value(QStringLiteral("configuredLauncherExecutable")).toString().trimmed();
        if (!configuredLauncher.isEmpty())
            scanRoot = QFileInfo(configuredLauncher).absolutePath();
    }
    if (scanRoot.isEmpty())
        scanRoot = g.installPath;
    QString autoApi;
    QString autoArch;
    QStringList candidates;
    QString autoExe;
    if (!scanRoot.isEmpty() && QDir(scanRoot).exists())
        autoExe = SteamScanner::findBestExecutable(scanRoot, g.name, autoApi, autoArch, &candidates);

    const bool launcherOnly = !autoExe.isEmpty() && SteamScanner::isLikelyLauncherExecutable(autoExe);
    if ((autoExe.isEmpty() || launcherOnly) && !g.protonPrefix.isEmpty()) {
        QString ubisoftInstallPath;
        QString ubisoftApi;
        QString ubisoftArch;
        QStringList ubisoftCandidates;
        const QString ubisoftExe = SteamScanner::findBestUbisoftExecutable(g.protonPrefix, g.name,
                                                                           ubisoftApi, ubisoftArch,
                                                                           ubisoftInstallPath, &ubisoftCandidates);
        if (!ubisoftExe.isEmpty()) {
            if (!scanRoot.isEmpty())
                g.sourceMetadata.insert(QStringLiteral("launcherInstallPath"), scanRoot);
            g.installPath = ubisoftInstallPath;
            autoExe = ubisoftExe;
            autoApi = ubisoftApi;
            autoArch = ubisoftArch;
            candidates = ubisoftCandidates;
        } else if (launcherOnly) {
            autoExe.clear();
        }
    }

    if (!autoExe.isEmpty()) {
        g.detectedExePath = autoExe;
        if (autoApi.isEmpty() || autoApi == QStringLiteral("Unknown"))
            autoApi = PeParser::detectGraphicsApiFallback(autoExe);
        g.detectedGraphicsApi = autoApi.isEmpty() ? QStringLiteral("Unknown") : autoApi;
        if (!autoArch.isEmpty())
            g.architecture = autoArch;
        g.sourceMetadata.insert(QStringLiteral("autoSelectedExecutable"), autoExe);
        g.sourceMetadata.insert(QStringLiteral("executableCandidates"), candidates);
    }

    const QString exeOverride = m_settings.value(overrideKey(g.appId)).toString().trimmed();
    if (!exeOverride.isEmpty() && QFileInfo(exeOverride).isFile()) {
        const auto pe = PeParser::inspect(exeOverride);
        if (pe.valid) {
            g.exePath = QFileInfo(exeOverride).absoluteFilePath();
            QString activeAutoApi = pe.graphicsApi;
            if (activeAutoApi == QStringLiteral("Unknown"))
                activeAutoApi = PeParser::detectGraphicsApiFallback(g.exePath);
            g.detectedGraphicsApi = activeAutoApi.isEmpty() ? QStringLiteral("Unknown") : activeAutoApi;
            g.architecture = pe.architecture;
            g.exeOverridden = true;
        } else {
            m_settings.remove(overrideKey(g.appId));
            g.exePath = g.detectedExePath;
            g.exeOverridden = false;
        }
    } else {
        g.exePath = g.detectedExePath;
        g.exeOverridden = false;
    }

    g.graphicsApi = g.detectedGraphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.detectedGraphicsApi;
    const QString apiOverride = normalizeGraphicsApi(m_settings.value(graphicsApiOverrideKey(g.appId)).toString());
    if (!apiOverride.isEmpty()) {
        g.graphicsApi = apiOverride;
        g.graphicsApiOverridden = true;
    } else {
        g.graphicsApiOverridden = false;
    }

    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
    g.reframeworkSupported = reFrameworkSupportedFor(g);
    appendDetectionChanges(g, previousDetectedExe, previousDetectedPrefix, previousDetectedApi);
    refreshInstallState(g);
    syncGameBackToAllGames(g);
    ++m_revision;
    emit dataChanged(index(row), index(row));
    emit revisionChanged();
    return true;
}

void GameModel::refreshInstallState(int row) {
    if (row < 0 || row >= m_games.size())
        return;
    auto &g = m_games[row];
    refreshInstallState(g);
    syncGameBackToAllGames(g);
    const QModelIndex idx = index(row);
    ++m_revision;
    emit dataChanged(idx, idx);
    emit revisionChanged();
}

void GameModel::reloadConfiguration() {
    m_settings.sync();
    m_favoritesFirst = m_settings.value(QStringLiteral("library/favoritesFirst"), true).toBool();
    m_scanSteamEnabled = m_settings.value(QStringLiteral("library/scanSteam"), true).toBool();
    m_scanHeroicEnabled = m_settings.value(QStringLiteral("library/scanHeroic"), true).toBool();
    m_scanLutrisEnabled = m_settings.value(QStringLiteral("library/scanLutris"), true).toBool();
    m_heroicRootOverride = m_settings.value(QStringLiteral("library/heroicRootOverride")).toString();
    m_lutrisDataRootOverride = m_settings.value(QStringLiteral("library/lutrisDataRootOverride")).toString();
    m_lutrisConfigRootOverride = m_settings.value(QStringLiteral("library/lutrisConfigRootOverride")).toString();
    emit librarySettingsChanged();
    refresh();
}

bool GameModel::removeCustomProgram(int row) {
    if (row < 0 || row >= m_games.size() || !m_games[row].customProgram)
        return false;
    const QString id = m_games[row].appId;
    deleteCustomProgram(id);
    m_allGames.erase(std::remove_if(m_allGames.begin(), m_allGames.end(), [&](const GameInfo &g) {
        return g.appId == id;
    }), m_allGames.end());
    rebuildDuplicateMetadata();
    emit totalCountChanged();
    applyFilter();
    return true;
}
