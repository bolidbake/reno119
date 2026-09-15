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

QString InstallerManager::desiredRenoDxUrlForGame(const GameInfo &game) const {
    // Resolution order:
    //   1. Dedicated per-game snapshot from the live RenoDX Mods wiki.
    //   2. Generic Unreal/Unity addon fallback when no dedicated entry exists.
    QString url = m_catalog ? m_catalog->renoDxSnapshot(game.name) : QString();

    if (url.isEmpty()) {
        if (game.engine == QStringLiteral("Unreal Engine")) {
            if (game.architecture != QStringLiteral("x64"))
                return {};
            url = QStringLiteral("https://marat569.github.io/renodx/renodx-ue-extended.addon64");
        } else if (game.engine == QStringLiteral("Unity")) {
            url = game.architecture == QStringLiteral("x86")
                ? QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon32")
                : QStringLiteral("https://notvoosh.github.io/renodx-unity/renodx-unityengine.addon64");
        }
    }
    return url;
}

QString InstallerManager::desiredRenoDxUrlFor(int row) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    return g ? desiredRenoDxUrlForGame(*g) : QString();
}

void InstallerManager::installRenoDx(int row) {
    beginRenoDxInstall(row, false, QString());
}

void InstallerManager::installRenoDxConfirmed(int row, const QString &confirmedUrl) {
    beginRenoDxInstall(row, false, confirmedUrl);
}

void InstallerManager::installRenoDxOverExternal(int row) {
    beginRenoDxInstall(row, true, QString());
}

void InstallerManager::installRenoDxOverExternalConfirmed(int row, const QString &confirmedUrl) {
    beginRenoDxInstall(row, true, confirmedUrl);
}

void InstallerManager::beginRenoDxInstall(int row, bool allowExternalOverwrite, const QString &confirmedNonExactUrl) {
    if (m_busy) return;
    beginInlineOperation(row, QStringLiteral("renodx"));
    const auto *g = m_games->game(row);
    auto report = [this, row](const QString &text) {
        setRenoDxStatus(row, text);
        setStatus(text);
    };

    if (!g || g->exePath.isEmpty()) { report("No game executable detected."); return; }
    if (!g->reshadeInstalled && !g->reshade64Installed) { report("Install ReShade or ReShade64 with full add-on support first."); return; }

    if (g->renodxExternal && !allowExternalOverwrite) {
        report("Use Install over external RenoDX to back up and replace the existing addon.");
        return;
    }
    setRenoDxStatus(row, QStringLiteral("Resolving RenoDX addon for %1...").arg(g->name));

    if (!m_catalog->renoDxCatalogFinished()) {
        report("The RenoDX Mods catalog is still loading. Retry in a moment.");
        return;
    }

    const QString url = desiredRenoDxUrlFor(row);
    if (url.isEmpty()) {
        if (m_catalog->renoDxCatalogFinished() && !m_catalog->renoDxCatalogReady())
            report("The RenoDX Mods catalog could not be loaded, and no generic engine addon source is available for this game.");
        else
            report(QStringLiteral("No matching RenoDX addon was found for %1. Reno119 tried the exact title, subtitle-stripped title, a unique safe prefix match, and any Unreal/Unity generic fallback.").arg(g->name));
        return;
    }

    const QVariantMap catalogMatch = m_catalog ? m_catalog->renoDxSnapshotInfo(g->name) : QVariantMap{};
    const bool dedicatedMatch = !catalogMatch.value(QStringLiteral("url")).toString().isEmpty()
        && catalogMatch.value(QStringLiteral("url")).toString() == url;
    if (dedicatedMatch && catalogMatch.value(QStringLiteral("requiresConfirmation")).toBool()) {
        const QString matchedTitle = catalogMatch.value(QStringLiteral("title")).toString();
        const QString method = catalogMatch.value(QStringLiteral("method")).toString();
        if (confirmedNonExactUrl.isEmpty() || confirmedNonExactUrl != url) {
            report(QStringLiteral("Confirmation required before installing non-exact RenoDX match: %1 (%2). Review the current target before continuing.")
                       .arg(matchedTitle.isEmpty() ? QStringLiteral("unknown catalog title") : matchedTitle,
                            method.isEmpty() ? QStringLiteral("non-exact match") : method));
            return;
        }
    }

    const QString chosenFile = QFileInfo(QUrl(url).path()).fileName();
    setRenoDxStatus(row, chosenFile.isEmpty()
        ? QStringLiteral("Matched a RenoDX addon. Starting download...")
        : QStringLiteral("Matched RenoDX addon: %1").arg(chosenFile));

    const QString backupDir = createBackup(row, QStringLiteral("before-renodx-install"));
    if (!backupDir.isEmpty())
        writeState(row, {{"lastBackupDir", backupDir}});
    setBusy(true); setProgress(0.05); setStatus("Downloading RenoDX addon...");
    downloadRenoDx(row, QUrl(url), allowExternalOverwrite);
}

