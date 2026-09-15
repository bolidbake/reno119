#include "LutrisScanner.h"
#include "../graphics/PeParser.h"
#include "../steam/SteamScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

namespace {
QString unquote(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\'')))
        value = value.mid(1, value.size() - 2);
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QHash<QString, QString> gameSection(const QString &path) {
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;

    bool inGame = false;
    int gameIndent = -1;
    while (!file.atEnd()) {
        const QString raw = QString::fromUtf8(file.readLine());
        if (raw.trimmed().isEmpty() || raw.trimmed().startsWith('#'))
            continue;
        int indent = 0;
        while (indent < raw.size() && raw.at(indent).isSpace()) ++indent;
        const QString trimmed = raw.trimmed();
        if (!inGame) {
            if (trimmed == QStringLiteral("game:")) {
                inGame = true;
                gameIndent = indent;
            }
            continue;
        }
        if (indent <= gameIndent)
            break;
        const QRegularExpressionMatch match = QRegularExpression(QStringLiteral(R"(^([A-Za-z0-9_]+)\s*:\s*(.*)$)")).match(trimmed);
        if (!match.hasMatch())
            continue;
        QString value = match.captured(2).trimmed();
        if (!value.startsWith('"') && !value.startsWith('\'')) {
            const int comment = value.indexOf(QStringLiteral(" #"));
            if (comment >= 0) value = value.left(comment).trimmed();
        }
        values.insert(match.captured(1), unquote(value));
    }
    return values;
}

QString expandGameDir(QString value, const QString &gameDir) {
    value = value.trimmed();
    if (value.isEmpty())
        return {};
    value.replace(QStringLiteral("${GAMEDIR}"), gameDir);
    value.replace(QStringLiteral("$GAMEDIR"), gameDir);
    if (value.startsWith(QStringLiteral("~/")))
        value.replace(0, 1, QDir::homePath());
    return QDir::cleanPath(value);
}

QString normalizePrefix(const QString &raw, const QString &gameDir) {
    QString prefix = expandGameDir(raw, gameDir);
    if (prefix.isEmpty())
        return {};
    if (!QFileInfo(prefix).isAbsolute())
        prefix = QDir(gameDir).filePath(prefix);
    if (QDir(prefix + QStringLiteral("/drive_c")).exists())
        return QDir::cleanPath(prefix);
    if (QDir(prefix + QStringLiteral("/pfx/drive_c")).exists())
        return QDir::cleanPath(prefix + QStringLiteral("/pfx"));
    return QDir(prefix).exists() ? QDir::cleanPath(prefix) : QString();
}

QString findArtwork(const QString &root, const QString &slug, const QString &kind) {
    if (slug.isEmpty()) return {};
    const QString base = QDir(root).filePath(kind + QStringLiteral("/") + slug);
    for (const QString &ext : {QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".webp")}) {
        const QFileInfo info(base + ext);
        if (info.isFile() && info.size() > 512)
            return info.absoluteFilePath();
    }
    return {};
}

