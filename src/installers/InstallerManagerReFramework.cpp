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

void InstallerManager::installReFramework(int row) {
    beginReFrameworkInstall(row, false);
}

void InstallerManager::installReFrameworkOverExternal(int row) {
    beginReFrameworkInstall(row, true);
}

void InstallerManager::beginReFrameworkInstall(int row, bool allowExternalOverwrite) {
    if (m_busy)
        return;
    beginInlineOperation(row, QStringLiteral("reframework"));
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty()) {
        setStatus(QStringLiteral("No game executable detected."));
        return;
    }
    if (!g->reframeworkSupported && !(allowExternalOverwrite && g->reframeworkExternal)) {
        setStatus(QStringLiteral("This title is not in Reno119's current REFramework support data. A positively detected external REFramework install can still be taken over explicitly."));
        return;
    }
    if (g->architecture == QStringLiteral("x86")) {
        setStatus(QStringLiteral("The current monolithic REFramework nightly is intended for supported 64-bit RE Engine games."));
        return;
    }
    if (g->reframeworkExternal && !allowExternalOverwrite) {
        setStatus(QStringLiteral("External REFramework detected. Use Install over external REFramework if you want Reno119 to take ownership."));
        return;
    }

    QString takeoverBackup;
    if (g->reframeworkExternal) {
        takeoverBackup = createExternalReFrameworkBackup(row);
        if (takeoverBackup.isEmpty()) {
            setStatus(QStringLiteral("Could not create the mandatory external REFramework takeover backup."));
            return;
        }
    } else {
        const QString backup = createBackup(row, QStringLiteral("before-reframework-install"));
        if (!backup.isEmpty())
            writeState(row, {{QStringLiteral("lastBackupDir"), backup}});
    }

    setBusy(true);
    setProgress(0.03);

    const QJsonObject cachedRelease = m_releaseCache.value(QStringLiteral("REFramework")).toObject();
    const QString cachedVersion = cachedRelease.value(QStringLiteral("version")).toString();
    const QUrl cachedAssetUrl(cachedRelease.value(QStringLiteral("assetUrl")).toString());
    if (releaseCacheFresh(QStringLiteral("REFramework")) && !cachedVersion.isEmpty() && cachedAssetUrl.isValid()) {
        setStatus(QStringLiteral("Using cached REFramework release metadata for %1...").arg(cachedVersion));
        downloadReFramework(row, cachedVersion, cachedAssetUrl, takeoverBackup);
        return;
    }

    setStatus(QStringLiteral("Checking the latest REFramework nightly..."));
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/praydog/REFramework-nightly/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Reno119/" RENO119_VERSION));
    request.setRawHeader("Accept", "application/vnd.github+json");
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, row, takeoverBackup] {
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("Failed to query the REFramework nightly release: %1").arg(reply->errorString()));
            reply->deleteLater();
            setBusy(false);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        if (!doc.isObject()) {
            setStatus(QStringLiteral("GitHub returned an invalid REFramework release response."));
            setBusy(false);
            return;
        }
        const QJsonObject release = doc.object();
        const QString version = release.value(QStringLiteral("tag_name")).toString();
        QUrl assetUrl;
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString().compare(QStringLiteral("REFramework.zip"), Qt::CaseInsensitive) == 0) {
                assetUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
                break;
            }
        }
        if (version.isEmpty() || !assetUrl.isValid()) {
            setStatus(QStringLiteral("The latest REFramework release did not contain the expected REFramework.zip asset."));
            setBusy(false);
            return;
        }

        QJsonObject cacheEntry;
        cacheEntry.insert(QStringLiteral("version"), version);
        cacheEntry.insert(QStringLiteral("url"), release.value(QStringLiteral("html_url")));
        cacheEntry.insert(QStringLiteral("assetUrl"), assetUrl.toString());
        cacheEntry.insert(QStringLiteral("publishedUtc"), release.value(QStringLiteral("published_at")));
        cacheEntry.insert(QStringLiteral("checkedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        m_releaseCache.insert(QStringLiteral("REFramework"), cacheEntry);
        m_failedReleaseSources.remove(QStringLiteral("REFramework"));
        applyReleaseCache();

        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
        QSaveFile cacheFile(cacheDir + QStringLiteral("/update-releases-v1.json"));
        const QByteArray cacheData = QJsonDocument(m_releaseCache).toJson();
        if (cacheFile.open(QIODevice::WriteOnly) && cacheFile.write(cacheData) == cacheData.size())
            cacheFile.commit();

        downloadReFramework(row, version, assetUrl, takeoverBackup);
    });
}