void InstallerManager::downloadRenoDx(int row, const QUrl &url, bool allowExternalOverwrite) {
    const auto *g = m_games->game(row); if (!g) { setBusy(false); return; }
    auto report = [this, row](const QString &text) {
        setRenoDxStatus(row, text);
        setStatus(text);
    };

    const QString fileName = QFileInfo(url.path()).fileName();
    if (!(fileName.endsWith(".addon64", Qt::CaseInsensitive) || fileName.endsWith(".addon32", Qt::CaseInsensitive))) {
        report("Refusing RenoDX URL with an unexpected file extension."); setBusy(false); return;
    }
    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString destination = exeDir + "/reshade-addons/" + fileName;
    const QString currentlyOwned = readState(row).value("renodxFile").toString();
    if (!allowExternalOverwrite && QFileInfo::exists(destination) && currentlyOwned != fileName) {
        report("Refusing to overwrite existing unmanaged RenoDX addon: " + fileName);
        setBusy(false);
        return;
    }
    const QString gameId = g->appId;
    const QString selectedExe = g->exePath;
    QDir addons(exeDir + QStringLiteral("/reshade-addons"));
    const QStringList externalNames = addons.entryList({QStringLiteral("*renodx*.addon64"), QStringLiteral("*renodx*.addon32")}, QDir::Files, QDir::Name);
    QStringList targets{destination, exeDir + QStringLiteral("/ReShade.ini"), exeDir + QStringLiteral("/.reno119-state.json")};
    if (!currentlyOwned.isEmpty()) targets << addons.filePath(currentlyOwned);
    if (allowExternalOverwrite)
        for (const QString &name : externalNames) targets << addons.filePath(name);
    targets.removeDuplicates();
    QJsonObject beforeDownload;
    for (const QString &path : targets) {
        const auto fingerprint = tweakFileState(path);
        if (fingerprint.contains(QStringLiteral("error"))) {
            report(fingerprint.value(QStringLiteral("error")).toString());
            setBusy(false);
            return;
        }
        beforeDownload.insert(path, fingerprint);
    }
    setRenoDxStatus(row, QStringLiteral("Downloading %1...").arg(fileName));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Reno119/" RENO119_VERSION);
    auto *reply = m_net.get(req);
    auto bytes = QSharedPointer<QByteArray>::create();
    connect(reply, &QNetworkReply::readyRead, this, [reply, bytes] { bytes->append(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 done, qint64 total) {
        if (total > 0) setProgress(0.08 + 0.82 * (double(done) / double(total)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, row, fileName, url, allowExternalOverwrite, gameId, selectedExe, targets, beforeDownload, externalNames] {
        bytes->append(reply->readAll());
        auto reportFinished = [this, row](const QString &text) {
            setRenoDxStatus(row, text);
            setStatus(text);
        };
        if (reply->error() != QNetworkReply::NoError) {
            reportFinished("RenoDX download failed: " + reply->errorString()); reply->deleteLater(); setBusy(false); return;
        }
        reply->deleteLater();
        if (bytes->size() < 2 || bytes->left(2) != "MZ") {
            reportFinished("RenoDX download did not contain a valid PE addon."); setBusy(false); return;
        }
        const auto *g = m_games->game(row); if (!g) { setBusy(false); return; }
        if (g->appId != gameId || g->exePath != selectedExe) {
            reportFinished("The selected game changed during download. Retry the installation.");
            setBusy(false); return;
        }
        const QString exeDir = QFileInfo(g->exePath).absolutePath();
        QDir addons(exeDir + QStringLiteral("/reshade-addons"));
        const auto unchanged = [&]() {
            if (addons.entryList({QStringLiteral("*renodx*.addon64"), QStringLiteral("*renodx*.addon32")}, QDir::Files, QDir::Name) != externalNames) return false;
            for (const QString &path : targets)
                if (tweakFileState(path) != beforeDownload.value(path).toObject()) return false;
            return true;
        };
        if (!unchanged()) {
            reportFinished("RenoDX files or configuration changed during download. Retry to review the current installation.");
            setBusy(false); return;
        }
        QString recoveryDir, error;
        if (!createRenoDxTweakSnapshot(row, targets, recoveryDir, error)) {
            reportFinished("Could not create the required RenoDX safety backup: " + error);
            setBusy(false); return;
        }
        if (!unchanged()) {
            QDir(recoveryDir).removeRecursively();
            reportFinished("Files changed while backing up RenoDX. Retry the installation.");
            setBusy(false); return;
        }
        const auto fail = [&](const QString &message) {
            QString restoreError;
            const bool restored = restoreRenoDxTweakSnapshot(row, recoveryDir, restoreError);
            reportFinished(message + (restored ? QStringLiteral(" Previous files restored.") : QStringLiteral(" Rollback failed: ") + restoreError)
                           + QStringLiteral(" Recovery copy: ") + recoveryDir);
            m_games->refreshInstallState(row);
            setBusy(false);
        };
        if (!QDir().mkpath(addons.absolutePath())) { fail("Could not create the addon directory."); return; }
        const QString dst = addons.filePath(fileName);
        QSaveFile out(dst);
        if (!out.open(QIODevice::WriteOnly) || out.write(*bytes) != bytes->size() || !out.commit()) {
            out.cancelWriting();
            fail("Could not write RenoDX addon to " + dst); return;
        }
        QStringList obsolete;
        if (allowExternalOverwrite) obsolete = externalNames;
        const QString oldOwned = readState(row).value("renodxFile").toString();
        if (!oldOwned.isEmpty()) obsolete << oldOwned;
        obsolete.removeDuplicates();
        for (const QString &name : obsolete) {
            if (name != fileName && QFileInfo::exists(addons.filePath(name)) && !QFile::remove(addons.filePath(name))) {
                fail("Could not replace previous RenoDX addon: " + name); return;
            }
        }
        if (!ensureReShadeIni(exeDir)) { fail("Could not configure the ReShade addon directory."); return; }
        if (!writeState(row, {{"renodxFile", fileName}, {"renodxUrl", url.toString()}})) {
            fail("Could not save RenoDX ownership metadata."); return;
        }
        m_games->refreshInstallState(row);
        const QVariantMap tweakInfo = renoDxTweaksInfo(row);
        const QString tweakSuffix = tweakInfo.value(QStringLiteral("available")).toBool()
            ? QStringLiteral(" A managed tweak profile is available below.")
            : QString();
        m_activeUpdateSucceeded = true;
        setProgress(1.0); reportFinished("RenoDX addon installed: " + fileName + "." + tweakSuffix + QStringLiteral(" Recovery copy: ") + recoveryDir); setBusy(false);
    });
}

void InstallerManager::uninstallRenoDx(int row) {
    if (m_busy) return;
    beginInlineOperation(row, QStringLiteral("renodx"));
    const auto *g = m_games->game(row); if (!g) return;
    auto report = [this, row](const QString &text) {
        setRenoDxStatus(row, text);
        setStatus(text);
    };
    const QString backupDir = createBackup(row, QStringLiteral("before-renodx-remove"));
    if (!backupDir.isEmpty())
        writeState(row, {{"lastBackupDir", backupDir}});
    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const auto state = readState(row);
    const QString ownedAddon = state.value("renodxFile").toString();
    if (ownedAddon.isEmpty()) {
        report("No Reno119-managed RenoDX addon was found.");
        return;
    }
    const QString addonPath = exeDir + "/reshade-addons/" + ownedAddon;
    const bool removed = !QFileInfo::exists(addonPath) || QFile::remove(addonPath);
    if (!removed) {
        report("Could not remove " + ownedAddon);
        return;
    }
    const bool tweaksManaged = !state.value(QStringLiteral("renoDxTweakSnapshotDir")).toString().isEmpty();
    writeState(row, {{"renodxFile", QJsonValue()}, {"renodxUrl", QJsonValue()}});
    m_games->refreshInstallState(row);
    report("Removed Reno119-managed RenoDX addon: " + ownedAddon +
           (tweaksManaged ? QStringLiteral(". Managed Engine.ini/ReShade.ini tweaks were preserved; use Restore original if you want to undo them.") : QString()));
}


QVariantMap InstallerManager::buildRenoDxTweakPlan(const GameInfo &game) const {
    QVariantMap plan;
    plan.insert(QStringLiteral("id"), QString());
    plan.insert(QStringLiteral("title"), QStringLiteral("No managed tweaks"));
    plan.insert(QStringLiteral("description"), QString());
    plan.insert(QStringLiteral("source"), QString());
    plan.insert(QStringLiteral("sourceUrl"), QString());
    plan.insert(QStringLiteral("engineReadOnly"), false);
    plan.insert(QStringLiteral("engineChanges"), QVariantList{});
    plan.insert(QStringLiteral("reshadeChanges"), QVariantList{});
    plan.insert(QStringLiteral("engineRelativeCandidates"), QStringList{});
    plan.insert(QStringLiteral("enginePathOverride"), QString());
    plan.insert(QStringLiteral("discoverEngineIni"), false);
    plan.insert(QStringLiteral("profilePending"), false);

    auto change = [](const QString &section, const QString &key, const QString &value) {
        return QVariantMap{{QStringLiteral("section"), section},
                           {QStringLiteral("key"), key},
                           {QStringLiteral("value"), value}};
    };

    QVariantList engineChanges;
    QVariantList reshadeChanges;

    // Curated bespoke profiles are intentionally narrow. A named RenoDX addon
    // must not inherit generic UE-Extended HDR CVars just because it uses UE.
    if (game.appId == QStringLiteral("2947440") ||
        game.name.compare(QStringLiteral("SILENT HILL f"), Qt::CaseInsensitive) == 0 ||
        game.name.compare(QStringLiteral("Silent Hill f"), Qt::CaseInsensitive) == 0) {
        plan[QStringLiteral("id")] = QStringLiteral("bespoke-silenthillf-lut");
        plan[QStringLiteral("title")] = QStringLiteral("Silent Hill f — real-time RenoDX controls");
        plan[QStringLiteral("description")] = QStringLiteral("Keeps the game's LUT builder updating so RenoDX tone-mapping and color-grading controls apply immediately.");
        plan[QStringLiteral("source")] = QStringLiteral("RenoDX mod instructions (MusaQH)");
        plan[QStringLiteral("sourceUrl")] = QStringLiteral("https://www.nexusmods.com/silenthillf/mods/64");
        plan[QStringLiteral("engineReadOnly")] = true;
        plan[QStringLiteral("engineRelativeCandidates")] = QStringList{
            QStringLiteral("SHf/Saved/Config/Windows/Engine.ini")
        };
        engineChanges << change(QStringLiteral("/Script/Engine.RendererSettings"),
                                QStringLiteral("r.LUT.UpdateEveryFrame"), QStringLiteral("1"));
    }

    // RHI's live manifest contains a small set of per-game [renodx]
    // ReShade.ini overrides. These can safely augment bespoke profiles.
    if (m_catalog) {
        const QVariantMap overrides = m_catalog->rhiRenoDxIniOverrides(game.name);
        for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
            reshadeChanges << change(QStringLiteral("renodx"), it.key(), it.value().toString());
    }

    const bool dedicatedSnapshot = m_catalog && !m_catalog->renoDxSnapshot(game.name).isEmpty();
    const bool unrealCandidate = game.engine == QStringLiteral("Unreal Engine") &&
                                 game.architecture == QStringLiteral("x64") &&
                                 engineChanges.isEmpty();
    const bool renoDxCatalogPending = m_catalog && !m_catalog->renoDxCatalogFinished();
    const bool renoDxCatalogReady = m_catalog && m_catalog->renoDxCatalogReady();
    const QVariantMap rhiCompatibility = m_catalog ? m_catalog->rhiUeExtendedCompatibility(game.name) : QVariantMap{};
    const bool forceRhiUeExtended = m_catalog &&
        (m_catalog->rhiNativeHdrGame(game.name) || !rhiCompatibility.isEmpty() ||
         !m_catalog->rhiEngineIniProfileFile(game.name).isEmpty());
    const bool genericUnreal = unrealCandidate && !renoDxCatalogPending &&
                               (forceRhiUeExtended || (renoDxCatalogReady && !dedicatedSnapshot));

    if (unrealCandidate && renoDxCatalogPending) {
        plan[QStringLiteral("id")] = QStringLiteral("renodx-catalog-pending");
        plan[QStringLiteral("title")] = QStringLiteral("Checking RenoDX game profile…");
        plan[QStringLiteral("description")] = QStringLiteral("Reno119 is waiting for the RenoDX Mods catalog so it can distinguish a bespoke addon from the generic UE-Extended fallback before offering Engine.ini changes.");
        plan[QStringLiteral("source")] = QStringLiteral("RenoDX Mods catalog");
        plan[QStringLiteral("profilePending")] = true;
    }

    // For generic UE-Extended only, follow RHI's live profile data. Do not
    // offer the generic keys until that manifest has been checked, because a
    // game may have a narrower per-game engine file specifically to avoid the
    // standard set.
    if (genericUnreal && engineChanges.isEmpty() && m_catalog && !m_catalog->rhiManifestFinished()) {
        plan[QStringLiteral("id")] = QStringLiteral("rhi-generic-pending");
        plan[QStringLiteral("title")] = QStringLiteral("Checking RenoDX tweak profile…");
        plan[QStringLiteral("description")] = QStringLiteral("Reno119 is checking RHI's live manifest for per-game Engine.ini exceptions before offering the generic UE-Extended profile.");
        plan[QStringLiteral("source")] = QStringLiteral("RHI live manifest");
        plan[QStringLiteral("sourceUrl")] = QStringLiteral("https://github.com/RankFTW/RHI/blob/main/manifest.json");
        plan[QStringLiteral("profilePending")] = true;
    } else if (genericUnreal && engineChanges.isEmpty() && m_catalog && m_catalog->rhiManifestReady()) {
        plan[QStringLiteral("enginePathOverride")] = m_catalog->rhiEngineIniPathOverride(game.name);
        const QString customFile = m_catalog->rhiEngineIniProfileFile(game.name);
        const QString customText = m_catalog->rhiEngineIniProfileText(game.name);
        if (!customFile.isEmpty()) {
            plan[QStringLiteral("id")] = QStringLiteral("rhi-custom-") + customFile;
            plan[QStringLiteral("title")] = QStringLiteral("RHI custom UE-Extended profile");
            plan[QStringLiteral("description")] = QStringLiteral("Uses the per-game Engine.ini profile published by RHI instead of Reno119's generic Unreal HDR set.");
            plan[QStringLiteral("source")] = QStringLiteral("RHI live manifest / engine-files");
            plan[QStringLiteral("sourceUrl")] = QStringLiteral("https://github.com/RankFTW/RHI/tree/main/engine-files");
            plan[QStringLiteral("engineReadOnly")] = true;
            plan[QStringLiteral("discoverEngineIni")] = true;
            if (customText.isEmpty()) {
                plan[QStringLiteral("profilePending")] = true;
            } else {
                QString section;
                const QStringList lines = customText.split(QRegularExpression(QStringLiteral("\\r?\\n")));
                for (const QString &raw : lines) {
                    const QString line = raw.trimmed();
                    if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
                        continue;
                    if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                        section = line.mid(1, line.size() - 2).trimmed();
                        continue;
                    }
                    const qsizetype equals = line.indexOf(QLatin1Char('='));
                    if (equals <= 0 || section.isEmpty())
                        continue;
                    engineChanges << change(section, line.left(equals).trimmed(), line.mid(equals + 1).trimmed());
                }
            }
        } else {
            const QVariantMap compatibility = rhiCompatibility;
            const bool hdrEnabled = !compatibility.contains(QStringLiteral("hdr")) || compatibility.value(QStringLiteral("hdr")).toBool();
            const bool lutEnabled = !compatibility.contains(QStringLiteral("lut")) || compatibility.value(QStringLiteral("lut")).toBool();

            plan[QStringLiteral("id")] = QStringLiteral("rhi-generic-ue-extended");
            plan[QStringLiteral("title")] = QStringLiteral("Generic UE-Extended profile");
            plan[QStringLiteral("description")] = QStringLiteral("Applies the Unreal Engine settings used for RenoDX UE-Extended when no dedicated game addon exists.");
            plan[QStringLiteral("source")] = QStringLiteral("RHI UE-Extended configuration");
            plan[QStringLiteral("sourceUrl")] = QStringLiteral("https://github.com/RankFTW/RHI");
            plan[QStringLiteral("engineReadOnly")] = true;
            plan[QStringLiteral("discoverEngineIni")] = true;

            if (hdrEnabled) {
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.AllowHDR"), QStringLiteral("1"));
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.HDR.EnableHDROutput"), QStringLiteral("1"));
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.HDR.Display.OutputDevice"), QStringLiteral("5"));
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.HDR.Display.ColorGamut"), QStringLiteral("2"));
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.HDR.UI.CompositeMode"), QStringLiteral("1"));
                engineChanges << change(QStringLiteral("SystemSettings"), QStringLiteral("r.HDR.UI.Level"), QStringLiteral("1.5"));
            }
            if (lutEnabled) {
                engineChanges << change(QStringLiteral("/Script/Engine.RendererSettings"),
                                        QStringLiteral("r.LUT.UpdateEveryFrame"), QStringLiteral("1"));
            }
        }
    }

    if (plan.value(QStringLiteral("id")).toString().isEmpty() && !reshadeChanges.isEmpty()) {
        plan[QStringLiteral("id")] = QStringLiteral("rhi-renodx-ini-overrides");
        plan[QStringLiteral("title")] = QStringLiteral("RenoDX compatibility settings");
        plan[QStringLiteral("description")] = QStringLiteral("Applies the per-game [renodx] values published in RHI's live compatibility manifest.");
        plan[QStringLiteral("source")] = QStringLiteral("RHI live manifest");
        plan[QStringLiteral("sourceUrl")] = QStringLiteral("https://github.com/RankFTW/RHI/blob/main/manifest.json");
    }

    plan[QStringLiteral("engineChanges")] = engineChanges;
    plan[QStringLiteral("reshadeChanges")] = reshadeChanges;
    plan[QStringLiteral("available")] = !plan.value(QStringLiteral("id")).toString().isEmpty() &&
                                        (!engineChanges.isEmpty() || !reshadeChanges.isEmpty() || plan.value(QStringLiteral("profilePending")).toBool());
    return plan;
}

QString InstallerManager::resolveEngineIniPath(const GameInfo &game, const QVariantMap &plan, QStringList *candidates) const {
    if (candidates)
        candidates->clear();
    if (game.protonPrefix.trimmed().isEmpty())
        return {};

    const QString usersRoot = QDir(game.protonPrefix).filePath(QStringLiteral("drive_c/users"));
    QDir users(usersRoot);
    if (!users.exists())
        return {};

    QStringList localRoots;
    const QFileInfoList userDirs = users.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &userDir : userDirs) {
        const QString local = QDir(userDir.absoluteFilePath()).filePath(QStringLiteral("AppData/Local"));
        if (QFileInfo(local).isDir())
            localRoots << QDir::cleanPath(local);
    }
    const QString preferred = QDir(usersRoot).filePath(QStringLiteral("steamuser/AppData/Local"));
    if (QFileInfo(preferred).isDir()) {
        localRoots.removeAll(QDir::cleanPath(preferred));
        localRoots.prepend(QDir::cleanPath(preferred));
    }
    localRoots.removeDuplicates();
    if (localRoots.isEmpty())
        return {};

    // RHI can publish an explicit project/config override for games whose
    // Unreal config is not under the obvious project-name path. Translate
    // the Windows variables into the selected Proton prefix instead of
    // treating them as host paths. Pipe-separated alternatives are retained
    // and only a single existing config directory is accepted.
    const QString pathOverride = plan.value(QStringLiteral("enginePathOverride")).toString().trimmed();
    if (!pathOverride.isEmpty()) {
        QStringList overrideTargets;
        const QStringList platformNames{QStringLiteral("Windows"), QStringLiteral("WindowsNoEditor"),
                                        QStringLiteral("WindowsClient"), QStringLiteral("WinGDK")};
        const QStringList specs = pathOverride.split(QLatin1Char('|'), Qt::SkipEmptyParts);

        auto addTargetIfParentExists = [&](QString target) {
            target.replace(QLatin1Char('\\'), QLatin1Char('/'));
            target = QDir::cleanPath(target);
            if (!target.endsWith(QStringLiteral("Engine.ini"), Qt::CaseInsensitive))
                target = QDir(target).filePath(QStringLiteral("Engine.ini"));
            if (QFileInfo(QFileInfo(target).absolutePath()).isDir())
                overrideTargets << target;
        };

        for (QString spec : specs) {
            spec = spec.trimmed();
            spec.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (spec.startsWith(QStringLiteral("%LOCALAPPDATA%/"), Qt::CaseInsensitive)) {
                const QString relative = spec.mid(QStringLiteral("%LOCALAPPDATA%/").size());
                for (const QString &localRoot : std::as_const(localRoots))
                    addTargetIfParentExists(QDir(localRoot).filePath(relative));
                continue;
            }
            if (spec.startsWith(QStringLiteral("%USERPROFILE%/"), Qt::CaseInsensitive)) {
                const QString relative = spec.mid(QStringLiteral("%USERPROFILE%/").size());
                for (const QFileInfo &userDir : userDirs)
                    addTargetIfParentExists(QDir(userDir.absoluteFilePath()).filePath(relative));
                continue;
            }
            if (spec.size() > 3 && spec.at(1) == QLatin1Char(':') &&
                spec.at(2) == QLatin1Char('/')) {
                addTargetIfParentExists(QDir(game.protonPrefix).filePath(QStringLiteral("drive_c/") + spec.mid(3)));
                continue;
            }

            // A bare RHI override is an Unreal project name under LocalAppData.
            // If a relative subpath is ever supplied, preserve it before the
            // conventional Saved/Config/<platform> suffix.
            for (const QString &localRoot : std::as_const(localRoots)) {
                const QString projectRoot = QDir(localRoot).filePath(spec);
                for (const QString &platform : platformNames)
                    addTargetIfParentExists(QDir(projectRoot).filePath(QStringLiteral("Saved/Config/") + platform));
            }
        }

        overrideTargets.removeDuplicates();
        if (candidates)
            *candidates = overrideTargets;
        if (overrideTargets.size() == 1)
            return overrideTargets.first();
        // An explicit upstream override is authoritative; do not ignore it and
        // guess a different project directory from the rest of the prefix.
        return {};
    }

    const QStringList relatives = plan.value(QStringLiteral("engineRelativeCandidates")).toStringList();
    for (const QString &localRoot : std::as_const(localRoots)) {
        for (const QString &relative : relatives) {
            const QString target = QDir::cleanPath(QDir(localRoot).filePath(relative));
            const QString parent = QFileInfo(target).absolutePath();
            if (QFileInfo(parent).isDir()) {
                if (candidates)
                    candidates->append(target);
                return target;
            }
        }
    }

    if (!plan.value(QStringLiteral("discoverEngineIni")).toBool())
        return {};

    QStringList found;
    for (const QString &localRoot : std::as_const(localRoots)) {
        QDirIterator it(localRoot, QStringList{QStringLiteral("Engine.ini")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QDir::cleanPath(it.next());
            const QString normalized = path.toLower();
            if (!normalized.contains(QStringLiteral("/saved/config/")) &&
                !normalized.contains(QStringLiteral("/saved_global/config/")))
                continue;
            if (!(normalized.contains(QStringLiteral("/windows/engine.ini")) ||
                  normalized.contains(QStringLiteral("/windowsnoeditor/engine.ini")) ||
                  normalized.contains(QStringLiteral("/windowsclient/engine.ini")) ||
                  normalized.contains(QStringLiteral("/wingdk/engine.ini"))))
                continue;
            found << path;
        }
    }
    found.removeDuplicates();

    // A freshly launched game can have a valid Saved/Config platform folder
    // without Engine.ini itself. In that case we can safely create the file
    // only when exactly one plausible Unreal config folder exists.
    if (found.isEmpty()) {
        QStringList configTargets;
        const QStringList platformNames{QStringLiteral("windows"), QStringLiteral("windowsnoeditor"),
                                        QStringLiteral("windowsclient"), QStringLiteral("wingdk")};
        for (const QString &localRoot : std::as_const(localRoots)) {
            QDirIterator dirs(localRoot, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirs.hasNext()) {
                const QString dirPath = QDir::cleanPath(dirs.next());
                const QFileInfo dirInfo(dirPath);
                if (!platformNames.contains(dirInfo.fileName().toLower()))
                    continue;
                if (dirInfo.dir().dirName().compare(QStringLiteral("Config"), Qt::CaseInsensitive) != 0)
                    continue;
                const QString lower = dirPath.toLower();
                if (!lower.contains(QStringLiteral("/saved/config/")) &&
                    !lower.contains(QStringLiteral("/saved_global/config/")))
                    continue;
                configTargets << QDir(dirPath).filePath(QStringLiteral("Engine.ini"));
            }
        }
        configTargets.removeDuplicates();
        found = configTargets;
    }

    if (candidates)
        *candidates = found;
    return found.size() == 1 ? found.first() : QString();
}

bool InstallerManager::mergeIniChanges(const QString &path, const QVariantList &changes, QString &error) const {
    error.clear();
    QFile input(path);
    QString text;
    QFileDevice::Permissions originalPermissions{};
    const bool existed = QFileInfo::exists(path);
    if (existed) {
        originalPermissions = QFile::permissions(path);
        if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
            error = QStringLiteral("Could not read %1").arg(path);
            return false;
        }
        text = QString::fromUtf8(input.readAll());
        input.close();
    }

    const QByteArray bytes = mergedTweakIni(text, changes);

    QDir().mkpath(QFileInfo(path).absolutePath());
    if (existed)
        QFile::setPermissions(path, originalPermissions | QFileDevice::WriteOwner);
    QSaveFile out(path);
    out.setDirectWriteFallback(true);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (existed)
            QFile::setPermissions(path, originalPermissions);
        error = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    if (out.write(bytes) != bytes.size() || !out.commit()) {
        if (existed)
            QFile::setPermissions(path, originalPermissions);
        error = QStringLiteral("Could not commit %1").arg(path);
        return false;
    }
    if (existed && !QFile::setPermissions(path, originalPermissions)) {
        error = QStringLiteral("Could not preserve permissions for %1").arg(path);
        return false;
    }
    return true;
}

bool InstallerManager::createRenoDxTweakSnapshot(int row, const QStringList &targets, QString &snapshotDir, QString &error) {
    error.clear();
    snapshotDir.clear();
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return false;

    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss-zzz"));
    snapshotDir = backupsBaseDir() + QLatin1Char('/') + g->appId + QLatin1Char('/') + stamp + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral("-renodx-tweaks");
    if (!QDir().mkpath(snapshotDir)) {
        error = QStringLiteral("Could not create tweak backup directory.");
        return false;
    }

    QJsonArray entries;
    int index = 0;
    for (const QString &target : targets) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), target);
        const QFileInfo info(target);
        entry.insert(QStringLiteral("existed"), info.exists());
        entry.insert(QStringLiteral("permissions"),
                     info.exists() ? static_cast<int>(QFile::permissions(target).toInt()) : 0);
        if (info.exists()) {
            const QString backupName = QStringLiteral("%1.bin").arg(index++);
            if (!copyFileEnsuringParent(target, snapshotDir + QLatin1Char('/') + backupName)) {
                error = QStringLiteral("Could not back up %1").arg(target);
                QDir(snapshotDir).removeRecursively();
                snapshotDir.clear();
                return false;
            }
            entry.insert(QStringLiteral("backup"), backupName);
        }
        entries.append(entry);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("version"), 1);
    manifest.insert(QStringLiteral("files"), entries);
    QSaveFile out(snapshotDir + QStringLiteral("/tweak-manifest.json"));
    out.setDirectWriteFallback(true);
    if (!out.open(QIODevice::WriteOnly) ||
        out.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0 || !out.commit()) {
        error = QStringLiteral("Could not write tweak backup manifest.");
        QDir(snapshotDir).removeRecursively();
        snapshotDir.clear();
        return false;
    }
    return true;
}

