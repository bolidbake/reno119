#include "InstallerManager.h"
#include "InstallerManagerInternal.h"
#include "../core/AppSettings.h"
#include "../core/GameModel.h"
#include "../network/RenoDxCatalogService.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QMap>
#include <QSettings>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <utility>


using namespace Reno119::InstallerInternal;

InstallerManager::InstallerManager(GameModel *games, RenoDxCatalogService *catalog, AppSettings *settings, QObject *parent)
    : QObject(parent), m_games(games), m_catalog(catalog), m_settings(settings) { loadReleaseCache(); }

void InstallerManager::shutdown() {
    m_updateQueueActive = false;
    m_updateQueue.clear();
    m_updateQueueConfirmedRenoDxUrls.clear();
    m_releaseFetchBusy = false;
    m_releaseCallbacks.clear();
    m_bulkUpdateBusy = false;
    m_busy = false;

    const auto replies = m_net.findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
}

void InstallerManager::setBusy(bool v) {
    if (m_busy != v) {
        m_busy = v;
        emit busyChanged();
    }
    if (!v) {
        clearInlineOperation();
        if (m_updateQueueActive)
            QTimer::singleShot(0, this, &InstallerManager::processNextQueuedUpdate);
    }
}

QString InstallerManager::inlineStatusLevel(const QString &text) {
    const QString lower = text.toLower();
    if (lower.contains(QStringLiteral("failed")) ||
        lower.contains(QStringLiteral("could not")) ||
        lower.contains(QStringLiteral("refusing")) ||
        lower.contains(QStringLiteral("unsupported")) ||
        lower.contains(QStringLiteral("invalid")) ||
        lower.contains(QStringLiteral("not configured")) ||
        lower.contains(QStringLiteral("does not exist")) ||
        lower.startsWith(QStringLiteral("no ")) ||
        lower.startsWith(QStringLiteral("unknown ")))
        return QStringLiteral("error");
    if (lower.contains(QStringLiteral("installed successfully")) ||
        lower.startsWith(QStringLiteral("installed custom")) ||
        lower.startsWith(QStringLiteral("removed ")) ||
        lower.startsWith(QStringLiteral("restored ")))
        return QStringLiteral("success");
    return QStringLiteral("info");
}

void InstallerManager::beginInlineOperation(int row, const QString &component) {
    m_activeInlineAppId.clear();
    m_activeInlineComponent.clear();
    if (const auto *g = m_games ? m_games->game(row) : nullptr) {
        m_activeInlineAppId = g->appId;
        m_activeInlineComponent = component;
    }
}

void InstallerManager::clearInlineOperation() {
    m_activeInlineAppId.clear();
    m_activeInlineComponent.clear();
}

void InstallerManager::recordInlineStatus(const QString &text) {
    if (m_activeInlineAppId.isEmpty() || m_activeInlineComponent.isEmpty())
        return;
    const QString key = m_activeInlineAppId + QLatin1Char('\n') + m_activeInlineComponent;
    const QVariantMap next{{QStringLiteral("text"), text},
                           {QStringLiteral("level"), inlineStatusLevel(text)}};
    if (m_inlineStatus.value(key) == next)
        return;
    m_inlineStatus.insert(key, next);
    ++m_inlineStatusRevision;
    emit inlineStatusChanged();
}

QVariantMap InstallerManager::inlineStatus(int row, const QString &component) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return {};
    return m_inlineStatus.value(g->appId + QLatin1Char('\n') + component);
}

void InstallerManager::setStatus(const QString &s) {
    recordInlineStatus(s);
    const bool clearSynchronousContext = !m_busy && !m_activeInlineComponent.isEmpty();
    if (m_status != s) {
        m_status = s;
        logLine(s);
        emit statusChanged();
    }
    if (clearSynchronousContext)
        clearInlineOperation();
}
void InstallerManager::setProgress(double p) { p = qBound(0.0, p, 1.0); if (!qFuzzyCompare(m_progress, p)) { m_progress = p; emit progressChanged(); } }