void InstallerManager::downloadReFramework(int row, const QString &version, const QUrl &url, const QString &takeoverBackup) {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) {
        setBusy(false);
        return;
    }

    const QString safeVersion = version.isEmpty() ? QStringLiteral("latest") : version;
    const QString cache = rootCacheBase() + QStringLiteral("/reframework/") + safeVersion;
    QDir().mkpath(cache);
    const QString archivePath = cache + QStringLiteral("/REFramework.zip");
    const QString extractedPath = cache + QStringLiteral("/dinput8.dll");

    if (QFileInfo(extractedPath).size() > 256 * 1024) {
        const QString exeDir = QFileInfo(g->exePath).absolutePath();
        const QString destination = exeDir + QStringLiteral("/dinput8.dll");
        QSaveFile out(destination);
        out.setDirectWriteFallback(true);
        QFile staged(extractedPath);
        if (!staged.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly) ||
            out.write(staged.readAll()) <= 0 || !out.commit()) {
            setStatus(QStringLiteral("Could not install cached REFramework dinput8.dll."));
            setBusy(false);
            return;
        }
        if (!writeState(row, {{QStringLiteral("reframeworkFile"), QStringLiteral("dinput8.dll")},
                              {QStringLiteral("reframeworkVersion"), version},
                              {QStringLiteral("reframeworkUrl"), url.toString()},
                              {QStringLiteral("reframeworkManagedBy"), QStringLiteral("reno119")}})) {
            if (!takeoverBackup.isEmpty())
                restoreBackupDirectory(row, takeoverBackup);
            setStatus(QStringLiteral("REFramework was copied, but Reno119 could not save ownership state. The takeover backup was restored where available."));
            setBusy(false);
            return;
        }
        m_games->refreshInstallState(row);
        setProgress(1.0);
        m_activeUpdateSucceeded = true;
        setStatus(QStringLiteral("REFramework %1 installed successfully.").arg(version));
        setBusy(false);
        return;
    }

    setStatus(QStringLiteral("Downloading REFramework %1...").arg(version));
    setProgress(0.12);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Reno119/" RENO119_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_net.get(request);
    auto *file = new QSaveFile(archivePath, reply);
    file->setDirectWriteFallback(true);
    if (!file->open(QIODevice::WriteOnly)) {
        setStatus(QStringLiteral("Could not open the REFramework cache archive for writing."));
        reply->abort();
        reply->deleteLater();
        setBusy(false);
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 done, qint64 total) {
        if (total > 0)
            setProgress(0.12 + 0.60 * (double(done) / double(total)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, row, version, url, archivePath, cache, takeoverBackup] {
        file->write(reply->readAll());
        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting();
            setStatus(QStringLiteral("REFramework download failed: %1").arg(reply->errorString()));
            reply->deleteLater();
            setBusy(false);
            return;
        }
        if (!file->commit()) {
            setStatus(QStringLiteral("Could not commit the downloaded REFramework archive."));
            reply->deleteLater();
            setBusy(false);
            return;
        }
        reply->deleteLater();

        setStatus(QStringLiteral("Extracting dinput8.dll from REFramework..."));
        setProgress(0.78);
        QString error;
        if (!extractReFramework(archivePath, cache, error)) {
            setStatus(error);
            setBusy(false);
            return;
        }

        const auto *game = m_games ? m_games->game(row) : nullptr;
        if (!game) {
            setBusy(false);
            return;
        }
        const QString staged = cache + QStringLiteral("/dinput8.dll");
        QFile input(staged);
        const QString destination = QFileInfo(game->exePath).absolutePath() + QStringLiteral("/dinput8.dll");
        QSaveFile out(destination);
        out.setDirectWriteFallback(true);
        if (!input.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly)) {
            setStatus(QStringLiteral("Could not stage REFramework dinput8.dll into the game directory."));
            setBusy(false);
            return;
        }
        const QByteArray bytes = input.readAll();
        if (bytes.size() < 256 * 1024 || out.write(bytes) != bytes.size() || !out.commit()) {
            setStatus(QStringLiteral("Could not write REFramework dinput8.dll to the game directory."));
            setBusy(false);
            return;
        }

        if (!writeState(row, {{QStringLiteral("reframeworkFile"), QStringLiteral("dinput8.dll")},
                              {QStringLiteral("reframeworkVersion"), version},
                              {QStringLiteral("reframeworkUrl"), url.toString()},
                              {QStringLiteral("reframeworkManagedBy"), QStringLiteral("reno119")}})) {
            if (!takeoverBackup.isEmpty())
                restoreBackupDirectory(row, takeoverBackup);
            setStatus(QStringLiteral("REFramework was copied, but Reno119 could not save ownership state. The previous external setup was restored where available."));
            setBusy(false);
            return;
        }
        m_games->refreshInstallState(row);
        setProgress(1.0);
        m_activeUpdateSucceeded = true;
        setStatus(QStringLiteral("REFramework %1 installed successfully.").arg(version));
        setBusy(false);
    });
}