bool InstallerManager::restoreRenoDxTweakSnapshot(int, const QString &snapshotDir, QString &error) {
    error.clear();
    QFile manifestFile(snapshotDir + QStringLiteral("/tweak-manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Tweak backup manifest is missing.");
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    if (!doc.isObject()) {
        error = QStringLiteral("Tweak backup manifest is invalid.");
        return false;
    }

    const QJsonArray entries = doc.object().value(QStringLiteral("files")).toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString target = entry.value(QStringLiteral("path")).toString();
        const bool existed = entry.value(QStringLiteral("existed")).toBool();
        if (target.isEmpty())
            continue;

        if (QFileInfo::exists(target))
            QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner);

        if (!existed) {
            if (QFileInfo::exists(target) && !QFile::remove(target)) {
                error = QStringLiteral("Could not remove Reno119-created file %1").arg(target);
                return false;
            }
            continue;
        }

        const QString backup = snapshotDir + QLatin1Char('/') + entry.value(QStringLiteral("backup")).toString();
        if (!QFileInfo::exists(backup)) {
            error = QStringLiteral("Backup data is missing for %1").arg(target);
            return false;
        }
        if (!copyFileEnsuringParent(backup, target)) {
            error = QStringLiteral("Could not restore %1").arg(target);
            return false;
        }
        if (!QFile::setPermissions(target, QFileDevice::Permissions::fromInt(entry.value(QStringLiteral("permissions")).toInt()))) {
            error = QStringLiteral("Could not restore permissions for %1").arg(target);
            return false;
        }
    }
    return true;
}

