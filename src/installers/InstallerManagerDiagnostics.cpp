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

QString InstallerManager::debugSummary(int row) const {
    const auto *g = m_games->game(row);
    if (!g)
        return QStringLiteral("No game selected.");

    const QJsonObject state = readState(row);
    QStringList lines;
    lines << "Reno119 debug summary"
          << QString("Application version: %1").arg(QCoreApplication::applicationVersion())
          << QString("Game: %1").arg(g->name)
          << QString("Source: %1").arg(g->source)
          << QString("ID: %1").arg(g->appId)
          << QString("Install path: %1").arg(g->installPath)
          << QString("Steam library: %1").arg(g->steamLibrary)
          << QString("Proton prefix: %1").arg(g->protonPrefix)
          << QString("Executable: %1").arg(g->exePath)
          << QString("Detected executable: %1").arg(g->detectedExePath)
          << QString("Executable override active: %1").arg(g->exeOverridden ? "yes" : "no")
          << QString("Graphics API: %1").arg(g->graphicsApi)
          << QString("Architecture: %1").arg(g->architecture)
          << QString("Engine: %1").arg(g->engine)
          << QString("ReShade installed: %1").arg(g->reshadeInstalled ? "yes" : "no")
          << QString("ReShade ownership: %1").arg(g->reshadeExternal ? "external" : (g->reshadeManaged ? "managed" : "none"))
          << QString("Detected ReShade version: %1").arg(g->reshadeVersion)
          << QString("Detected ReShade proxy: %1").arg(g->reshadeProxy)
          << QString("ReShade64 installed: %1").arg(g->reshade64Installed ? "yes" : "no")
          << QString("ReShade64 ownership: %1").arg(g->reshade64External ? "external" : (g->reshade64Managed ? "managed" : "none"))
          << QString("Detected ReShade64 version: %1").arg(g->reshade64Version)
          << QString("RenoDX installed: %1").arg(g->renodxInstalled ? "yes" : "no")
          << QString("RenoDX ownership: %1").arg(g->renodxExternal ? "external" : (g->renodxManaged ? "managed" : "none"))
          << QString("Detected RenoDX file: %1").arg(g->renodxFile)
          << QString("REFramework supported: %1").arg(g->reframeworkSupported ? "yes" : "no")
          << QString("REFramework installed: %1").arg(g->reframeworkInstalled ? "yes" : "no")
          << QString("REFramework ownership: %1").arg(g->reframeworkExternal ? "external" : (g->reframeworkManaged ? "managed" : "none"))
          << QString("REFramework version: %1").arg(g->reframeworkVersion)
          << QString("ReShade state path: %1").arg(statePathForRow(row))
          << QString("ReShade version: %1").arg(state.value("reshadeVersion").toString())
          << QString("ReShade channel: %1").arg(state.value("reshadeChannel").toString())
          << QString("ReShade proxy: %1").arg(state.value("reshadeProxy").toString())
          << QString("ReShade64 file: %1").arg(state.value("reshade64File").toString())
          << QString("ReShade64 version: %1").arg(state.value("reshade64Version").toString())
          << QString("ReShade64 channel: %1").arg(state.value("reshade64Channel").toString())
          << QString("RenoDX file: %1").arg(state.value("renodxFile").toString())
          << QString("RenoDX URL: %1").arg(state.value("renodxUrl").toString())
          << QString("REFramework file: %1").arg(state.value("reframeworkFile").toString())
          << QString("REFramework recorded version: %1").arg(state.value("reframeworkVersion").toString())
          << QString("Last backup dir: %1").arg(state.value("lastBackupDir").toString())
          << QString("Cache path: %1").arg(cacheDir())
          << QString("Cache size: %1").arg(cacheSizeString())
          << QString("Custom ReShade: %1").arg(customReShadeSummary());
    return lines.join('\n');
}