QVector<GameInfo> scanRoot(const QString &dataRoot, const QString &configRoot) {
    QVector<GameInfo> result;
    const QString dbPath = QDir(dataRoot).filePath(QStringLiteral("pga.db"));
    if (!QFileInfo::exists(dbPath))
        return result;

    const QString connectionName = QStringLiteral("reno119_lutris_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("SELECT id, name, slug, runner, directory, configpath FROM games WHERE installed = 1 AND configpath IS NOT NULL AND configpath != ''"))) {
                while (query.next()) {
                    const QString runner = query.value(3).toString();
                    if (runner.compare(QStringLiteral("wine"), Qt::CaseInsensitive) != 0)
                        continue;
                    const QString id = query.value(0).toString();
                    const QString name = query.value(1).toString();
                    const QString slug = query.value(2).toString();
                    QString directory = query.value(4).toString().trimmed();
                    const QString configId = query.value(5).toString();
                    if (directory.startsWith(QStringLiteral("~/")))
                        directory.replace(0, 1, QDir::homePath());
                    if (!directory.isEmpty())
                        directory = QDir::cleanPath(directory);

                    const QString yaml = QDir(configRoot).filePath(QStringLiteral("games/%1.yml").arg(configId));
                    const auto game = gameSection(yaml);
                    const QString configuredExe = game.value(QStringLiteral("exe"));
                    const QString configuredPrefix = game.value(QStringLiteral("prefix"));
                    QString exe = expandGameDir(configuredExe, directory);
                    if (!exe.isEmpty() && !QFileInfo(exe).isAbsolute())
                        exe = QDir(directory).filePath(exe);

                    GameInfo g;
                    g.name = name.trimmed().isEmpty() ? slug : name;
                    g.appId = QStringLiteral("lutris:%1").arg(id);
                    g.source = QStringLiteral("Lutris");
                    g.installPath = QDir(directory).exists() ? directory : (!directory.isEmpty() ? directory : QFileInfo(exe).absolutePath());
                    g.protonPrefix = normalizePrefix(configuredPrefix, g.installPath);

                    if (!QFileInfo::exists(yaml))
                        g.importDiagnostics << QStringLiteral("Lutris game config was not found: %1").arg(yaml);
                    if (!directory.isEmpty() && !QDir(directory).exists())
                        g.importDiagnostics << QStringLiteral("Lutris install directory does not exist: %1").arg(directory);

                    if (!exe.isEmpty() && QFileInfo::exists(exe)) {
                        const auto pe = PeParser::inspect(exe);
                        if (pe.valid) {
                            g.exePath = QFileInfo(exe).absoluteFilePath();
                            g.graphicsApi = pe.graphicsApi;
                            if (g.graphicsApi == QStringLiteral("Unknown"))
                                g.graphicsApi = PeParser::detectGraphicsApiFallback(exe);
                            g.architecture = pe.architecture;
                        } else {
                            g.importDiagnostics << QStringLiteral("Lutris's configured executable exists but is not a valid Windows PE: %1").arg(exe);
                        }
                    } else if (!exe.isEmpty()) {
                        g.importDiagnostics << QStringLiteral("Lutris's configured executable was not found: %1").arg(exe);
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
                        g.importDiagnostics << QStringLiteral("Could not resolve a Windows executable from Lutris metadata, the install directory, or Ubisoft Connect games inside the configured prefix.");

                    if (configuredPrefix.trimmed().isEmpty())
                        g.importDiagnostics << QStringLiteral("Lutris did not provide a Wine prefix path for this entry.");
                    else if (g.protonPrefix.isEmpty())
                        g.importDiagnostics << QStringLiteral("Lutris's configured Wine prefix could not be found: %1").arg(configuredPrefix);

                    if (g.graphicsApi.isEmpty()) g.graphicsApi = QStringLiteral("Unknown");
                    if (g.architecture.isEmpty()) g.architecture = QStringLiteral("Unknown");
                    g.detectedExePath = g.exePath;
                    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
                    g.reframeworkSupported = SteamScanner::supportsReFramework(g.name, g.engine);
                    g.coverArtPath = findArtwork(dataRoot, slug, QStringLiteral("coverart"));
                    g.bannerArtPath = findArtwork(dataRoot, slug, QStringLiteral("banners"));
                    if (g.bannerArtPath.isEmpty()) g.bannerArtPath = g.coverArtPath;
                    g.sourceMetadata.insert(QStringLiteral("dataRoot"), dataRoot);
                    g.sourceMetadata.insert(QStringLiteral("configRoot"), configRoot);
                    g.sourceMetadata.insert(QStringLiteral("database"), dbPath);
                    g.sourceMetadata.insert(QStringLiteral("launcherId"), id);
                    g.sourceMetadata.insert(QStringLiteral("slug"), slug);
                    g.sourceMetadata.insert(QStringLiteral("runner"), runner);
                    g.sourceMetadata.insert(QStringLiteral("configId"), configId);
                    g.sourceMetadata.insert(QStringLiteral("configFile"), yaml);
                    g.sourceMetadata.insert(QStringLiteral("configuredDirectory"), directory);
                    g.sourceMetadata.insert(QStringLiteral("configuredExe"), configuredExe);
                    g.sourceMetadata.insert(QStringLiteral("configuredPrefix"), configuredPrefix);
                    if (!executableCandidates.isEmpty()) {
                        g.sourceMetadata.insert(QStringLiteral("autoSelectedExecutable"), g.exePath);
                        g.sourceMetadata.insert(QStringLiteral("executableCandidates"), executableCandidates);
                    }
                    result.push_back(g);
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return result;
}

}

QVector<GameInfo> LutrisScanner::scan(const QString &additionalDataRoot, const QString &additionalConfigRoot) {
    QVector<GameInfo> result;
    const QString home = QDir::homePath();
    const struct { QString data; QString config; } roots[] = {
        {home + QStringLiteral("/.local/share/lutris"), home + QStringLiteral("/.config/lutris")},
        {home + QStringLiteral("/.var/app/net.lutris.Lutris/data/lutris"), home + QStringLiteral("/.var/app/net.lutris.Lutris/config/lutris")}
    };
    QSet<QString> seen;
    auto appendRoot = [&](QString data, QString config) {
        data = data.trimmed();
        config = config.trimmed();
        if (data.startsWith(QStringLiteral("~/"))) data.replace(0, 1, home);
        if (config.startsWith(QStringLiteral("~/"))) config.replace(0, 1, home);
        data = QDir::cleanPath(data);
        if (config.isEmpty()) config = data;
        else config = QDir::cleanPath(config);
        if (!QDir(data).exists()) return;
        if (!QDir(config).exists()) config = data;
        const auto games = scanRoot(data, config);
        for (const auto &game : games) {
            if (!seen.contains(game.appId)) {
                seen.insert(game.appId);
                result.push_back(game);
            }
        }
    };

    for (const auto &root : roots)
        appendRoot(root.data, root.config);

    if (!additionalDataRoot.trimmed().isEmpty())
        appendRoot(additionalDataRoot, additionalConfigRoot);

    return result;
}