QVariantMap InstallerManager::renoDxTweaksInfo(int row) const {
    QVariantMap out;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) {
        out[QStringLiteral("available")] = false;
        return out;
    }

    const QVariantMap plan = buildRenoDxTweakPlan(*g);
    const QVariantList engineChanges = plan.value(QStringLiteral("engineChanges")).toList();
    const QVariantList reshadeChanges = plan.value(QStringLiteral("reshadeChanges")).toList();
    QStringList candidates;
    const QString enginePath = engineChanges.isEmpty() ? QString() : resolveEngineIniPath(*g, plan, &candidates);
    const QString reshadePath = reshadeChanges.isEmpty() || g->exePath.isEmpty()
        ? QString() : QFileInfo(g->exePath).absolutePath() + QStringLiteral("/ReShade.ini");

    const QJsonObject state = readState(row);
    const QString activeSnapshot = state.value(QStringLiteral("renoDxTweakSnapshotDir")).toString();
    const QString activeProfile = state.value(QStringLiteral("renoDxTweakProfileId")).toString();

    QStringList summary;
    for (const QVariant &entry : engineChanges) {
        const QVariantMap c = entry.toMap();
        summary << QStringLiteral("Engine.ini · [%1] %2=%3")
                       .arg(c.value(QStringLiteral("section")).toString(),
                            c.value(QStringLiteral("key")).toString(),
                            c.value(QStringLiteral("value")).toString());
    }
    for (const QVariant &entry : reshadeChanges) {
        const QVariantMap c = entry.toMap();
        summary << QStringLiteral("ReShade.ini · [%1] %2=%3")
                       .arg(c.value(QStringLiteral("section")).toString(),
                            c.value(QStringLiteral("key")).toString(),
                            c.value(QStringLiteral("value")).toString());
    }

    QString engineStatus;
    if (!engineChanges.isEmpty()) {
        if (g->protonPrefix.trimmed().isEmpty())
            engineStatus = QStringLiteral("No Proton/Wine prefix is configured for this entry.");
        else if (!enginePath.isEmpty())
            engineStatus = QFileInfo::exists(enginePath)
                ? QStringLiteral("Engine.ini detected")
                : QStringLiteral("Config folder detected; Engine.ini will be created");
        else if (candidates.size() > 1)
            engineStatus = QStringLiteral("Multiple Engine.ini files were found; Reno119 will not guess which one belongs to this game.");
        else
            engineStatus = QStringLiteral("Unreal config folder not found. Launch the game once, then refresh.");
    }

    const bool pending = plan.value(QStringLiteral("profilePending")).toBool();
    const bool available = plan.value(QStringLiteral("available")).toBool();
    const bool canApply = available && !pending && g->renodxInstalled &&
                          (engineChanges.isEmpty() || !enginePath.isEmpty()) &&
                          (reshadeChanges.isEmpty() || !reshadePath.isEmpty());
    QString actionStatus;
    if (available && !pending && !g->renodxInstalled)
        actionStatus = QStringLiteral("Install RenoDX first; the profile will not be written to the game until the addon is present.");

    const QJsonObject appliedSettings = state.value(QStringLiteral("renoDxAppliedProfileSettings")).toObject();
    out[QStringLiteral("profileChanged")] = !activeSnapshot.isEmpty() && !appliedSettings.isEmpty() && !pending && available && appliedSettings != tweakProfileSettings(plan);
    out[QStringLiteral("profileComparisonUnknown")] = !activeSnapshot.isEmpty() && appliedSettings.isEmpty();
    QStringList profileChanges;
    if (out.value(QStringLiteral("profileChanged")).toBool()) {
        const auto values = [](const QJsonObject &profile) {
            QMap<QString, QString> result;
            for (const QString &group : {QStringLiteral("engineChanges"), QStringLiteral("reshadeChanges")}) {
                for (const auto &value : profile.value(group).toArray()) {
                    const auto change = value.toObject();
                    const QString name = (group == QStringLiteral("engineChanges") ? QStringLiteral("Engine.ini") : QStringLiteral("ReShade.ini"))
                        + QStringLiteral(" [") + change.value(QStringLiteral("section")).toString() + QStringLiteral("] ") + change.value(QStringLiteral("key")).toString();
                    result.insert(name, change.value(QStringLiteral("value")).toString());
                }
            }
            result.insert(QStringLiteral("Engine.ini read-only"), profile.value(QStringLiteral("engineReadOnly")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"));
            return result;
        };
        const auto before = values(appliedSettings);
        const auto after = values(tweakProfileSettings(plan));
        for (auto it = after.begin(); it != after.end(); ++it) {
            if (!before.contains(it.key())) profileChanges << QStringLiteral("Added: ") + it.key() + QStringLiteral(" = ") + it.value();
            else if (before.value(it.key()) != it.value()) profileChanges << it.key() + QStringLiteral(": ") + before.value(it.key()) + QStringLiteral(" → ") + it.value();
        }
        for (auto it = before.begin(); it != before.end(); ++it)
            if (!after.contains(it.key())) profileChanges << QStringLiteral("Removed from profile: ") + it.key() + QStringLiteral(" (existing INI value is kept)");
        if (profileChanges.isEmpty()) profileChanges << QStringLiteral("Profile identity or setting order changed.");
    }
    out[QStringLiteral("profileChangeSummary")] = profileChanges;
    out[QStringLiteral("available")] = available;
    out[QStringLiteral("profilePending")] = pending;
    out[QStringLiteral("canApply")] = canApply;
    out[QStringLiteral("canRestore")] = !activeSnapshot.isEmpty();
    out[QStringLiteral("applied")] = !activeSnapshot.isEmpty() && activeProfile == plan.value(QStringLiteral("id")).toString();
    out[QStringLiteral("profileId")] = plan.value(QStringLiteral("id"));
    out[QStringLiteral("title")] = plan.value(QStringLiteral("title"));
    out[QStringLiteral("description")] = plan.value(QStringLiteral("description"));
    out[QStringLiteral("source")] = plan.value(QStringLiteral("source"));
    out[QStringLiteral("sourceUrl")] = plan.value(QStringLiteral("sourceUrl"));
    out[QStringLiteral("engineIniPath")] = enginePath;
    out[QStringLiteral("engineStatus")] = engineStatus;
    out[QStringLiteral("actionStatus")] = actionStatus;
    out[QStringLiteral("changes")] = summary;
    out[QStringLiteral("rhiManifestReady")] = m_catalog && m_catalog->rhiManifestReady();
    out[QStringLiteral("rhiManifestFinished")] = m_catalog && m_catalog->rhiManifestFinished();
    return out;
}

QVariantMap InstallerManager::previewRenoDxTweaks(int row, bool restoring) const {
    QVariantMap result;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) return result;
    const QJsonObject state = readState(row);
    const QVariantMap plan = buildRenoDxTweakPlan(*g);
    const QVariantMap info = renoDxTweaksInfo(row);
    const QString snapshot = state.value(QStringLiteral("renoDxTweakSnapshotDir")).toString();
    QStringList paths;
    QJsonArray originals;
    QString error;
    if (!snapshot.isEmpty()) {
        QFile manifest(snapshot + QStringLiteral("/tweak-manifest.json"));
        if (!manifest.open(QIODevice::ReadOnly)) error = QStringLiteral("Cannot read the original backup manifest.");
        else {
            originals = QJsonDocument::fromJson(manifest.readAll()).object().value(QStringLiteral("files")).toArray();
            if (originals.isEmpty()) error = QStringLiteral("The original backup manifest is invalid.");
        }
    }
    if (restoring) {
        for (const auto &entry : originals) paths << entry.toObject().value(QStringLiteral("path")).toString();
    } else {
        if (!plan.value(QStringLiteral("engineChanges")).toList().isEmpty()) paths << info.value(QStringLiteral("engineIniPath")).toString();
        if (!plan.value(QStringLiteral("reshadeChanges")).toList().isEmpty()) paths << QFileInfo(g->exePath).absolutePath() + QStringLiteral("/ReShade.ini");
        if (!snapshot.isEmpty()) {
            QStringList oldPaths;
            for (const auto &entry : originals) oldPaths << entry.toObject().value(QStringLiteral("path")).toString();
            if (QSet<QString>(paths.begin(), paths.end()) != QSet<QString>(oldPaths.begin(), oldPaths.end()))
                error = QStringLiteral("The profile's target files changed. Restore the original profile before applying this one.");
        }
    }
    QJsonObject current;
    QStringList sections;
    QStringList changed;
    const QJsonObject baseline = state.value(QStringLiteral("renoDxTweakBaseline")).toObject();
    const bool unknown = !snapshot.isEmpty() && baseline.isEmpty();
    for (const QString &path : paths) {
        if (path.isEmpty()) { error = QStringLiteral("A target file could not be resolved."); continue; }
        const QJsonObject fingerprint = tweakFileState(path);
        current.insert(path, fingerprint);
        if (fingerprint.contains(QStringLiteral("error"))) error = fingerprint.value(QStringLiteral("error")).toString();
        if (!snapshot.isEmpty() && (unknown || baseline.value(path).toObject() != fingerprint)) changed << path;
        QFile input(path);
        QByteArray before;
        if (fingerprint.value(QStringLiteral("exists")).toBool()) {
            if (!input.open(QIODevice::ReadOnly)) error = QStringLiteral("Cannot read ") + path;
            else before = input.readAll();
        }
        QByteArray after;
        bool removed = false;
        if (restoring) {
            for (const auto &value : originals) {
                const QJsonObject entry = value.toObject();
                if (entry.value(QStringLiteral("path")).toString() != path) continue;
                removed = !entry.value(QStringLiteral("existed")).toBool();
                if (!removed) {
                    QFile backup(snapshot + QLatin1Char('/') + entry.value(QStringLiteral("backup")).toString());
                    if (!backup.open(QIODevice::ReadOnly)) error = QStringLiteral("Cannot read original backup for ") + path;
                    else after = backup.readAll();
                }
            }
        } else {
            const bool engine = path == info.value(QStringLiteral("engineIniPath")).toString();
            const QVariantList changes = plan.value(engine ? QStringLiteral("engineChanges") : QStringLiteral("reshadeChanges")).toList();
            QStringList valueSummary;
            for (const QVariant &item : changes) {
                const QVariantMap change = item.toMap();
                const QString section = change.value(QStringLiteral("section")).toString().trimmed();
                const QString key = change.value(QStringLiteral("key")).toString().trimmed();
                const QString proposed = change.value(QStringLiteral("value")).toString();
                bool inSection = false;
                bool found = false;
                QString previous;
                for (const QString &line : QString::fromUtf8(before).split(QLatin1Char('\n'))) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
                        if (inSection) break;
                        inSection = trimmed.mid(1, trimmed.size() - 2).trimmed().compare(section, Qt::CaseInsensitive) == 0;
                        continue;
                    }
                    const int equals = line.indexOf(QLatin1Char('='));
                    if (inSection && equals >= 0 && line.left(equals).trimmed().compare(key, Qt::CaseInsensitive) == 0) {
                        found = true;
                        previous = line.mid(equals + 1).trimmed();
                        break;
                    }
                }
                const QString action = !found ? QStringLiteral("ADD") : previous == proposed ? QStringLiteral("UNCHANGED") : QStringLiteral("CHANGE");
                valueSummary << QStringLiteral("%1 [%2] %3: %4 → %5").arg(action, section, key, found ? previous : QStringLiteral("(not set)"), proposed);
            }
            sections << path + QStringLiteral("\nSETTING CHANGES\n") + valueSummary.join(QLatin1Char('\n'));
            after = mergedTweakIni(QString::fromUtf8(before), changes);
        }
        sections << path + QStringLiteral("\nCURRENT\n") + (fingerprint.value(QStringLiteral("exists")).toBool() ? QString::fromUtf8(before) : QStringLiteral("(file does not exist)"))
                    + QStringLiteral("\nPROPOSED\n") + (removed ? QStringLiteral("(file will be removed)") : QString::fromUtf8(after));
    }
    QString warning;
    if (unknown) warning = QStringLiteral("This profile was applied before change tracking was available. Its baseline is unavailable.");
    else if (!changed.isEmpty()) warning = QStringLiteral("Files changed outside the managed tweak operation:\n") + changed.join(QLatin1Char('\n'));
    if (!changed.isEmpty()) warning += QStringLiteral("\nContinuing may replace your edits. A recovery copy of the current files will be kept; Cancel preserves them in place.");
    if (!restoring && plan.value(QStringLiteral("engineReadOnly")).toBool()) sections << QStringLiteral("Engine.ini will be made read-only.");
    if (restoring) sections << QStringLiteral("Original file permissions will also be restored.");
    QJsonObject review{{QStringLiteral("state"), state}, {QStringLiteral("originals"), originals}, {QStringLiteral("plan"), QJsonObject::fromVariantMap(plan)},
                       {QStringLiteral("files"), current}, {QStringLiteral("restore"), restoring},
                       {QStringLiteral("preview"), sections.join(QStringLiteral("\n\n────────\n\n"))},
                       {QStringLiteral("game"), g->appId}, {QStringLiteral("exe"), g->exePath}};
    result[QStringLiteral("token")] = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(review).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    result[QStringLiteral("text")] = sections.join(QStringLiteral("\n\n────────\n\n"));
    result[QStringLiteral("warning")] = warning;
    result[QStringLiteral("requiresAcknowledgement")] = !changed.isEmpty();
    result[QStringLiteral("error")] = error;
    result[QStringLiteral("canProceed")] = error.isEmpty() && !paths.isEmpty() && info.value(restoring ? QStringLiteral("canRestore") : QStringLiteral("canApply")).toBool();
    result[QStringLiteral("paths")] = paths;
    return result;
}