QVariantMap InstallerManager::renoDxResolutionInfo(int row) const {
    QVariantMap out;
    const auto *g = m_games->game(row);
    if (!g) {
        out[QStringLiteral("source")] = QStringLiteral("No game selected");
        return out;
    }

    const QVariantMap catalogMatch = m_catalog ? m_catalog->renoDxSnapshotInfo(g->name) : QVariantMap{};
    QString url = catalogMatch.value(QStringLiteral("url")).toString();
    const QString matchedCatalogTitle = catalogMatch.value(QStringLiteral("title")).toString();
    const QString matchMethod = catalogMatch.value(QStringLiteral("method")).toString();
    const bool matchRequiresConfirmation = catalogMatch.value(QStringLiteral("requiresConfirmation")).toBool();
    QString source;
    if (!url.isEmpty())
        source = QStringLiteral("RenoDX Mods wiki");

    if (url.isEmpty() && g->engine == QStringLiteral("Unreal Engine") && g->architecture == QStringLiteral("x64")) {
        url = QStringLiteral("https://marat569.github.io/renodx/renodx-ue-extended.addon64");
        source = QStringLiteral("Generic Unreal Engine fallback");
    } else if (url.isEmpty() && g->engine == QStringLiteral("Unity")) {
        url = g->architecture == QStringLiteral("x86")
            ? QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon32")
            : QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon64");
        source = QStringLiteral("Generic Unity fallback");
    }

    const QStringList genericUrls{
        QStringLiteral("https://marat569.github.io/renodx/renodx-ue-extended.addon64"),
        QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon32"),
        QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon64")
    };
    const bool generic = genericUrls.contains(url);
    const bool dedicated = m_catalog->renoDxCatalogReady() && !url.isEmpty() && !generic;
    const QString installedUrl = readState(row).value(QStringLiteral("renodxUrl")).toString();
    const bool dedicatedAvailable = dedicated && g->renodxManaged && genericUrls.contains(installedUrl);
    QString supportState, supportDetail;
    if (!m_catalog->renoDxCatalogFinished()) {
        supportState = QStringLiteral("Checking support…");
        supportDetail = QStringLiteral("Waiting for the RenoDX catalog before confirming dedicated game support.");
    } else if (!m_catalog->renoDxCatalogReady()) {
        supportState = QStringLiteral("Catalog unavailable");
        supportDetail = QStringLiteral("The RenoDX catalog could not be loaded. Dedicated support could not be checked; this does not mean the game is unsupported.");
    } else if (dedicatedAvailable) {
        supportState = QStringLiteral("Dedicated addon now available");
        supportDetail = QStringLiteral("This game is using a managed generic addon. A dedicated game addon is now available: %1. Review the RenoDX update in Update Center; nothing has been changed automatically.")
            .arg(QFileInfo(QUrl(url).path()).fileName());
    } else if (dedicated) {
        supportState = QStringLiteral("Dedicated addon available");
        supportDetail = QStringLiteral("A dedicated game addon was resolved from the RenoDX catalog.");
    } else if (generic) {
        supportState = QStringLiteral("Generic fallback available");
        supportDetail = QStringLiteral("No dedicated addon was resolved for this game. %1 is available; game-specific HDR compatibility is not guaranteed.").arg(source);
    } else {
        supportState = QStringLiteral("No matching addon");
        supportDetail = QStringLiteral("The catalog loaded, but no dedicated addon or supported generic fallback was resolved for this game.");
    }
    if (generic && !m_catalog->renoDxCatalogReady())
        supportDetail += QStringLiteral(" A generic addon is available, but dedicated support is still unconfirmed.");
    if (m_catalog->renoDxCatalogReady() && m_catalog->catalogStale())
        supportDetail += QStringLiteral(" Catalog result is stale (last successful fetch: %1); refresh to confirm current support.").arg(m_catalog->catalogCheckedUtc());
    out[QStringLiteral("supportState")] = supportState;
    out[QStringLiteral("supportDetail")] = supportDetail;
    out[QStringLiteral("dedicatedAvailable")] = dedicatedAvailable;
    out[QStringLiteral("genericAvailable")] = generic;
    out[QStringLiteral("dedicatedMatch")] = dedicated;
    out[QStringLiteral("matchedCatalogTitle")] = dedicated ? matchedCatalogTitle : QString();
    out[QStringLiteral("matchMethod")] = dedicated ? matchMethod : (generic ? QStringLiteral("Engine fallback") : QStringLiteral("No match"));
    out[QStringLiteral("requiresConfirmation")] = dedicated && matchRequiresConfirmation;

    out[QStringLiteral("source")] = source.isEmpty() ? QStringLiteral("No matching addon source") : source;
    out[QStringLiteral("url")] = url;
    out[QStringLiteral("file")] = url.isEmpty() ? QString() : QFileInfo(QUrl(url).path()).fileName();
    out[QStringLiteral("catalogReady")] = m_catalog->renoDxCatalogReady();
    out[QStringLiteral("catalogFinished")] = m_catalog->renoDxCatalogFinished();
    return out;
}