void InstallerManager::setRenoDxStatus(int row, const QString &text) {
    QString appId;
    if (const auto *g = m_games->game(row))
        appId = g->appId;
    if (m_renoDxStatus == text && m_renoDxStatusAppId == appId)
        return;
    m_renoDxStatus = text;
    m_renoDxStatusAppId = appId;
    emit renoDxStatusChanged();
}

QString InstallerManager::rootCacheBase() const {
    QString root = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (root.isEmpty())
        root = QDir::homePath() + "/.cache";
    const QString d = root + "/reno119";
    QDir().mkpath(d);
    return d;
}

QString InstallerManager::cacheDir() const {
    const QString d = rootCacheBase() + "/reshade";
    QDir().mkpath(d);
    return d;
}

QString InstallerManager::backupsBaseDir() const {
    const QString d = rootCacheBase() + "/backups";
    QDir().mkpath(d);
    return d;
}

QString InstallerManager::cachePath() const { return cacheDir(); }

QString InstallerManager::versionCacheDir(const QString &version) const {
    const QString safeVersion = version.isEmpty() ? QStringLiteral("unknown") : version;
    const QString d = cacheDir() + "/" + safeVersion;
    QDir().mkpath(d);
    return d;
}

QString InstallerManager::statePathForRow(int row) const {
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty())
        return {};
    return QFileInfo(g->exePath).absolutePath() + "/.reno119-state.json";
}

QString InstallerManager::humanSize(qint64 bytes) {
    static const char *suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double size = double(bytes);
    int i = 0;
    while (size >= 1024.0 && i < 4) {
        size /= 1024.0;
        ++i;
    }
    return QString::number(size, 'f', i == 0 ? 0 : 1) + " " + suffixes[i];
}

