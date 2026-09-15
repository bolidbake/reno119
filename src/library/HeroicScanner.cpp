#include "HeroicScanner.h"
#include "../graphics/PeParser.h"
#include "../steam/SteamScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStringList>
#include <utility>

namespace {
QString normalizedPrefix(const QString &raw) {
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return {};
    const QString path = QDir::cleanPath(trimmed);
    if (QDir(path + QStringLiteral("/drive_c")).exists())
        return path;
    if (QDir(path + QStringLiteral("/pfx/drive_c")).exists())
        return path + QStringLiteral("/pfx");
    return QDir(path).exists() ? path : QString();
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonObject heroicGameConfig(const QString &appFolder, const QString &appName) {
    const QJsonObject root = readObject(QDir(appFolder).filePath(QStringLiteral("GamesConfig/%1.json").arg(appName)));
    if (root.isEmpty())
        return {};
    const QJsonObject perGame = root.value(appName).toObject();
    return perGame.isEmpty() ? root : perGame;
}

QString heroicPrefixRaw(const QString &appFolder, const QString &appName) {
    const QJsonObject config = heroicGameConfig(appFolder, appName);
    QString prefix = config.value(QStringLiteral("winePrefix")).toString().trimmed();
    if (prefix.startsWith(QStringLiteral("~/")))
        prefix.replace(0, 1, QDir::homePath());
    return prefix;
}

QString heroicTargetExe(const QString &appFolder, const QString &appName) {
    const QJsonObject config = heroicGameConfig(appFolder, appName);
    return config.value(QStringLiteral("targetExe")).toString().trimmed();
}

QString epicTitle(const QString &legendaryRoot, const QString &appName) {
    const QJsonObject root = readObject(QDir(legendaryRoot).filePath(QStringLiteral("metadata/%1.json").arg(appName)));
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QString title = metadata.value(QStringLiteral("title")).toString().trimmed();
    return title.isEmpty() ? appName : title;
}

QString firstExistingImage(const QStringList &candidates) {
    for (const QString &path : candidates) {
        const QFileInfo info(path);
        if (info.isFile() && info.size() > 512)
            return info.absoluteFilePath();
    }
    return {};
}

QString heroicIcon(const QString &appFolder, const QString &appName) {
    const QString base = QDir(appFolder).filePath(QStringLiteral("icons/%1").arg(appName));
    return firstExistingImage({base + ".png", base + ".jpg", base + ".jpeg", base + ".webp"});
}

GameInfo makeHeroicGame(const QString &appFolder,
                        const QString &store,
                        const QString &appName,
                        const QString &title,
                        const QString &installPath,
                        const QString &configuredExe) {
    GameInfo g;
    g.name = title.trimmed().isEmpty() ? appName : title.trimmed();
    g.appId = QStringLiteral("heroic:%1:%2").arg(store, appName);
    g.source = QStringLiteral("Heroic");
    g.installPath = QDir::cleanPath(installPath);

    const QString rawPrefix = heroicPrefixRaw(appFolder, appName);
    g.protonPrefix = normalizedPrefix(rawPrefix);

    const QString targetExe = heroicTargetExe(appFolder, appName);
    QString exe = targetExe;
    if (exe.isEmpty())
        exe = configuredExe.trimmed();
    const QString rawConfiguredExe = exe;
    if (exe.startsWith(QStringLiteral("~/")))
        exe.replace(0, 1, QDir::homePath());
    if (!exe.isEmpty() && !QFileInfo(exe).isAbsolute())
        exe = QDir(g.installPath).filePath(exe);

    if (!exe.isEmpty() && QFileInfo::exists(exe)) {
        const auto pe = PeParser::inspect(exe);
        if (pe.valid) {
            g.exePath = QFileInfo(exe).absoluteFilePath();
            g.graphicsApi = pe.graphicsApi;
            if (g.graphicsApi == QStringLiteral("Unknown"))
                g.graphicsApi = PeParser::detectGraphicsApiFallback(exe);
            g.architecture = pe.architecture;
        } else {
            g.importDiagnostics << QStringLiteral("Heroic's configured executable exists but is not a valid Windows PE: %1").arg(exe);
        }
    } else if (!exe.isEmpty()) {
        g.importDiagnostics << QStringLiteral("Heroic's configured executable was not found: %1").arg(exe);
    }

    QStringList executableCandidates;
    if (g.exePath.isEmpty() && QDir(g.installPath).exists())
        g.exePath = SteamScanner::findBestExecutable(g.installPath, g.name, g.graphicsApi, g.architecture, &executableCandidates);
    const bool launcherOnlyCandidate = !g.exePath.isEmpty() && SteamScanner::isLikelyLauncherExecutable(g.exePath);
    if ((g.exePath.isEmpty() || launcherOnlyCandidate) && !g.protonPrefix.isEmpty()) {
        QString ubisoftInstallPath;
        QStringList ubisoftCandidates;
        const QString ubisoftExe = SteamScanner::findBestUbisoftExecutable(g.protonPrefix, g.name,
                                                                           g.graphicsApi, g.architecture,
                                                                           ubisoftInstallPath, &ubisoftCandidates);
        if (!ubisoftExe.isEmpty()) {
            g.sourceMetadata.insert(QStringLiteral("launcherInstallPath"), g.installPath);
            if (launcherOnlyCandidate)
                g.sourceMetadata.insert(QStringLiteral("rejectedLauncherExecutable"), g.exePath);
            g.installPath = ubisoftInstallPath;
            g.exePath = ubisoftExe;
            executableCandidates = ubisoftCandidates;
            g.importDiagnostics << QStringLiteral("Resolved the game executable inside the Ubisoft Connect prefix: %1")
                                       .arg(ubisoftExe);
        } else if (launcherOnlyCandidate) {
            g.importDiagnostics << QStringLiteral("The configured executable resolves to a Ubisoft/Uplay launcher rather than the game: %1")
                                       .arg(g.exePath);
            g.exePath.clear();
        }
    }
    if (g.exePath.isEmpty())
        g.importDiagnostics << QStringLiteral("Could not resolve a Windows executable from Heroic metadata, the install directory, or Ubisoft Connect games inside the configured prefix.");

    if (rawPrefix.isEmpty())
        g.importDiagnostics << QStringLiteral("Heroic did not provide a Wine prefix path for this entry.");
    else if (g.protonPrefix.isEmpty())
        g.importDiagnostics << QStringLiteral("Heroic's configured Wine prefix could not be found: %1").arg(rawPrefix);

    if (g.graphicsApi.isEmpty()) g.graphicsApi = QStringLiteral("Unknown");
    if (g.architecture.isEmpty()) g.architecture = QStringLiteral("Unknown");
    g.detectedExePath = g.exePath;
    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
    g.reframeworkSupported = SteamScanner::supportsReFramework(g.name, g.engine);
    g.coverArtPath = heroicIcon(appFolder, appName);
    g.bannerArtPath = g.coverArtPath;
    g.sourceMetadata.insert(QStringLiteral("launcherRoot"), appFolder);
    g.sourceMetadata.insert(QStringLiteral("store"), store);
    g.sourceMetadata.insert(QStringLiteral("launcherId"), appName);
    g.sourceMetadata.insert(QStringLiteral("installPath"), installPath);
    g.sourceMetadata.insert(QStringLiteral("targetExe"), targetExe);
    g.sourceMetadata.insert(QStringLiteral("configuredExe"), rawConfiguredExe);
    g.sourceMetadata.insert(QStringLiteral("configuredPrefix"), rawPrefix);
    if (!executableCandidates.isEmpty()) {
        g.sourceMetadata.insert(QStringLiteral("autoSelectedExecutable"), g.exePath);
        g.sourceMetadata.insert(QStringLiteral("executableCandidates"), executableCandidates);
    }
    return g;
}

void appendObjectEntries(const QString &appFolder,
                         const QString &store,
                         const QJsonObject &object,
                         QVector<GameInfo> &out,
                         QSet<QString> &seen) {
    auto consume = [&](const QString &id, const QJsonObject &entry) {
        QString installPath = entry.value(QStringLiteral("install_path")).toString();
        if (installPath.isEmpty()) installPath = entry.value(QStringLiteral("installPath")).toString();
        if (installPath.isEmpty()) installPath = entry.value(QStringLiteral("path")).toString();
        if (installPath.isEmpty() || !QDir(installPath).exists())
            return;

        QString title = entry.value(QStringLiteral("title")).toString();
        if (title.isEmpty()) title = entry.value(QStringLiteral("name")).toString();
        if (title.isEmpty()) title = id;
        QString exe = entry.value(QStringLiteral("executable")).toString();
        if (exe.isEmpty()) exe = entry.value(QStringLiteral("exe")).toString();

        const QString stableId = QStringLiteral("heroic:%1:%2").arg(store, id);
        if (seen.contains(stableId))
            return;
        GameInfo g = makeHeroicGame(appFolder, store, id, title, installPath, exe);
        seen.insert(stableId);
        out.push_back(g);
    };

    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isObject())
            consume(it.key(), it.value().toObject());
        else if (it.value().isArray()) {
            const QJsonArray array = it.value().toArray();
            for (const QJsonValue &value : array) {
                if (!value.isObject()) continue;
                const QJsonObject entry = value.toObject();
                QString id = entry.value(QStringLiteral("app_name")).toString();
                if (id.isEmpty()) id = entry.value(QStringLiteral("appName")).toString();
                if (id.isEmpty()) id = entry.value(QStringLiteral("id")).toVariant().toString();
                if (!id.isEmpty()) consume(id, entry);
            }
        }
    }
}