bool InstallerManager::saveDiagnosticsReport(const QUrl &fileUrl, const QString &contents) {
    QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    if (path.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Diagnostics export cancelled: no destination was selected."));
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".txt");

    QSaveFile out(path);
    out.setDirectWriteFallback(true);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not create diagnostics report: %1").arg(path));
        return false;
    }
    const QByteArray bytes = contents.toUtf8();
    if (out.write(bytes) != bytes.size() || !out.commit()) {
        setStatus(QStringLiteral("Could not save diagnostics report: %1").arg(path));
        return false;
    }
    setStatus(QStringLiteral("Diagnostics exported to %1").arg(path));
    return true;
}

void InstallerManager::openGameFolder(int row) {
    const auto *g = m_games->game(row); if (!g || g->exePath.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(g->exePath).absolutePath()));
}

void InstallerManager::openPrefixFolder(int row) {
    const auto *g = m_games->game(row);
    if (!g || g->protonPrefix.isEmpty()) {
        setStatus(QStringLiteral("No Wine / Proton prefix is known for this game."));
        return;
    }
    const QString prefix = QDir::cleanPath(g->protonPrefix);
    if (!QDir(prefix).exists()) {
        setStatus(QStringLiteral("The Wine / Proton prefix does not exist: %1").arg(prefix));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(prefix)))
        setStatus(QStringLiteral("Could not open the Wine / Proton prefix: %1").arg(prefix));
}

void InstallerManager::openRenoDxEngineIniFolder(int row) {
    const QVariantMap info = renoDxTweaksInfo(row);
    const QString engineIniPath = info.value(QStringLiteral("engineIniPath")).toString();
    if (engineIniPath.isEmpty()) {
        setStatus(QStringLiteral("No Engine.ini location is currently resolved for this game."));
        return;
    }

    const QFileInfo engineInfo(engineIniPath);
    const QString folderPath = engineInfo.absolutePath();
    if (!QDir(folderPath).exists()) {
        setStatus(QStringLiteral("The Engine.ini folder does not exist yet: %1").arg(folderPath));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath)))
        setStatus(QStringLiteral("Could not open the Engine.ini folder: %1").arg(folderPath));
}

void InstallerManager::copyText(const QString &text) {
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    setStatus("Copied text to clipboard.");
}

void InstallerManager::logLine(const QString &line) const {
    QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataRoot.isEmpty())
        dataRoot = QDir::homePath() + "/.local/share";
    const QString logDir = dataRoot + "/reno119";
    QDir().mkpath(logDir);
    QFile f(logDir + "/reno119.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << line << "\n";
    }
}

QString InstallerManager::recoveryFolder(int row) const {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game) return {};
    const QDir backups(backupsBaseDir() + QLatin1Char('/') + game->appId);
    const auto directories = backups.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QFileInfo &directory : directories) {
        const QString path = directory.absoluteFilePath();
        if (QFileInfo::exists(path + QStringLiteral("/tweak-manifest.json")) ||
            QFileInfo::exists(path + QStringLiteral("/backup-manifest.json"))) return path;
    }
    return {};
}

void InstallerManager::openRecoveryFolder(int row) {
    const QString path = recoveryFolder(row);
    if (path.isEmpty()) { setStatus(QStringLiteral("No retained recovery copy exists for this game yet.")); return; }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        setStatus(QStringLiteral("Could not open recovery folder: ") + path);
}