QString InstallerManager::cacheSizeString() const {
    qint64 total = 0;
    QDirIterator it(cacheDir(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return humanSize(total);
}

void InstallerManager::clearDownloadCache() {
    QDir(cacheDir()).removeRecursively();
    QDir().mkpath(cacheDir());
    ++m_cacheRevision;
    emit cacheInfoChanged();
    setStatus("Cleared cached ReShade downloads and extracted DLLs.");
}

QString InstallerManager::recommendedReShadeVersion(int row) const {
    const auto *g = m_games->game(row);
    return g ? m_catalog->recommendedReShadeVersion(g->name) : QString();
}

QString InstallerManager::customReShadeSummary() const {
    return m_settings ? m_settings->customReShadeSummary() : QStringLiteral("Custom");
}

QString InstallerManager::proxyNameFor(int row) const {
    const auto *g = m_games->game(row);
    if (!g) return {};
    if (g->graphicsApi == "DirectX 9") return "d3d9.dll";
    if (g->graphicsApi == "OpenGL") return "opengl32.dll";
    if (g->graphicsApi.startsWith("DirectX")) return "dxgi.dll";
    return {};
}

QJsonObject InstallerManager::readStateForGame(const GameInfo &game) const {
    if (game.exePath.isEmpty())
        return {};
    const QString path = QFileInfo(game.exePath).absolutePath() + QStringLiteral("/.reno119-state.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QJsonObject InstallerManager::readState(int row) const {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    return game ? readStateForGame(*game) : QJsonObject{};
}

bool InstallerManager::writeState(int row, const QJsonObject &patch) {
    const QString path = statePathForRow(row);
    if (path.isEmpty())
        return false;

    QJsonObject state = readState(row);
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        if (it.value().isNull() || it.value().isUndefined()) state.remove(it.key());
        else state.insert(it.key(), it.value());
    }

    QSaveFile f(path);
    // Some Steam libraries live on filesystems/mounts where QSaveFile's atomic
    // temp-file rename is not supported even though normal file copies work.
    // Falling back to a direct write is preferable to silently losing Reno119
    // ownership metadata and then misclassifying our own install as external.
    f.setDirectWriteFallback(true);
    if (!f.open(QIODevice::WriteOnly)) {
        logLine(QStringLiteral("Failed to open state file for writing: %1 (%2)").arg(path, f.errorString()));
        return false;
    }

    const QByteArray json = QJsonDocument(state).toJson(QJsonDocument::Indented);
    if (f.write(json) != json.size()) {
        logLine(QStringLiteral("Failed to write complete state file: %1 (%2)").arg(path, f.errorString()));
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        logLine(QStringLiteral("Failed to commit state file: %1 (%2)").arg(path, f.errorString()));
        return false;
    }
    return true;
}

bool InstallerManager::copyFileEnsuringParent(const QString &src, const QString &dst) {
    QDir().mkpath(QFileInfo(dst).absolutePath());
    QFile::remove(dst);
    return QFile::copy(src, dst);
}

bool InstallerManager::copyDirectoryContents(const QString &srcDir, const QString &dstDir) {
    QDirIterator it(srcDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = QDir(srcDir).relativeFilePath(it.filePath());
        if (rel == QStringLiteral("backup-manifest.json"))
            continue;
        if (!copyFileEnsuringParent(it.filePath(), dstDir + "/" + rel))
            return false;
    }
    return true;
}

QString InstallerManager::createBackup(int row, const QString &reason, bool force) const {
    if (!force && (!m_settings || !m_settings->backupBeforeChanges()))
        return {};

    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty())
        return {};

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
    const QString backupDir = backupsBaseDir() + "/" + g->appId + "/" + stamp + "-" + reason;
    QDir().mkpath(backupDir);

    QJsonArray files;
    const auto state = readState(row);
    const auto maybeCopy = [&](const QString &src, const QString &rel) {
        if (!QFileInfo::exists(src))
            return;
        const QString dst = backupDir + "/" + rel;
        if (copyFileEnsuringParent(src, dst))
            files.append(rel);
    };

    const QString proxy = state.value("reshadeProxy").toString();
    if (!proxy.isEmpty())
        maybeCopy(exeDir + "/" + proxy, proxy);
    else {
        const QString targetProxy = proxyNameFor(row);
        if (!targetProxy.isEmpty())
            maybeCopy(exeDir + "/" + targetProxy, targetProxy);
    }

    QString reshade64File = state.value("reshade64File").toString();
    if (reshade64File.isEmpty() && state.value("reshadeProxy").toString().compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0)
        reshade64File = QStringLiteral("ReShade64.dll");
    if (!reshade64File.isEmpty())
        maybeCopy(exeDir + "/" + reshade64File, reshade64File);

    const QString addon = state.value("renodxFile").toString();
    if (!addon.isEmpty())
        maybeCopy(exeDir + "/reshade-addons/" + addon, "reshade-addons/" + addon);

    const QString reframeworkFile = state.value(QStringLiteral("reframeworkFile")).toString();
    if (!reframeworkFile.isEmpty())
        maybeCopy(exeDir + QStringLiteral("/") + reframeworkFile, reframeworkFile);
    const QStringList reframeworkConfigs = QDir(exeDir).entryList({QStringLiteral("*_fw_config.txt")}, QDir::Files, QDir::Name);
    for (const QString &configName : reframeworkConfigs)
        maybeCopy(exeDir + QLatin1Char('/') + configName, configName);

    maybeCopy(exeDir + "/ReShade.ini", "ReShade.ini");
    maybeCopy(exeDir + "/.reno119-state.json", ".reno119-state.json");

    QSaveFile manifest(backupDir + "/backup-manifest.json");
    if (manifest.open(QIODevice::WriteOnly)) {
        QJsonObject root{
            {"reason", reason},
            {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {"game", g->name},
            {"appId", g->appId},
            {"files", files}
        };
        manifest.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        manifest.commit();
    }
    return backupDir;
}

QString InstallerManager::createExternalReShadeBackup(int row) const {
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty() || !g->reshadeExternal || g->reshadeProxy.isEmpty())
        return {};

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
    const QString backupDir = backupsBaseDir() + "/" + g->appId + "/" + stamp + "-before-external-reshade-takeover";
    QDir().mkpath(backupDir);

    QJsonArray files;
    const auto maybeCopy = [&](const QString &src, const QString &rel) {
        if (!QFileInfo::exists(src))
            return true;
        const QString dst = backupDir + "/" + rel;
        if (!copyFileEnsuringParent(src, dst))
            return false;
        files.append(rel);
        return true;
    };

    const bool hadStateFile = QFileInfo::exists(exeDir + QStringLiteral("/.reno119-state.json"));
    if (!maybeCopy(exeDir + "/" + g->reshadeProxy, g->reshadeProxy) ||
        !maybeCopy(exeDir + "/ReShade.ini", "ReShade.ini") ||
        !maybeCopy(exeDir + "/.reno119-state.json", ".reno119-state.json")) {
        QDir(backupDir).removeRecursively();
        return {};
    }

    QJsonArray removeOnRestore;
    if (!hadStateFile)
        removeOnRestore.append(QStringLiteral(".reno119-state.json"));

    QSaveFile manifest(backupDir + "/backup-manifest.json");
    manifest.setDirectWriteFallback(true);
    if (!manifest.open(QIODevice::WriteOnly)) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    const QJsonObject root{
        {"reason", QStringLiteral("before-external-reshade-takeover")},
        {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"game", g->name},
        {"appId", g->appId},
        {"externalReShadeTakeover", true},
        {"externalProxy", g->reshadeProxy},
        {"files", files},
        {"removeOnRestore", removeOnRestore}
    };
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (manifest.write(json) != json.size() || !manifest.commit()) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    return backupDir;
}

bool InstallerManager::restoreBackupDirectory(int row, const QString &dir) const {
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty() || dir.isEmpty() || !QDir(dir).exists())
        return false;
    const QString exeDir = QFileInfo(g->exePath).absolutePath();

    QJsonObject manifest;
    QStringList backupFiles;
    QFile manifestFile(dir + QStringLiteral("/backup-manifest.json"));
    if (manifestFile.open(QIODevice::ReadOnly)) {
        manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        const QJsonArray files = manifest.value(QStringLiteral("files")).toArray();
        for (const QJsonValue &value : files) {
            const QString rel = value.toString();
            if (!rel.isEmpty())
                backupFiles << rel;
        }
        const QJsonArray removeOnRestore = manifest.value(QStringLiteral("removeOnRestore")).toArray();
        for (const QJsonValue &value : removeOnRestore) {
            const QString rel = value.toString();
            if (!rel.isEmpty())
                QFile::remove(exeDir + QStringLiteral("/") + rel);
        }
    }

    // A historical restore should not leave newer Reno119-owned components
    // beside the restored snapshot. Only paths recorded in the CURRENT Reno119
    // ownership state are eligible for cleanup; unrelated game/mod files are
    // never swept broadly.
    const QJsonObject currentState = readState(row);
    QStringList currentlyManaged;
    const QString currentProxy = currentState.value(QStringLiteral("reshadeProxy")).toString();
    if (!currentProxy.isEmpty())
        currentlyManaged << currentProxy;
    QString currentReShade64 = currentState.value(QStringLiteral("reshade64File")).toString();
    if (currentReShade64.isEmpty() && currentProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0)
        currentReShade64 = QStringLiteral("ReShade64.dll");
    if (!currentReShade64.isEmpty())
        currentlyManaged << currentReShade64;
    const QString currentAddon = currentState.value(QStringLiteral("renodxFile")).toString();
    if (!currentAddon.isEmpty())
        currentlyManaged << QStringLiteral("reshade-addons/") + currentAddon;
    const QString currentRef = currentState.value(QStringLiteral("reframeworkFile")).toString();
    if (!currentRef.isEmpty())
        currentlyManaged << currentRef;
    currentlyManaged.removeDuplicates();

    for (const QString &rel : currentlyManaged) {
        if (!backupFiles.contains(rel))
            QFile::remove(exeDir + QStringLiteral("/") + rel);
    }

    return copyDirectoryContents(dir, exeDir);
}

