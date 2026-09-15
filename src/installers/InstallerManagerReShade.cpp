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

void InstallerManager::installReShade(int row, const QString &channel) {
    clearExternalReShadeOverwriteContext();
    m_installAsReShade64 = false;
    beginReShadeInstall(row, channel, false, false);
}

void InstallerManager::installReShadeOverExternal(int row, const QString &channel) {
    clearExternalReShadeOverwriteContext();
    m_installAsReShade64 = false;
    beginReShadeInstall(row, channel, true, false);
}

void InstallerManager::installReShade64(int row, const QString &channel) {
    clearExternalReShadeOverwriteContext();
    m_installAsReShade64 = true;
    beginReShadeInstall(row, channel, false, true);
}

void InstallerManager::installReShade64OverExternal(int row, const QString &channel) {
    clearExternalReShadeOverwriteContext();
    m_installAsReShade64 = true;
    beginReShadeInstall(row, channel, true, true);
}

void InstallerManager::beginReShadeInstall(int row, const QString &channel, bool allowExternalOverwrite, bool asReShade64) {
    if (m_busy) return;
    beginInlineOperation(row, asReShade64 ? QStringLiteral("reshade64") : QStringLiteral("reshade"));
    const auto *g = m_games->game(row);
    if (!g || g->exePath.isEmpty()) { setStatus("No game executable detected."); return; }
    if (asReShade64 && g->architecture != QStringLiteral("x64")) {
        setStatus("ReShade64.dll chain-loading is available for x64 games only.");
        return;
    }
    if (!asReShade64 && g->graphicsApi == "Vulkan") { setStatus("Vulkan implicit-layer installation is not implemented yet."); return; }

    const QJsonObject state = readState(row);
    const QString managedProxy = state.value("reshadeProxy").toString();
    QString managedReShade64 = state.value("reshade64File").toString();
    if (managedReShade64.isEmpty() && managedProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0)
        managedReShade64 = QStringLiteral("ReShade64.dll");

    QString targetProxy;
    if (asReShade64) {
        targetProxy = QStringLiteral("ReShade64.dll");
    } else {
        const QString defaultProxy = proxyNameFor(row);
        if (defaultProxy.isEmpty()) { setStatus("Unsupported or unknown graphics API."); return; }
        targetProxy = managedProxy.isEmpty() || managedProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0
            ? defaultProxy : managedProxy;
    }

    if (allowExternalOverwrite) {
        const bool externalAvailable = asReShade64 ? g->reshade64External : g->reshadeExternal;
        if (!externalAvailable || (!asReShade64 && g->reshadeProxy.isEmpty())) {
            setStatus(asReShade64 ? "No verified external ReShade64.dll is available to replace."
                                  : "No verified external ReShade installation is available to replace.");
            return;
        }
        if ((asReShade64 && !managedReShade64.isEmpty()) || (!asReShade64 && !managedProxy.isEmpty())) {
            setStatus(asReShade64 ? "This ReShade64.dll is already managed by Reno119."
                                  : "This ReShade installation is already managed by Reno119.");
            return;
        }
        if (!asReShade64)
            targetProxy = g->reshadeProxy;
        const QString backupDir = asReShade64 ? createExternalReShade64Backup(row) : createExternalReShadeBackup(row);
        if (backupDir.isEmpty()) {
            setStatus(asReShade64 ? "Could not create the required backup of the external ReShade64.dll. Nothing was changed."
                                  : "Could not create the required backup of the external ReShade installation. Nothing was changed.");
            return;
        }
        m_externalReShadeOverwriteActive = true;
        m_externalReShadeOverwriteAppId = g->appId;
        m_externalReShadeProxy = targetProxy;
        m_externalReShadeBackupDir = backupDir;
    } else {
        const QString managedTarget = asReShade64 ? managedReShade64 : managedProxy;
        const QString dest = QFileInfo(g->exePath).absolutePath() + "/" + targetProxy;
        if (QFileInfo::exists(dest) && managedTarget.compare(targetProxy, Qt::CaseInsensitive) != 0) {
            setStatus(QString("Refusing to overwrite existing %1 because Reno119 did not install it.").arg(targetProxy));
            return;
        }
    }

    const QString normalized = channel.trimmed().toLower();
    setBusy(true); setProgress(0.03);

    if (!allowExternalOverwrite) {
        const QString backupDir = createBackup(row, asReShade64 ? QStringLiteral("before-reshade64") : QStringLiteral("before-reshade"));
        if (!backupDir.isEmpty())
            writeState(row, {{"lastBackupDir", backupDir}});
    }

    if (normalized == QStringLiteral("custom")) {
        installCustomReShade(row);
        return;
    }

    if (normalized == QStringLiteral("recommended")) {
        const QString version = m_catalog->recommendedReShadeVersion(g->name);
        if (version.isEmpty()) {
            setStatus("Could not determine the RenoDX-recommended ReShade version.");
            setBusy(false);
            return;
        }
        setStatus("Using recommended ReShade " + version + " with full add-on support...");
        const QUrl url(QStringLiteral("https://reshade.me/downloads/ReShade_Setup_") + version + QStringLiteral("_Addon.exe"));
        installReShadeVersion(row, version, url, normalized);
        return;
    }

    const QJsonObject cachedRelease = m_releaseCache.value(QStringLiteral("ReShade")).toObject();
    const QString cachedVersion = cachedRelease.value(QStringLiteral("version")).toString();
    if (releaseCacheFresh(QStringLiteral("ReShade")) && !cachedVersion.isEmpty()) {
        setStatus(QStringLiteral("Using cached ReShade release metadata for %1...").arg(cachedVersion));
        const QUrl cachedUrl(QStringLiteral("https://reshade.me/downloads/ReShade_Setup_") + cachedVersion + QStringLiteral("_Addon.exe"));
        installReShadeVersion(row, cachedVersion, cachedUrl, QStringLiteral("latest"));
        return;
    }

    setStatus("Checking reshade.me for the latest full add-on build...");
    fetchReShadeHome(row);
}