QString InstallerManager::redactDiagnostics(const QString &text) const {
    QString result = text;
    const QString home = QDir::homePath();
    if (home.size() > 1) result.replace(home, QStringLiteral("~"));
    result.replace(QRegularExpression(QStringLiteral("/home/[^/\\s]+")), QStringLiteral("/home/<user>"));
    return result;
}

QVariantList InstallerManager::recoveryHistory(int row) const {
    QVariantList result;
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game) return result;
    const QDir base(backupsBaseDir() + QLatin1Char('/') + game->appId);
    for (const QFileInfo &dir : base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
        QFile file(dir.absoluteFilePath() + QStringLiteral("/tweak-manifest.json"));
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto entries = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("files")).toArray();
        if (entries.isEmpty()) continue;
        bool addon = false;
        for (const auto &entry : entries) if (entry.toObject().value(QStringLiteral("path")).toString().contains(QStringLiteral("/reshade-addons/"))) addon = true;
        result << QVariantMap{{QStringLiteral("path"), dir.absoluteFilePath()}, {QStringLiteral("label"), dir.fileName().left(19) + (addon ? QStringLiteral(" · RenoDX addon recovery") : QStringLiteral(" · INI recovery"))}};
    }
    return result;
}

QVariantMap InstallerManager::recoveryPreview(int row, const QString &path) const {
    QVariantMap result;
    bool allowed = false;
    for (const QVariant &entry : recoveryHistory(row)) if (entry.toMap().value(QStringLiteral("path")).toString() == path) allowed = true;
    if (!allowed) { result[QStringLiteral("error")] = QStringLiteral("Select a recovery copy belonging to this game."); return result; }
    QFile manifest(path + QStringLiteral("/tweak-manifest.json"));
    if (!manifest.open(QIODevice::ReadOnly)) return result;
    const QByteArray manifestBytes = manifest.readAll();
    const auto entries = QJsonDocument::fromJson(manifestBytes).object().value(QStringLiteral("files")).toArray();
    QStringList targets, lines;
    QByteArray review = manifestBytes;
    bool addon = false;
    for (const auto &value : entries) {
        const auto entry = value.toObject();
        const QString target = entry.value(QStringLiteral("path")).toString();
        if (!QDir::isAbsolutePath(target)) { result[QStringLiteral("error")] = QStringLiteral("Invalid recovery target."); return result; }
        if (target.endsWith(QStringLiteral("/.reno119-state.json")) && target != statePathForRow(row)) {
            result[QStringLiteral("error")] = QStringLiteral("This addon recovery copy uses a different executable folder."); return result;
        }
        targets << target;
        addon |= target == statePathForRow(row);
        review += QJsonDocument(tweakFileState(target)).toJson(QJsonDocument::Compact);
        if (entry.value(QStringLiteral("existed")).toBool()) {
            const QString name = entry.value(QStringLiteral("backup")).toString();
            if (name.isEmpty() || QFileInfo(name).fileName() != name) return result;
            const auto fingerprint = tweakFileState(path + QLatin1Char('/') + name);
            if (!fingerprint.value(QStringLiteral("exists")).toBool() || fingerprint.contains(QStringLiteral("error"))) {
                result[QStringLiteral("error")] = QStringLiteral("Missing or unreadable backup data."); return result;
            }
            review += QJsonDocument(fingerprint).toJson(QJsonDocument::Compact);
        }
        lines << (entry.value(QStringLiteral("existed")).toBool() ? QStringLiteral("Restore: ") : QStringLiteral("Remove if present: ")) + target;
    }
    const auto *game = m_games->game(row);
    const auto state = readState(row);
    if (addon) {
        const QString owned = state.value(QStringLiteral("renodxFile")).toString();
        if (!owned.isEmpty() && QFileInfo(owned).fileName() == owned) {
            const QString currentAddon = QFileInfo(game->exePath).absolutePath() + QStringLiteral("/reshade-addons/") + owned;
            if (!targets.contains(currentAddon)) { targets << currentAddon; lines << QStringLiteral("Remove current managed addon: ") + currentAddon; }
        }
        const QDir addons(QFileInfo(game->exePath).absolutePath() + QStringLiteral("/reshade-addons"));
        for (const QFileInfo &file : addons.entryInfoList({QStringLiteral("*renodx*.addon64"), QStringLiteral("*renodx*.addon32")}, QDir::Files, QDir::Name)) {
            if (!targets.contains(file.absoluteFilePath())) {
                result[QStringLiteral("error")] = QStringLiteral("An additional unmanaged RenoDX addon is present: ") + file.fileName() + QStringLiteral(". Resolve it before recovering to avoid duplicate addons."); return result;
            }
        }
        lines << QStringLiteral("Only RenoDX ownership fields are recovered; other component ownership is retained.");
    }
    for (const QString &target : targets) {
        const auto fingerprint = tweakFileState(target);
        if (fingerprint.contains(QStringLiteral("error"))) { result[QStringLiteral("error")] = QStringLiteral("Cannot read a current target file."); return result; }
        review += QJsonDocument(fingerprint).toJson(QJsonDocument::Compact);
    }
    review += QJsonDocument(state).toJson(QJsonDocument::Compact) + game->exePath.toUtf8() + path.toUtf8();
    result[QStringLiteral("token")] = QString::fromLatin1(QCryptographicHash::hash(review, QCryptographicHash::Sha256).toHex());
    result[QStringLiteral("text")] = lines.join(QLatin1Char('\n')) + QStringLiteral("\n\nCurrent versions of these files will be backed up before recovery. This replaces their contents and permissions.");
    result[QStringLiteral("targets")] = targets;
    result[QStringLiteral("addon")] = addon;
    result[QStringLiteral("canRestore")] = !targets.isEmpty();
    return result;
}