bool InstallerManager::validateTweakReview(int row, bool restoring, const QString &token, bool acknowledged) {
    const QVariantMap review = previewRenoDxTweaks(row, restoring);
    if (!review.value(QStringLiteral("canProceed")).toBool() || token.isEmpty() || token != review.value(QStringLiteral("token")).toString()) {
        setStatus(QStringLiteral("Files or the profile changed, or the preview is unavailable. Open a fresh preview before continuing."));
        return false;
    }
    if (review.value(QStringLiteral("requiresAcknowledgement")).toBool() && !acknowledged) {
        setStatus(QStringLiteral("Review and acknowledge the external changes before continuing."));
        return false;
    }
    return true;
}

void InstallerManager::applyRenoDxTweaks(int row, const QString &token, bool acknowledged) {
    if (!validateTweakReview(row, false, token, acknowledged)) return;
    if (m_busy)
        return;
    beginInlineOperation(row, QStringLiteral("renodx"));
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) {
        setStatus(QStringLiteral("No game selected."));
        return;
    }

    const QVariantMap info = renoDxTweaksInfo(row);
    const QVariantMap plan = buildRenoDxTweakPlan(*g);
    if (!info.value(QStringLiteral("available")).toBool()) {
        setStatus(QStringLiteral("No managed RenoDX tweaks are available for this game."));
        return;
    }
    if (info.value(QStringLiteral("profilePending")).toBool()) {
        setStatus(QStringLiteral("The managed RenoDX tweak profile is still being resolved from the live catalogs. Retry once the profile finishes loading."));
        return;
    }
    if (!info.value(QStringLiteral("canApply")).toBool()) {
        const QString detail = info.value(QStringLiteral("engineStatus")).toString();
        setStatus(detail.isEmpty() ? QStringLiteral("The managed tweak profile cannot be applied yet.") : detail);
        return;
    }

    const QJsonObject state = readState(row);
    const QString previousSnapshot = state.value(QStringLiteral("renoDxTweakSnapshotDir")).toString();
    const QString previousProfile = state.value(QStringLiteral("renoDxTweakProfileId")).toString();
    const QString profileId = plan.value(QStringLiteral("id")).toString();
    if (!previousSnapshot.isEmpty() && !previousProfile.isEmpty() && previousProfile != profileId) {
        setStatus(QStringLiteral("A different RenoDX tweak profile is already managed for this game. Restore the original files before applying the new profile."));
        return;
    }

    const QVariantList engineChanges = plan.value(QStringLiteral("engineChanges")).toList();
    const QVariantList reshadeChanges = plan.value(QStringLiteral("reshadeChanges")).toList();
    const QString enginePath = info.value(QStringLiteral("engineIniPath")).toString();
    const QString reshadePath = reshadeChanges.isEmpty() ? QString() : QFileInfo(g->exePath).absolutePath() + QStringLiteral("/ReShade.ini");

    QStringList targets;
    if (!engineChanges.isEmpty()) targets << enginePath;
    if (!reshadeChanges.isEmpty()) targets << reshadePath;
    targets.removeDuplicates();

    QString snapshotDir = previousSnapshot;
    QString error;
    QString recoveryDir;
    const bool newSnapshot = snapshotDir.isEmpty();
    const bool keepRecovery = previewRenoDxTweaks(row, false).value(QStringLiteral("requiresAcknowledgement")).toBool();
    if (!createRenoDxTweakSnapshot(row, targets, recoveryDir, error)) {
        setStatus(QStringLiteral("Could not back up the current tweak files: %1").arg(error));
        return;
    }
    if (!validateTweakReview(row, false, token, acknowledged)) {
        QDir(recoveryDir).removeRecursively();
        return;
    }
    if (newSnapshot) snapshotDir = recoveryDir;
    const auto rollback = [&]() {
        QString restoreError;
        const bool restored = restoreRenoDxTweakSnapshot(row, recoveryDir, restoreError);
        return restored ? QStringLiteral("Current files restored; recovery copy: ") + recoveryDir
                        : QStringLiteral("Rollback incomplete: ") + restoreError + QStringLiteral(". Recovery copy: ") + recoveryDir;
    };

    bool ok = true;
    if (!engineChanges.isEmpty())
        ok = mergeIniChanges(enginePath, engineChanges, error);
    if (ok && !reshadeChanges.isEmpty())
        ok = mergeIniChanges(reshadePath, reshadeChanges, error);

    if (!ok) {
        setStatus(QStringLiteral("Tweak application failed: ") + error + QStringLiteral(". ") + rollback());
        return;
    }

    if (!engineChanges.isEmpty() && plan.value(QStringLiteral("engineReadOnly")).toBool()) {
        QFileDevice::Permissions perms = QFile::permissions(enginePath);
        perms &= ~QFileDevice::WriteOwner;
        perms &= ~QFileDevice::WriteGroup;
        perms &= ~QFileDevice::WriteOther;
        if (!QFile::setPermissions(enginePath, perms)) {
            setStatus(QStringLiteral("Could not make Engine.ini read-only. ") + rollback());
            return;
        }
    }

    QJsonObject baseline;
    for (const QString &path : targets) {
        const QJsonObject fileState = tweakFileState(path);
        if (fileState.contains(QStringLiteral("error"))) {
            setStatus(fileState.value(QStringLiteral("error")).toString() + QStringLiteral(". ") + rollback());
            return;
        }
        baseline.insert(path, fileState);
    }
    if (!writeState(row, {{QStringLiteral("renoDxAppliedProfileSettings"), tweakProfileSettings(plan)},
                          {QStringLiteral("renoDxTweakBaseline"), baseline},
                          {QStringLiteral("renoDxTweakSnapshotDir"), snapshotDir},
                          {QStringLiteral("renoDxTweakProfileId"), profileId},
                          {QStringLiteral("renoDxTweakProfileTitle"), plan.value(QStringLiteral("title")).toString()}})) {
        setStatus(QStringLiteral("Could not save tweak metadata. ") + rollback());
        return;
    }
    if (!newSnapshot && !keepRecovery) QDir(recoveryDir).removeRecursively();
    m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Applied managed RenoDX tweaks: %1").arg(plan.value(QStringLiteral("title")).toString()) + (keepRecovery ? QStringLiteral(". Recovery copy: ") + recoveryDir : QString()));
}