void scanEpicRoot(const QString &appFolder,
                  const QString &legendaryRoot,
                  QVector<GameInfo> &out,
                  QSet<QString> &seen) {
    const QJsonObject installed = readObject(QDir(legendaryRoot).filePath(QStringLiteral("installed.json")));
    for (auto it = installed.constBegin(); it != installed.constEnd(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject entry = it.value().toObject();
        const QString installPath = entry.value(QStringLiteral("install_path")).toString();
        if (installPath.isEmpty() || !QDir(installPath).exists())
            continue;
        const QString stableId = QStringLiteral("heroic:epic:%1").arg(it.key());
        if (seen.contains(stableId))
            continue;
        GameInfo g = makeHeroicGame(appFolder,
                                    QStringLiteral("epic"),
                                    it.key(),
                                    epicTitle(legendaryRoot, it.key()),
                                    installPath,
                                    entry.value(QStringLiteral("executable")).toString());
        seen.insert(stableId);
        out.push_back(g);
    }
}
}

QVector<GameInfo> HeroicScanner::scan(const QString &additionalAppFolder) {
    QVector<GameInfo> result;
    QSet<QString> seen;
    const QString home = QDir::homePath();
    QStringList appFolders = {
        home + QStringLiteral("/.config/heroic"),
        home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/heroic")
    };
    const QString extra = additionalAppFolder.trimmed();
    if (!extra.isEmpty()) {
        const QString normalized = QDir::cleanPath(extra.startsWith(QStringLiteral("~/"))
                                                   ? home + extra.mid(1)
                                                   : extra);
        if (!appFolders.contains(normalized))
            appFolders << normalized;
    }

    for (const QString &appFolder : std::as_const(appFolders)) {
        if (!QDir(appFolder).exists())
            continue;

        QStringList legendaryRoots = {
            QDir(appFolder).filePath(QStringLiteral("legendaryConfig/legendary"))
        };
        if (appFolder == home + QStringLiteral("/.config/heroic"))
            legendaryRoots << home + QStringLiteral("/.config/legendary");
        else
            legendaryRoots << home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/legendary");
        for (const QString &root : std::as_const(legendaryRoots))
            scanEpicRoot(appFolder, root, result, seen);

        // Heroic's GOG electron-store and Nile/Amazon install caches are JSON.
        // Their exact metadata differs by runner, so consume only entries with
        // a real install directory. Executable resolution failures remain visible
        // as diagnostics instead of silently dropping the launcher entry.
        const QStringList genericStores = {
            QDir(appFolder).filePath(QStringLiteral("gog_store/installed.json")),
            QDir(appFolder).filePath(QStringLiteral("nile_config/nile/installed.json"))
        };
        for (int i = 0; i < genericStores.size(); ++i) {
            const QJsonObject object = readObject(genericStores.at(i));
            if (!object.isEmpty())
                appendObjectEntries(appFolder, i == 0 ? QStringLiteral("gog") : QStringLiteral("amazon"), object, result, seen);
        }
    }
    return result;
}