bool InstallerManager::restoreRecovery(int row, const QString &path, const QString &token) {
    if (m_busy) return false;
    beginInlineOperation(row, QStringLiteral("renodx"));
    const auto preview = recoveryPreview(row, path);
    if (token.isEmpty() || token != preview.value(QStringLiteral("token")).toString() || !preview.value(QStringLiteral("canRestore")).toBool()) {
        setStatus(QStringLiteral("Recovery files changed or are unavailable. Refresh the preview.")); return false;
    }
    const auto currentState = readState(row);
    QString rollbackDir, error;
    if (!createRenoDxTweakSnapshot(row, preview.value(QStringLiteral("targets")).toStringList(), rollbackDir, error)) { setStatus(error); return false; }
    if (recoveryPreview(row, path).value(QStringLiteral("token")).toString() != token) {
        setStatus(QStringLiteral("Files changed during backup. Refresh the recovery preview.")); return false;
    }
    bool ok = restoreRenoDxTweakSnapshot(row, path, error);
    if (ok && preview.value(QStringLiteral("addon")).toBool()) {
        const auto recoveredState = readState(row);
        QJsonObject patch = currentState;
        for (auto it = recoveredState.begin(); it != recoveredState.end(); ++it)
            if (!currentState.contains(it.key())) patch.insert(it.key(), QJsonValue());
        patch.insert(QStringLiteral("renodxFile"), recoveredState.value(QStringLiteral("renodxFile")));
        patch.insert(QStringLiteral("renodxUrl"), recoveredState.value(QStringLiteral("renodxUrl")));
        const QString owned = currentState.value(QStringLiteral("renodxFile")).toString();
        if (!owned.isEmpty() && QFileInfo(owned).fileName() == owned) {
            const auto *game = m_games->game(row);
            const QString currentAddon = QFileInfo(game->exePath).absolutePath() + QStringLiteral("/reshade-addons/") + owned;
            QFile manifest(path + QStringLiteral("/tweak-manifest.json"));
            bool included = false;
            if (manifest.open(QIODevice::ReadOnly)) {
                for (const auto &entry : QJsonDocument::fromJson(manifest.readAll()).object().value(QStringLiteral("files")).toArray()) {
                    if (entry.toObject().value(QStringLiteral("path")).toString() == currentAddon)
                        included = true;
                }
            }
            if (!included && QFileInfo::exists(currentAddon)) ok = QFile::remove(currentAddon);
        }
        if (ok) ok = writeState(row, patch);
    }
    if (!ok) {
        if (error.isEmpty()) error = QStringLiteral("Could not update addon files or ownership metadata");
        QString rollbackError;
        const bool rolledBack = restoreRenoDxTweakSnapshot(row, rollbackDir, rollbackError);
        setStatus(QStringLiteral("Recovery failed: ") + error + (rolledBack ? QStringLiteral(". Current files restored.") : QStringLiteral(". Rollback failed: ") + rollbackError) + QStringLiteral(" Backup: ") + rollbackDir);
        m_games->refreshInstallState(row); return false;
    }
    m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Recovery completed. Previous setup preserved at: ") + rollbackDir);
    return true;
}