QString InstallerManager::createExternalReShade64Backup(int row) const {
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty() || !g->reshade64External)
        return {};

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
    const QString backupDir = backupsBaseDir() + "/" + g->appId + "/" + stamp + "-before-external-reshade64-takeover";
    QDir().mkpath(backupDir);

    QJsonArray files;
    const auto maybeCopy = [&](const QString &src, const QString &rel) {
        if (!QFileInfo::exists(src))
            return true;
        const QString dst = backupDir + "/" + rel;
        if (!copyFileEnsuringParent(src, dst))
            return false;
        files.append(rel);
        return true;
    };

    const bool hadStateFile = QFileInfo::exists(exeDir + QStringLiteral("/.reno119-state.json"));
    if (!maybeCopy(exeDir + QStringLiteral("/ReShade64.dll"), QStringLiteral("ReShade64.dll")) ||
        !maybeCopy(exeDir + QStringLiteral("/ReShade.ini"), QStringLiteral("ReShade.ini")) ||
        !maybeCopy(exeDir + QStringLiteral("/.reno119-state.json"), QStringLiteral(".reno119-state.json"))) {
        QDir(backupDir).removeRecursively();
        return {};
    }

    QJsonArray removeOnRestore;
    if (!hadStateFile)
        removeOnRestore.append(QStringLiteral(".reno119-state.json"));

    QSaveFile manifest(backupDir + QStringLiteral("/backup-manifest.json"));
    manifest.setDirectWriteFallback(true);
    if (!manifest.open(QIODevice::WriteOnly)) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    const QJsonObject root{
        {"reason", QStringLiteral("before-external-reshade64-takeover")},
        {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"game", g->name},
        {"appId", g->appId},
        {"externalReShade64Takeover", true},
        {"files", files},
        {"removeOnRestore", removeOnRestore}
    };
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (manifest.write(json) != json.size() || !manifest.commit()) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    return backupDir;
}