bool InstallerManager::extractReFramework(const QString &archivePath, const QString &outputDir, QString &error) {
    QString sevenZip = QStandardPaths::findExecutable(QStringLiteral("7z"));
    if (sevenZip.isEmpty())
        sevenZip = QStandardPaths::findExecutable(QStringLiteral("7zz"));
    if (sevenZip.isEmpty()) {
        error = QStringLiteral("Neither 7z nor 7zz was found. Install the Arch/CachyOS '7zip' package and retry.");
        return false;
    }
    QDir().mkpath(outputDir);
    QFile::remove(outputDir + QStringLiteral("/dinput8.dll"));
    QProcess process;
    process.start(sevenZip, {QStringLiteral("e"), QStringLiteral("-y"), QStringLiteral("-o") + outputDir,
                             archivePath, QStringLiteral("dinput8.dll")});
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000) || process.exitCode() != 0) {
        error = QStringLiteral("7z could not extract REFramework: %1").arg(QString::fromUtf8(process.readAllStandardError()));
        return false;
    }
    QFile dll(outputDir + QStringLiteral("/dinput8.dll"));
    if (!dll.open(QIODevice::ReadOnly) || dll.size() < 256 * 1024 || dll.read(2) != QByteArray("MZ")) {
        error = QStringLiteral("REFramework extraction finished, but dinput8.dll was missing or invalid.");
        return false;
    }
    return true;
}

void InstallerManager::uninstallReFramework(int row) {
    if (m_busy)
        return;
    beginInlineOperation(row, QStringLiteral("reframework"));
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty()) {
        setStatus(QStringLiteral("No game executable detected."));
        return;
    }
    const QJsonObject state = readState(row);
    const QString owned = state.value(QStringLiteral("reframeworkFile")).toString();
    if (owned.isEmpty()) {
        setStatus(QStringLiteral("No Reno119-managed REFramework installation was found."));
        return;
    }
    const QString backup = createBackup(row, QStringLiteral("before-reframework-remove"));
    if (!backup.isEmpty())
        writeState(row, {{QStringLiteral("lastBackupDir"), backup}});
    const QString path = QFileInfo(g->exePath).absolutePath() + QStringLiteral("/") + owned;
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        setStatus(QStringLiteral("Could not remove %1.").arg(owned));
        return;
    }
    writeState(row, {{QStringLiteral("reframeworkFile"), QJsonValue()},
                     {QStringLiteral("reframeworkVersion"), QJsonValue()},
                     {QStringLiteral("reframeworkUrl"), QJsonValue()},
                     {QStringLiteral("reframeworkManagedBy"), QJsonValue()}});
    m_games->refreshInstallState(row);
    setStatus(QStringLiteral("Removed Reno119-managed REFramework dinput8.dll. REFramework config/scripts were preserved."));
}

QVariantMap InstallerManager::reFrameworkHotkey(int row) const {
    QVariantMap out;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty())
        return out;

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    QString raw;
    const QString path = findExistingReFrameworkConfig(exeDir, &raw);
    const int code = parseVirtualKey(raw, 0x2D);
    out[QStringLiteral("available")] = g->reframeworkInstalled;
    out[QStringLiteral("code")] = code;
    out[QStringLiteral("name")] = virtualKeyName(code);
    out[QStringLiteral("configured")] = !raw.isEmpty();
    out[QStringLiteral("detectedExistingConfig")] = !path.isEmpty();
    out[QStringLiteral("raw")] = raw;
    out[QStringLiteral("path")] = path;
    return out;
}

bool InstallerManager::setReFrameworkHotkey(int row, int virtualKey) {
    beginInlineOperation(row, QStringLiteral("reframework"));
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty() || !g->reframeworkInstalled) {
        setStatus(QStringLiteral("Install or detect REFramework before changing its overlay hotkey."));
        return false;
    }
    if (virtualKey < 1 || virtualKey > 255) {
        setStatus(QStringLiteral("Invalid Windows virtual-key code."));
        return false;
    }
    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    bool legacyMenuKey = false;
    const QString path = findExistingReFrameworkConfig(exeDir, nullptr, &legacyMenuKey);
    if (path.isEmpty()) {
        setStatus(QStringLiteral("No existing REFramework *_fw_config.txt was found. Launch the game with REFramework once so it can create its game-specific config, then retry."));
        return false;
    }
    const QString backup = createBackup(row, QStringLiteral("before-reframework-hotkey"));
    if (!backup.isEmpty())
        writeState(row, {{QStringLiteral("lastBackupDir"), backup}});

    const QString value = QString::number(virtualKey);
    if (!writeSimpleKeyValue(path, QStringLiteral("REFrameworkConfig_MenuKey_V2"), value)) {
        setStatus(QStringLiteral("Could not update the REFramework menu-key setting."));
        return false;
    }
    // Older REFramework builds used the unprefixed key. If this config already
    // contains it, keep it synchronized so externally managed legacy installs
    // continue to work as well.
    if (legacyMenuKey && !writeSimpleKeyValue(path, QStringLiteral("MenuKey_V2"), value)) {
        setStatus(QStringLiteral("Updated the current REFramework menu key, but could not synchronize the legacy MenuKey_V2 setting."));
        return false;
    }
    setStatus(QStringLiteral("REFramework overlay hotkey changed to %1.").arg(virtualKeyName(virtualKey)));
    return true;
}