QString InstallerManager::setupFingerprint(int row) const {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game || game->exePath.isEmpty() || !QFileInfo::exists(game->exePath)) return {};
    QJsonObject state = readState(row);
    state.remove(QStringLiteral("lastBackupDir"));
    const QDir dir(QFileInfo(game->exePath).absolutePath());
    QStringList paths;
    for (const auto &file : dir.entryInfoList({QStringLiteral("*.dll"), QStringLiteral("*.ini"), QStringLiteral("*_fw_config.txt")}, QDir::Files, QDir::Name)) paths << file.absoluteFilePath();
    const QDir addons(dir.filePath(QStringLiteral("reshade-addons")));
    for (const auto &file : addons.entryInfoList({QStringLiteral("*.addon64"), QStringLiteral("*.addon32")}, QDir::Files, QDir::Name)) paths << file.absoluteFilePath();
    const auto baseline = state.value(QStringLiteral("renoDxTweakBaseline")).toObject();
    for (auto it = baseline.begin(); it != baseline.end(); ++it) paths << it.key();
    const QString engine = renoDxTweaksInfo(row).value(QStringLiteral("engineIniPath")).toString();
    if (!engine.isEmpty()) paths << engine;
    paths.removeDuplicates(); paths.sort();
    QJsonObject files;
    for (const QString &path : paths) {
        const auto fingerprint = tweakFileState(path);
        if (fingerprint.contains(QStringLiteral("error"))) return {};
        files.insert(path, fingerprint);
    }
    const QJsonObject setup{{QStringLiteral("state"), state}, {QStringLiteral("files"), files}, {QStringLiteral("exe"), game->exePath}, {QStringLiteral("prefix"), game->protonPrefix}};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(setup).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

QVariantMap InstallerManager::verificationInfo(int row) const {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game) return {};
    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    const QString key = QStringLiteral("verification/") + QString::fromLatin1(QUrl::toPercentEncoding(game->appId));
    const QVariantMap saved = settings.value(key).toMap();
    if (saved.isEmpty()) return {{QStringLiteral("text"), QStringLiteral("Not marked as tested")}};
    const QString current = setupFingerprint(row);
    const bool same = !current.isEmpty() && saved.value(QStringLiteral("fingerprint")).toString() == current;
    const QString date = saved.value(QStringLiteral("date")).toString();
    return {{QStringLiteral("text"), (same ? QStringLiteral("Verified working · ") : QStringLiteral("Needs retesting · last verified ")) + date}, {QStringLiteral("verified"), same}};
}

void InstallerManager::markVerified(int row) {
    if (m_busy) return;
    const auto *game = m_games ? m_games->game(row) : nullptr;
    const QString fingerprint = setupFingerprint(row);
    if (!game || fingerprint.isEmpty()) { setStatus(QStringLiteral("Cannot record verification: setup files are unavailable.")); return; }
    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    settings.setValue(QStringLiteral("verification/") + QString::fromLatin1(QUrl::toPercentEncoding(game->appId)), QVariantMap{{QStringLiteral("fingerprint"), fingerprint}, {QStringLiteral("date"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    settings.sync();
    setStatus(settings.status() == QSettings::NoError ? QStringLiteral("Marked this setup as verified working.") : QStringLiteral("Could not save verification."));
}