QString InstallerManager::createExternalReFrameworkBackup(int row) const {
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty() || !g->reframeworkExternal)
        return {};

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
    const QString backupDir = backupsBaseDir() + "/" + g->appId + "/" + stamp + "-before-external-reframework-takeover";
    QDir().mkpath(backupDir);

    QJsonArray files;
    const auto maybeCopy = [&](const QString &src, const QString &rel) {
        if (!QFileInfo::exists(src))
            return true;
        const QString dst = backupDir + "/" + rel;
        if (!copyFileEnsuringParent(src, dst))
            return false;
        files.append(rel);
        return true;
    };

    const bool hadStateFile = QFileInfo::exists(exeDir + QStringLiteral("/.reno119-state.json"));
    if (!maybeCopy(exeDir + QStringLiteral("/dinput8.dll"), QStringLiteral("dinput8.dll")) ||
        !maybeCopy(exeDir + QStringLiteral("/.reno119-state.json"), QStringLiteral(".reno119-state.json"))) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    const QStringList reframeworkConfigs = QDir(exeDir).entryList({QStringLiteral("*_fw_config.txt")}, QDir::Files, QDir::Name);
    for (const QString &configName : reframeworkConfigs) {
        if (!maybeCopy(exeDir + QLatin1Char('/') + configName, configName)) {
            QDir(backupDir).removeRecursively();
            return {};
        }
    }

    QJsonArray removeOnRestore;
    if (!hadStateFile)
        removeOnRestore.append(QStringLiteral(".reno119-state.json"));

    QSaveFile manifest(backupDir + QStringLiteral("/backup-manifest.json"));
    manifest.setDirectWriteFallback(true);
    if (!manifest.open(QIODevice::WriteOnly)) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    const QJsonObject root{
        {"reason", QStringLiteral("before-external-reframework-takeover")},
        {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"game", g->name},
        {"appId", g->appId},
        {"externalReFrameworkTakeover", true},
        {"files", files},
        {"removeOnRestore", removeOnRestore}
    };
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (manifest.write(json) != json.size() || !manifest.commit()) {
        QDir(backupDir).removeRecursively();
        return {};
    }
    return backupDir;
}