void InstallerManager::installCustomReShade(int row) {
    if (!m_settings || !m_settings->customReShadeConfigured()) {
        setStatus("Custom ReShade is not configured in Settings.");
        setBusy(false);
        return;
    }

    const QString source = m_settings->customReShadeSource();
    if (source == QStringLiteral("version")) {
        const QString version = m_settings->customReShadeVersion().trimmed();
        const QUrl url(QStringLiteral("https://reshade.me/downloads/ReShade_Setup_") + version + QStringLiteral("_Addon.exe"));
        setStatus("Using custom ReShade version " + version + "...");
        installReShadeVersion(row, QStringLiteral("custom-") + version, url, QStringLiteral("custom"));
        return;
    }

    if (source == QStringLiteral("url")) {
        const QUrl url = QUrl::fromUserInput(m_settings->customReShadeUrl().trimmed());
        if (!url.isValid()) {
            setStatus("The configured custom ReShade URL is invalid.");
            setBusy(false);
            return;
        }
        QString version = QFileInfo(url.path()).completeBaseName();
        if (version.isEmpty())
            version = QStringLiteral("custom-url");
        installReShadeVersion(row, version, url, "custom");
        return;
    }

    if (source == QStringLiteral("file")) {
        const QString file = QFileInfo(m_settings->customReShadeFile().trimmed()).absoluteFilePath();
        if (!QFileInfo::exists(file)) {
            setStatus("The configured custom ReShade file does not exist.");
            setBusy(false);
            return;
        }
        const QString version = QStringLiteral("local-") + QFileInfo(file).completeBaseName();
        const QString outputDir = versionCacheDir(version);
        QString error;
        setStatus("Extracting custom local ReShade build...");
        if (!extractReShade(file, outputDir, error)) {
            setStatus(error);
            setBusy(false);
            return;
        }
        const auto *g = m_games->game(row);
        const QString staged = outputDir + (g && g->architecture == "x86" ? "/ReShade32.dll" : "/ReShade64.dll");
        if (deployReShadeDll(row, staged, version, "custom"))
            setStatus("Installed custom ReShade build from local file.");
        setProgress(1.0);
        setBusy(false);
        ++m_cacheRevision;
        emit cacheInfoChanged();
        return;
    }

    setStatus("Unknown custom ReShade source.");
    setBusy(false);
}