void InstallerManager::restoreRenoDxTweaks(int row, const QString &token, bool acknowledged) {
    if (!validateTweakReview(row, true, token, acknowledged)) return;
    if (m_busy)
        return;
    beginInlineOperation(row, QStringLiteral("renodx"));
    const QJsonObject state = readState(row);
    const QString snapshotDir = state.value(QStringLiteral("renoDxTweakSnapshotDir")).toString();
    if (snapshotDir.isEmpty()) {
        setStatus(QStringLiteral("No Reno119-managed RenoDX tweak backup exists for this game."));
        return;
    }

    QString error;
    QString recoveryDir;
    const QVariantMap review = previewRenoDxTweaks(row, true);
    if (!createRenoDxTweakSnapshot(row, review.value(QStringLiteral("paths")).toStringList(), recoveryDir, error)) {
        setStatus(QStringLiteral("Could not preserve the current files: ") + error);
        return;
    }
    if (!validateTweakReview(row, true, token, acknowledged)) {
        QDir(recoveryDir).removeRecursively();
        return;
    }
    if (!restoreRenoDxTweakSnapshot(row, snapshotDir, error)) {
        QString rollbackError;
        const bool rolledBack = restoreRenoDxTweakSnapshot(row, recoveryDir, rollbackError);
        setStatus(QStringLiteral("Restore failed: ") + error + (rolledBack ? QStringLiteral(". Current files restored.") : QStringLiteral(". Rollback failed: ") + rollbackError) + QStringLiteral(" Recovery copy: ") + recoveryDir);
        return;
    }

    if (!writeState(row, {{QStringLiteral("renoDxAppliedProfileSettings"), QJsonValue()},
                          {QStringLiteral("renoDxTweakBaseline"), QJsonValue()},
                          {QStringLiteral("renoDxTweakSnapshotDir"), QJsonValue()},
                          {QStringLiteral("renoDxTweakProfileId"), QJsonValue()},
                          {QStringLiteral("renoDxTweakProfileTitle"), QJsonValue()}})) {
        QString rollbackError;
        const bool rolledBack = restoreRenoDxTweakSnapshot(row, recoveryDir, rollbackError);
        m_games->refreshInstallState(row);
        setStatus(QStringLiteral("Could not clear tweak metadata. ") + (rolledBack ? QStringLiteral("Current files restored.") : QStringLiteral("Rollback failed: ") + rollbackError) + QStringLiteral(" Recovery copy: ") + recoveryDir);
        return;
    }
    QDir(snapshotDir).removeRecursively();
    m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Restored the original tweak files. Recovery copy of the previous files: ") + recoveryDir);
}