QString InstallerManager::externalReFrameworkOverwritePreview(int row) const {
    const auto *g = m_games->game(row);
    if (!g || !g->reframeworkExternal)
        return QStringLiteral("No external REFramework installation is currently detected.");
    QStringList lines;
    lines << QStringLiteral("Reno119 did not install the existing REFramework dinput8.dll.")
          << QString()
          << QStringLiteral("Reno119 will create a mandatory takeover backup before replacing it.")
          << QStringLiteral("Files that may be changed:")
          << QStringLiteral("• dinput8.dll")
          << QStringLiteral("• .reno119-state.json (ownership metadata)")
          << QString()
          << QStringLiteral("Existing *_fw_config.txt files, scripts, plugins, and other REFramework content are preserved.");
    return lines.join('\n');
}

QString InstallerManager::externalReShadeOverwritePreview(int row) const {
    const auto *g = m_games->game(row);
    if (!g || !g->reshadeExternal || g->reshadeProxy.isEmpty())
        return QStringLiteral("No external ReShade installation is currently detected.");

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    QStringList lines;
    lines << QStringLiteral("Reno119 did not install the existing ReShade files.")
          << QString()
          << QStringLiteral("Detected external install:")
          << QStringLiteral("• Proxy: %1").arg(g->reshadeProxy)
          << QStringLiteral("• Version: %1").arg(g->reshadeVersion.isEmpty() ? QStringLiteral("unknown") : g->reshadeVersion)
          << QString()
          << QStringLiteral("Reno119 will create a takeover backup before changing anything.")
          << QStringLiteral("Files that may be replaced or modified:")
          << QStringLiteral("• %1").arg(g->reshadeProxy);
    if (QFileInfo::exists(exeDir + QStringLiteral("/ReShade.ini")))
        lines << QStringLiteral("• ReShade.ini (only the add-on path section may be adjusted)");
    else
        lines << QStringLiteral("• ReShade.ini (will be created)");
    lines << QStringLiteral("• .reno119-state.json (ownership metadata will be created)")
          << QString()
          << QStringLiteral("Presets, shaders, and unrelated add-ons are left in place.");
    return lines.join('\n');
}

QString InstallerManager::externalReShade64OverwritePreview(int row) const {
    const auto *g = m_games->game(row);
    if (!g || !g->reshade64External)
        return QStringLiteral("No external ReShade64.dll installation is currently detected.");

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    QStringList lines;
    lines << QStringLiteral("Reno119 did not install the existing ReShade64.dll.")
          << QString()
          << QStringLiteral("Detected OptiScaler chain-loader ReShade:")
          << QStringLiteral("• File: ReShade64.dll")
          << QStringLiteral("• Version: %1").arg(g->reshade64Version.isEmpty() ? QStringLiteral("unknown") : g->reshade64Version)
          << QString()
          << QStringLiteral("Reno119 will create a mandatory takeover backup before changing anything.")
          << QStringLiteral("Files that may be replaced or modified:")
          << QStringLiteral("• ReShade64.dll");
    if (QFileInfo::exists(exeDir + QStringLiteral("/ReShade.ini")))
        lines << QStringLiteral("• ReShade.ini (only the add-on path section may be adjusted)");
    else
        lines << QStringLiteral("• ReShade.ini (will be created)");
    lines << QStringLiteral("• .reno119-state.json (ReShade64 ownership metadata will be created)")
          << QString()
          << QStringLiteral("Presets, shaders, normal proxy ReShade files, and unrelated add-ons are left in place.");
    return lines.join('\n');
}

bool InstallerManager::ensureReShadeIni(const QString &exeDir) {
    const QString path = exeDir + "/ReShade.ini";
    QFile f(path);
    QString text;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        text = QString::fromUtf8(f.readAll());
        f.close();
    } else if (QFileInfo::exists(path)) {
        return false;
    }
    if (!text.contains(QRegularExpression("^\\s*\\[ADDON\\]", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption))) {
        if (!text.isEmpty() && !text.endsWith('\n')) text += '\n';
        text += "\n[ADDON]\nAddonPath=.\\reshade-addons\n";
    } else if (!text.contains(QRegularExpression("^\\s*AddonPath\\s*=", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption))) {
        text.replace(QRegularExpression("(^\\s*\\[ADDON\\]\\s*$)", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption),
                     "\\1\nAddonPath=.\\reshade-addons");
    }
    QSaveFile out(path);
    const QByteArray bytes = text.toUtf8();
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text) || out.write(bytes) != bytes.size() || !out.commit())
        return false;
    return QDir().mkpath(exeDir + "/reshade-addons");
}