void InstallerManager::fetchReShadeHome(int row) {
    QNetworkRequest req(QUrl("https://reshade.me"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Reno119/" RENO119_VERSION);
    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, row] {
        if (reply->error() != QNetworkReply::NoError) {
            setStatus("Failed to query reshade.me: " + reply->errorString()); setBusy(false); reply->deleteLater(); return;
        }
        const QString html = QString::fromUtf8(reply->readAll());
        const QRegularExpression rx("/downloads/ReShade_Setup_([\\d.]+)_Addon\\.exe", QRegularExpression::CaseInsensitiveOption);
        const auto match = rx.match(html);
        if (!match.hasMatch()) {
            setStatus("Could not find the full add-on ReShade download link on reshade.me."); setBusy(false); reply->deleteLater(); return;
        }
        const QString version = match.captured(1);
        const QUrl url(QStringLiteral("https://reshade.me") + match.captured(0));

        QJsonObject cacheEntry;
        cacheEntry.insert(QStringLiteral("version"), version);
        cacheEntry.insert(QStringLiteral("url"), QStringLiteral("https://reshade.me"));
        cacheEntry.insert(QStringLiteral("checkedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        m_releaseCache.insert(QStringLiteral("ReShade"), cacheEntry);
        m_failedReleaseSources.remove(QStringLiteral("ReShade"));
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
        QSaveFile cacheFile(cacheDir + QStringLiteral("/update-releases-v1.json"));
        const QByteArray cacheData = QJsonDocument(m_releaseCache).toJson();
        if (cacheFile.open(QIODevice::WriteOnly) && cacheFile.write(cacheData) == cacheData.size())
            cacheFile.commit();

        reply->deleteLater();
        installReShadeVersion(row, version, url, "latest");
    });
}

void InstallerManager::installReShadeVersion(int row, const QString &version, const QUrl &url, const QString &channel) {
    const auto *g = m_games->game(row);
    if (!g) { setBusy(false); return; }

    const QString versionDir = versionCacheDir(version);
    const QString staged = versionDir + (g->architecture == "x86" ? "/ReShade32.dll" : "/ReShade64.dll");
    if (QFileInfo(staged).size() > 1000000) {
        setStatus("Using cached ReShade " + version + " (" + channel + ")...");
        setProgress(0.9);
        if (deployReShadeDll(row, staged, version, channel))
            setStatus("ReShade " + version + " (" + channel + ") installed successfully.");
        setProgress(1.0); setBusy(false); ++m_cacheRevision; emit cacheInfoChanged(); return;
    }

    downloadReShadeInstaller(row, version, url, channel);
}

void InstallerManager::downloadReShadeInstaller(int row, const QString &version, const QUrl &url, const QString &channel) {
    setStatus("Downloading ReShade " + version + " (" + channel + ") with full add-on support..."); setProgress(0.12);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Reno119/" RENO119_VERSION);
    req.setRawHeader("Referer", "https://reshade.me/");
    auto *reply = m_net.get(req);
    const QString versionDir = versionCacheDir(version);
    const QString path = versionDir + "/" + (QFileInfo(url.path()).fileName().isEmpty() ? (QStringLiteral("ReShade_") + version + QStringLiteral(".exe")) : QFileInfo(url.path()).fileName());
    auto *file = new QSaveFile(path, reply);
    if (!file->open(QIODevice::WriteOnly)) {
        setStatus("Could not open ReShade cache file for writing."); setBusy(false); reply->abort(); reply->deleteLater(); return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 done, qint64 total) {
        if (total > 0) setProgress(0.12 + 0.62 * (double(done) / double(total)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, row, version, path, versionDir, channel] {
        file->write(reply->readAll());
        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting(); setStatus("ReShade download failed: " + reply->errorString()); setBusy(false); reply->deleteLater(); return;
        }
        if (!file->commit()) {
            setStatus("Failed to commit downloaded ReShade installer."); setBusy(false); reply->deleteLater(); return;
        }
        reply->deleteLater();
        QFile check(path);
        if (!check.open(QIODevice::ReadOnly) || check.read(2) != "MZ") {
            setStatus("Downloaded ReShade installer is not a valid PE file. The selected version may no longer be hosted by the configured source."); setBusy(false); return;
        }
        setStatus("Extracting ReShade " + version + " DLLs with 7z..."); setProgress(0.78);
        QString error;
        if (!extractReShade(path, versionDir, error)) { setStatus(error); setBusy(false); return; }
        const auto *g = m_games->game(row);
        if (!g) { setBusy(false); return; }
        const QString staged = versionDir + (g->architecture == "x86" ? "/ReShade32.dll" : "/ReShade64.dll");
        setProgress(0.93);
        if (deployReShadeDll(row, staged, version, channel))
            setStatus("ReShade " + version + " (" + channel + ") installed successfully.");
        setProgress(1.0); setBusy(false); ++m_cacheRevision; emit cacheInfoChanged();
    });
}

bool InstallerManager::extractReShade(const QString &installerPath, const QString &outputDir, QString &error) {
    QString sevenZip = QStandardPaths::findExecutable("7z");
    if (sevenZip.isEmpty()) sevenZip = QStandardPaths::findExecutable("7zz");
    if (sevenZip.isEmpty()) {
        error = "Neither 7z nor 7zz was found. On Arch/CachyOS install the '7zip' package, then retry.";
        return false;
    }
    QDir().mkpath(outputDir);
    QProcess p;
    p.start(sevenZip, {"e", "-y", "-o" + outputDir, installerPath, "ReShade64.dll", "ReShade32.dll"});
    if (!p.waitForStarted(5000) || !p.waitForFinished(60000) || p.exitCode() != 0) {
        error = "7z could not extract ReShade: " + QString::fromUtf8(p.readAllStandardError());
        return false;
    }
    if (QFileInfo(outputDir + "/ReShade64.dll").size() < 1000000 || QFileInfo(outputDir + "/ReShade32.dll").size() < 1000000) {
        error = "ReShade extraction finished but the expected DLLs were not found.";
        return false;
    }
    return true;
}

bool InstallerManager::deployReShadeDll(int row, const QString &stagedDll, const QString &version, const QString &channel) {
    const auto *g = m_games->game(row);
    if (!g) return false;

    const QJsonObject existingState = readState(row);
    const QString existingManagedProxy = existingState.value("reshadeProxy").toString();
    QString existingManagedReShade64 = existingState.value("reshade64File").toString();
    const bool legacyReShade64 = existingManagedProxy.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0;
    if (existingManagedReShade64.isEmpty() && legacyReShade64)
        existingManagedReShade64 = QStringLiteral("ReShade64.dll");

    const bool externalTakeover = m_externalReShadeOverwriteActive &&
                                  m_externalReShadeOverwriteAppId == g->appId &&
                                  !m_externalReShadeProxy.isEmpty();
    const QString target = m_installAsReShade64
        ? QStringLiteral("ReShade64.dll")
        : (externalTakeover
            ? m_externalReShadeProxy
            : ((existingManagedProxy.isEmpty() || legacyReShade64) ? proxyNameFor(row) : existingManagedProxy));
    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString dest = exeDir + "/" + target;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    if (QFileInfo::exists(dest) && !QFile::remove(dest)) {
        setStatus("Could not replace " + dest);
        return false;
    }
    if (!QFile::copy(stagedDll, dest)) {
        if (externalTakeover && !m_externalReShadeBackupDir.isEmpty())
            restoreBackupDirectory(row, m_externalReShadeBackupDir);
        setStatus(QStringLiteral("Could not copy %1 to %2%3")
                      .arg(m_installAsReShade64 ? QStringLiteral("ReShade64") : QStringLiteral("ReShade"),
                           dest,
                           externalTakeover ? QStringLiteral(". The external ReShade backup was restored.") : QString()));
        clearExternalReShadeOverwriteContext();
        m_installAsReShade64 = false;
        return false;
    }
    ensureReShadeIni(exeDir);

    const bool hadManagedState = m_installAsReShade64 ? !existingManagedReShade64.isEmpty()
                                                       : (!existingManagedProxy.isEmpty() && !legacyReShade64);
    QJsonObject patch;
    if (m_installAsReShade64) {
        patch = {
            {"reshade64File", QStringLiteral("ReShade64.dll")},
            {"reshade64Version", version},
            {"reshade64Channel", channel},
            {"reshade64ManagedBy", "reno119"},
            {"exePath", g->exePath}
        };
        if (legacyReShade64) {
            patch.insert("reshadeProxy", QJsonValue());
            patch.insert("reshadeVersion", QJsonValue());
            patch.insert("reshadeChannel", QJsonValue());
            patch.insert("reshadeManagedBy", QJsonValue());
        }
    } else {
        patch = {
            {"reshadeProxy", target},
            {"reshadeVersion", version},
            {"reshadeChannel", channel},
            {"reshadeManagedBy", "reno119"},
            {"exePath", g->exePath}
        };
        if (legacyReShade64 && QFileInfo::exists(exeDir + QStringLiteral("/ReShade64.dll"))) {
            patch.insert("reshade64File", QStringLiteral("ReShade64.dll"));
            patch.insert("reshade64Version", existingState.value("reshadeVersion"));
            patch.insert("reshade64Channel", existingState.value("reshadeChannel"));
            patch.insert("reshade64ManagedBy", "reno119");
        }
    }
    if (externalTakeover && !m_externalReShadeBackupDir.isEmpty())
        patch.insert(QStringLiteral("lastBackupDir"), m_externalReShadeBackupDir);

    if (!writeState(row, patch)) {
        if (externalTakeover && !m_externalReShadeBackupDir.isEmpty()) {
            QFile::remove(dest);
            restoreBackupDirectory(row, m_externalReShadeBackupDir);
        } else if (!hadManagedState) {
            QFile::remove(dest);
        }
        setStatus(externalTakeover
                      ? QStringLiteral("ReShade was copied, but Reno119 could not save its ownership state. The external ReShade backup was restored.")
                      : QStringLiteral("ReShade was copied, but Reno119 could not save its ownership state. The fresh copy was rolled back; check game-directory permissions/filesystem mount options."));
        clearExternalReShadeOverwriteContext();
        m_installAsReShade64 = false;
        m_games->refreshInstallState(row);
        return false;
    }
    m_games->recordReShadeInstalledChannel(row, channel);
    m_games->refreshInstallState(row);
    const bool installedAs64 = m_installAsReShade64;
    clearExternalReShadeOverwriteContext();
    m_installAsReShade64 = false;
    m_activeUpdateSucceeded = true;
    if (installedAs64)
        logLine(QStringLiteral("Installed ReShade as ReShade64.dll for OptiScaler chain-loading."));
    return true;
}

void InstallerManager::uninstallReShade(int row) {
    if (m_busy) return;
    beginInlineOperation(row, QStringLiteral("reshade"));
    const auto *g = m_games->game(row); if (!g) return;
    const QString backupDir = createBackup(row, QStringLiteral("before-reshade-remove"));
    if (!backupDir.isEmpty())
        writeState(row, {{"lastBackupDir", backupDir}});
    const auto state = readState(row);
    const QString proxy = state.value("reshadeProxy").toString();
    if (proxy.isEmpty()) { setStatus("No Reno119-managed ReShade installation found."); return; }
    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    if (QFileInfo::exists(exeDir + "/" + proxy) && !QFile::remove(exeDir + "/" + proxy)) {
        setStatus("Could not remove " + proxy); return;
    }
    writeState(row, {{"reshadeProxy", QJsonValue()}, {"reshadeVersion", QJsonValue()}, {"reshadeChannel", QJsonValue()}, {"reshadeManagedBy", QJsonValue()}});
    m_games->refreshInstallState(row);
    setStatus("Removed the Reno119-managed ReShade proxy. ReShade.ini was preserved.");
}

void InstallerManager::uninstallReShade64(int row) {
    if (m_busy) return;
    beginInlineOperation(row, QStringLiteral("reshade64"));
    const auto *g = m_games->game(row);
    if (!g) return;
    const QJsonObject state = readState(row);
    QString managedFile = state.value(QStringLiteral("reshade64File")).toString();
    const bool legacy = managedFile.isEmpty() &&
        state.value(QStringLiteral("reshadeProxy")).toString().compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0;
    if (legacy)
        managedFile = QStringLiteral("ReShade64.dll");
    if (managedFile.isEmpty()) {
        setStatus(QStringLiteral("No Reno119-managed ReShade64.dll installation found."));
        return;
    }

    const QString backupDir = createBackup(row, QStringLiteral("before-reshade64-remove"));
    if (!backupDir.isEmpty())
        writeState(row, {{"lastBackupDir", backupDir}});

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString path = exeDir + QStringLiteral("/") + managedFile;
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        setStatus(QStringLiteral("Could not remove %1").arg(managedFile));
        return;
    }

    QJsonObject patch{
        {"reshade64File", QJsonValue()}, {"reshade64Version", QJsonValue()},
        {"reshade64Channel", QJsonValue()}, {"reshade64ManagedBy", QJsonValue()}
    };
    if (legacy) {
        patch.insert("reshadeProxy", QJsonValue());
        patch.insert("reshadeVersion", QJsonValue());
        patch.insert("reshadeChannel", QJsonValue());
        patch.insert("reshadeManagedBy", QJsonValue());
    }
    writeState(row, patch);
    m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Removed the Reno119-managed ReShade64.dll. ReShade.ini was preserved."));
}