void InstallerManager::clearExternalReShadeOverwriteContext() {
    m_externalReShadeOverwriteActive = false;
    m_externalReShadeOverwriteAppId.clear();
    m_externalReShadeProxy.clear();
    m_externalReShadeBackupDir.clear();
}

QVariantList InstallerManager::backupHistory(int row) const {
    QVariantList out;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return out;

    const QString gameDirPath = backupsBaseDir() + QStringLiteral("/") + g->appId;
    QDir gameDir(gameDirPath);
    if (!gameDir.exists())
        return out;

    const QFileInfoList dirs = gameDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QFileInfo &dirInfo : dirs) {
        QFile manifestFile(dirInfo.absoluteFilePath() + QStringLiteral("/backup-manifest.json"));
        if (!manifestFile.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        const QString manifestAppId = manifest.value(QStringLiteral("appId")).toString();
        if (!manifestAppId.isEmpty() && manifestAppId != g->appId)
            continue;

        const QString reason = manifest.value(QStringLiteral("reason")).toString();
        const QString createdUtc = manifest.value(QStringLiteral("createdUtc")).toString();
        QDateTime created = QDateTime::fromString(createdUtc, Qt::ISODate);
        if (created.isValid())
            created = created.toLocalTime();

        QString friendlyReason = reason;
        friendlyReason.replace(QLatin1Char('-'), QLatin1Char(' '));
        if (friendlyReason.startsWith(QStringLiteral("before "), Qt::CaseInsensitive))
            friendlyReason = QStringLiteral("Before ") + friendlyReason.mid(7);
        else if (!friendlyReason.isEmpty())
            friendlyReason[0] = friendlyReason.at(0).toUpper();

        QVariantMap item;
        item[QStringLiteral("path")] = dirInfo.absoluteFilePath();
        item[QStringLiteral("reason")] = reason;
        item[QStringLiteral("component")] = backupComponentForReason(reason);
        item[QStringLiteral("label")] = friendlyReason.isEmpty() ? dirInfo.fileName() : friendlyReason;
        item[QStringLiteral("createdUtc")] = createdUtc;
        item[QStringLiteral("createdDisplay")] = created.isValid()
            ? created.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
            : dirInfo.fileName();
        item[QStringLiteral("fileCount")] = manifest.value(QStringLiteral("files")).toArray().size();
        item[QStringLiteral("externalTakeover")] =
            manifest.value(QStringLiteral("externalReShadeTakeover")).toBool() ||
            manifest.value(QStringLiteral("externalReShade64Takeover")).toBool() ||
            manifest.value(QStringLiteral("externalReFrameworkTakeover")).toBool();
        out << item;
        if (out.size() >= 24)
            break;
    }
    return out;
}

bool InstallerManager::restoreBackup(int row, const QString &backupDir) {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) {
        setStatus(QStringLiteral("No game is selected."));
        return false;
    }

    const QString allowedRoot = QDir(backupsBaseDir() + QStringLiteral("/") + g->appId).canonicalPath();
    const QString candidate = QDir(backupDir).canonicalPath();
    if (allowedRoot.isEmpty() || candidate.isEmpty() ||
        (candidate != allowedRoot && !candidate.startsWith(allowedRoot + QLatin1Char('/')))) {
        setStatus(QStringLiteral("Refusing to restore a backup outside this game's Reno119 backup history."));
        return false;
    }

    if (!restoreBackupDirectory(row, candidate)) {
        setStatus(QStringLiteral("Could not restore the selected component backup."));
        return false;
    }
    if (m_games)
        m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Restored the selected component backup."));
    return true;
}

void InstallerManager::restoreLastBackup(int row) {
    const auto state = readState(row);
    const QString dir = state.value("lastBackupDir").toString();
    if (dir.isEmpty()) {
        setStatus("No backup is recorded for this game yet.");
        return;
    }
    restoreBackup(row, dir);
}
