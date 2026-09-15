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

void InstallerManager::recordUpdateInfo(const QString &appId, const QVariantMap &info) {
    m_updateInfo.insert(appId, info);
    ++m_updateRevision;
    emit updateInfoChanged();
}

QVariantMap InstallerManager::updateInfo(int row) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return {};
    return m_updateInfo.value(g->appId);
}

void InstallerManager::loadReleaseCache() {
    QFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/update-releases-v1.json"));
    if (file.open(QIODevice::ReadOnly))
        m_releaseCache = QJsonDocument::fromJson(file.readAll()).object();
    applyReleaseCache();
}

bool InstallerManager::releaseCacheFresh(const QString &source) const {
    const auto entry = m_releaseCache.value(source).toObject();
    const auto stamp = QDateTime::fromString(entry.value(QStringLiteral("checkedUtc")).toString(), Qt::ISODate);
    const auto age = stamp.secsTo(QDateTime::currentDateTimeUtc());
    const bool hasRequiredMetadata = source != QStringLiteral("REFramework") ||
        !entry.value(QStringLiteral("assetUrl")).toString().isEmpty();
    return stamp.isValid() && age >= 0 && age < 21600 && !entry.value(QStringLiteral("version")).toString().isEmpty()
        && hasRequiredMetadata && !m_failedReleaseSources.contains(source) && !entry.value(QStringLiteral("refreshFailed")).toBool();
}

void InstallerManager::applyReleaseCache() {
    const auto ref = m_releaseCache.value(QStringLiteral("REFramework")).toObject();
    const auto opti = m_releaseCache.value(QStringLiteral("OptiScaler")).toObject();
    m_latestReFrameworkUrl = ref.value(QStringLiteral("url")).toString();
    m_latestReFrameworkAssetUrl = ref.value(QStringLiteral("assetUrl")).toString();
    m_latestReFrameworkPublishedUtc = ref.value(QStringLiteral("publishedUtc")).toString();
    m_latestOptiScalerVersion = opti.value(QStringLiteral("version")).toString();
    m_latestOptiScalerUrl = opti.value(QStringLiteral("url")).toString();
    m_latestOptiScalerPublishedUtc = opti.value(QStringLiteral("publishedUtc")).toString();
}

QString InstallerManager::updateCacheStatus() const {
    QStringList details;
    for (const auto &source : {QStringLiteral("ReShade"), QStringLiteral("REFramework"), QStringLiteral("OptiScaler")}) {
        const auto entry = m_releaseCache.value(source).toObject();
        const bool available = !entry.value(QStringLiteral("version")).toString().isEmpty();
        const QString stamp = entry.value(QStringLiteral("checkedUtc")).toString();
        details << source + QStringLiteral(": ") + (available
            ? (releaseCacheFresh(source) ? QStringLiteral("cached") : QStringLiteral("stale — last known result"))
                + QStringLiteral(" · ") + stamp
            : QStringLiteral("unavailable"));
    }
    return details.join(QStringLiteral("\n"));
}

void InstallerManager::fetchLatestVersions(std::function<void(const QString &, const QString &)> done,
                                                bool forceRefresh,
                                                const QSet<QString> &requiredSources) {
    m_releaseCallbacks.append(std::move(done));
    if (m_releaseFetchBusy)
        return;
    m_releaseFetchBusy = true;
    const auto pending = QSharedPointer<int>::create(1);
    const auto finish = [this, pending] {
        if (--*pending != 0) return;
        applyReleaseCache();
        m_releaseFetchBusy = false;
        const auto callbacks = std::exchange(m_releaseCallbacks, {});
        const QString reshade = m_releaseCache.value(QStringLiteral("ReShade")).toObject().value(QStringLiteral("version")).toString();
        const QString ref = m_releaseCache.value(QStringLiteral("REFramework")).toObject().value(QStringLiteral("version")).toString();
        emit bulkUpdateChanged();
        for (const auto &callback : callbacks) callback(reshade, ref);
    };
    const auto needsSource = [&requiredSources](const QString &source) {
        return requiredSources.isEmpty() || requiredSources.contains(source);
    };
    if (m_catalog && needsSource(QStringLiteral("catalog"))) {
        m_catalog->refreshCatalog(forceRefresh);
        if (m_catalog->catalogRefreshing()) {
            ++*pending;
            const auto connection = QSharedPointer<QMetaObject::Connection>::create();
            *connection = connect(m_catalog, &RenoDxCatalogService::catalogChanged, this, [this, connection, finish] {
                if (m_catalog->catalogRefreshing()) return;
                disconnect(*connection);
                finish();
            });
        }
    }
    const QList<QPair<QString, QString>> sources{
        {QStringLiteral("ReShade"), QStringLiteral("https://reshade.me")},
        {QStringLiteral("REFramework"), QStringLiteral("https://api.github.com/repos/praydog/REFramework-nightly/releases/latest")},
        {QStringLiteral("OptiScaler"), QStringLiteral("https://api.github.com/repos/optiscaler/OptiScaler/releases/latest")}
    };
    for (const auto &source : sources) {
        if (!needsSource(source.first)) continue;
        if (!forceRefresh && releaseCacheFresh(source.first)) continue;
        ++*pending;
        QNetworkRequest request{QUrl(source.second)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Reno119/" RENO119_VERSION));
        request.setTransferTimeout(15000);
        auto *reply = m_net.get(request);
        // An absolute deadline also covers a server that trickles bytes forever.
        QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, key = source.first, finish] {
            QJsonObject entry;
            if (reply->error() == QNetworkReply::NoError) {
                const auto bytes = reply->readAll();
                if (key == QStringLiteral("ReShade")) {
                    const QRegularExpression rx(QStringLiteral(R"(/downloads/ReShade_Setup_([\d.]+)_Addon\.exe)"), QRegularExpression::CaseInsensitiveOption);
                    const auto match = rx.match(QString::fromUtf8(bytes));
                    if (match.hasMatch()) entry.insert(QStringLiteral("version"), match.captured(1));
                } else {
                    const auto obj = QJsonDocument::fromJson(bytes).object();
                    entry.insert(QStringLiteral("version"), obj.value(QStringLiteral("tag_name")));
                    entry.insert(QStringLiteral("url"), obj.value(QStringLiteral("html_url")));
                    entry.insert(QStringLiteral("publishedUtc"), obj.value(QStringLiteral("published_at")));
                    if (key == QStringLiteral("REFramework")) {
                        const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
                        for (const QJsonValue &value : assets) {
                            const QJsonObject asset = value.toObject();
                            if (asset.value(QStringLiteral("name")).toString().compare(QStringLiteral("REFramework.zip"), Qt::CaseInsensitive) == 0) {
                                entry.insert(QStringLiteral("assetUrl"), asset.value(QStringLiteral("browser_download_url")));
                                break;
                            }
                        }
                    }
                }
            }
            if (!entry.value(QStringLiteral("version")).toString().isEmpty()) {
                entry.insert(QStringLiteral("checkedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                m_releaseCache.insert(key, entry);
                m_failedReleaseSources.remove(key);
            } else {
                m_failedReleaseSources.insert(key);
                auto previous = m_releaseCache.value(key).toObject();
                previous.insert(QStringLiteral("refreshFailed"), true);
                m_releaseCache.insert(key, previous);
            }
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            QDir().mkpath(dir);
            QSaveFile file(dir + QStringLiteral("/update-releases-v1.json"));
            const auto data = QJsonDocument(m_releaseCache).toJson();
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) file.commit();
            reply->deleteLater();
            finish();
        });
    }
    finish();
}

QVariantMap InstallerManager::evaluateUpdateInfo(const GameInfo &game, const QJsonObject &state,
                                                  const QString &latestReShade,
                                                  const QString &latestReFramework) const {
    QVariantMap info;
    const bool anyReShadeInstalled = game.reshadeInstalled || game.reshade64Installed;
    const bool anyReShadeExternal = (game.reshadeInstalled && game.reshadeExternal) ||
                                    (game.reshade64Installed && game.reshade64External);
    const bool anyReShadeManaged = game.reshadeManaged || game.reshade64Managed;
    const QString recommended = m_catalog ? m_catalog->recommendedReShadeVersion(game.name) : QString();
    const QString installedRenoDxUrl = state.value(QStringLiteral("renodxUrl")).toString();
    const QString desiredReno = desiredRenoDxUrlForGame(game);

    const auto layoutNeedsUpdate = [&recommended, &latestReShade](bool managed, const QString &channel, const QString &version) {
        if (!managed)
            return false;
        if (channel == QStringLiteral("recommended"))
            return !recommended.isEmpty() && version != recommended;
        if (channel == QStringLiteral("latest"))
            return !latestReShade.isEmpty() && version != latestReShade;
        return false;
    };

    const bool directReShadeUpdateDetected = layoutNeedsUpdate(game.reshadeManaged, game.reshadeChannel, game.reshadeVersion);
    const bool reShade64UpdateDetected = layoutNeedsUpdate(game.reshade64Managed, game.reshade64Channel, game.reshade64Version);
    QStringList reShadeTargets;
    if (game.reshadeManaged)
        reShadeTargets << (game.reshadeChannel == QStringLiteral("recommended") ? recommended :
                           game.reshadeChannel == QStringLiteral("latest") ? latestReShade : game.reshadeVersion);
    if (game.reshade64Managed)
        reShadeTargets << (game.reshade64Channel == QStringLiteral("recommended") ? recommended :
                           game.reshade64Channel == QStringLiteral("latest") ? latestReShade : game.reshade64Version);
    if (reShadeTargets.isEmpty() && anyReShadeExternal && !latestReShade.isEmpty())
        reShadeTargets << latestReShade;
    reShadeTargets.removeAll(QString());
    reShadeTargets.removeDuplicates();
    const QString reshadeTarget = reShadeTargets.join(QStringLiteral(" + "));

    const bool renoUpdateDetected = game.renodxManaged && !desiredReno.isEmpty() && installedRenoDxUrl != desiredReno;
    const bool reframeworkUpdateDetected = game.reframeworkManaged && !latestReFramework.isEmpty() &&
                                           game.reframeworkVersion != latestReFramework;

    const QString skippedReShade = m_settings ? m_settings->skippedUpdateTarget(game.appId, QStringLiteral("reshade")) : QString();
    const QString skippedRenoDx = m_settings ? m_settings->skippedUpdateTarget(game.appId, QStringLiteral("renodx")) : QString();
    const QString skippedReFramework = m_settings ? m_settings->skippedUpdateTarget(game.appId, QStringLiteral("reframework")) : QString();
    const QString skippedOptiScaler = m_settings ? m_settings->skippedUpdateTarget(game.appId, QStringLiteral("optiscaler")) : QString();

    const bool reshadeSkipped = !reshadeTarget.isEmpty() && skippedReShade == reshadeTarget;
    const bool renodxSkipped = !desiredReno.isEmpty() && skippedRenoDx == desiredReno;
    const bool reframeworkSkipped = !latestReFramework.isEmpty() && skippedReFramework == latestReFramework;
    const bool optiScalerSkipped = !m_latestOptiScalerVersion.isEmpty() && skippedOptiScaler == m_latestOptiScalerVersion;

    const bool directReShadeUpdate = directReShadeUpdateDetected && !reshadeSkipped;
    const bool reShade64Update = reShade64UpdateDetected && !reshadeSkipped;
    const bool reshadeUpdateAvailable = directReShadeUpdate || reShade64Update;
    const bool renoUpdateAvailable = renoUpdateDetected && !renodxSkipped;
    const bool reframeworkUpdateAvailable = reframeworkUpdateDetected && !reframeworkSkipped;

    QString installedReShadeVersion;
    QString installedReShadeChannel;
    if (game.reshadeManaged || (game.reshadeInstalled && !game.reshade64Managed)) {
        installedReShadeVersion = game.reshadeVersion;
        installedReShadeChannel = game.reshadeChannel;
    } else {
        installedReShadeVersion = game.reshade64Version;
        installedReShadeChannel = game.reshade64Channel;
    }

    info[QStringLiteral("installedReShadeVersion")] = installedReShadeVersion;
    info[QStringLiteral("installedReShadeChannel")] = installedReShadeChannel;
    info[QStringLiteral("recommendedReShadeVersion")] = recommended;
    info[QStringLiteral("latestReShadeVersion")] = latestReShade;
    info[QStringLiteral("installedRenoDxUrl")] = installedRenoDxUrl;
    info[QStringLiteral("desiredRenoDxUrl")] = desiredReno;
    info[QStringLiteral("latestReFrameworkVersion")] = latestReFramework;
    info[QStringLiteral("latestReFrameworkUrl")] = m_latestReFrameworkUrl;
    info[QStringLiteral("latestReFrameworkPublishedUtc")] = m_latestReFrameworkPublishedUtc;
    info[QStringLiteral("latestOptiScalerVersion")] = m_latestOptiScalerVersion;
    info[QStringLiteral("latestOptiScalerUrl")] = m_latestOptiScalerUrl;
    info[QStringLiteral("latestOptiScalerPublishedUtc")] = m_latestOptiScalerPublishedUtc;
    info[QStringLiteral("reshadeTarget")] = reshadeTarget;
    info[QStringLiteral("renodxTarget")] = desiredReno;
    info[QStringLiteral("reframeworkTarget")] = latestReFramework;
    info[QStringLiteral("optiscalerTarget")] = m_latestOptiScalerVersion;
    info[QStringLiteral("directReShadeUpdateDetected")] = directReShadeUpdateDetected;
    info[QStringLiteral("reShade64UpdateDetected")] = reShade64UpdateDetected;
    info[QStringLiteral("reshadeUpdateDetected")] = directReShadeUpdateDetected || reShade64UpdateDetected;
    info[QStringLiteral("renodxUpdateDetected")] = renoUpdateDetected;
    info[QStringLiteral("reframeworkUpdateDetected")] = reframeworkUpdateDetected;
    info[QStringLiteral("reshadeSkipped")] = reshadeSkipped;
    info[QStringLiteral("renodxSkipped")] = renodxSkipped;
    info[QStringLiteral("reframeworkSkipped")] = reframeworkSkipped;
    info[QStringLiteral("optiscalerSkipped")] = optiScalerSkipped;
    info[QStringLiteral("reshadeUpdateAvailable")] = reshadeUpdateAvailable;
    info[QStringLiteral("renodxUpdateAvailable")] = renoUpdateAvailable;
    info[QStringLiteral("reframeworkUpdateAvailable")] = reframeworkUpdateAvailable;

    QString reshadeSummary;
    if (!anyReShadeInstalled) {
        reshadeSummary = QStringLiteral("ReShade not installed. Recommended: %1")
            .arg(recommended.isEmpty() ? QStringLiteral("unknown") : recommended);
        if (!latestReShade.isEmpty())
            reshadeSummary += QStringLiteral(". Latest: %1").arg(latestReShade);
    } else if (anyReShadeManaged) {
        QStringList managedLayouts;
        if (game.reshadeManaged) {
            managedLayouts << QStringLiteral("direct ReShade %1 (%2)")
                .arg(game.reshadeVersion.isEmpty() ? QStringLiteral("unknown") : game.reshadeVersion,
                     game.reshadeChannel.isEmpty() ? QStringLiteral("unknown") : game.reshadeChannel);
        }
        if (game.reshade64Managed) {
            managedLayouts << QStringLiteral("ReShade64 %1 (%2)")
                .arg(game.reshade64Version.isEmpty() ? QStringLiteral("unknown") : game.reshade64Version,
                     game.reshade64Channel.isEmpty() ? QStringLiteral("unknown") : game.reshade64Channel);
        }
        reshadeSummary = QStringLiteral("Managed %1. Recommended: %2")
            .arg(managedLayouts.join(QStringLiteral(" + ")),
                 recommended.isEmpty() ? QStringLiteral("unknown") : recommended);
        if (!latestReShade.isEmpty())
            reshadeSummary += QStringLiteral(". Latest: %1").arg(latestReShade);
        if (anyReShadeExternal)
            reshadeSummary += QStringLiteral(". An additional external ReShade layout is present and remains informational.");

        const bool needsRecommendedLookup =
            (game.reshadeManaged && game.reshadeChannel == QStringLiteral("recommended")) ||
            (game.reshade64Managed && game.reshade64Channel == QStringLiteral("recommended"));
        const bool needsLatestLookup =
            (game.reshadeManaged && game.reshadeChannel == QStringLiteral("latest")) ||
            (game.reshade64Managed && game.reshade64Channel == QStringLiteral("latest"));
        if (needsLatestLookup && latestReShade.isEmpty())
            reshadeSummary += QStringLiteral(". Latest-version lookup unavailable; that update state is unknown.");
        if (needsRecommendedLookup && recommended.isEmpty())
            reshadeSummary += QStringLiteral(". Recommended-version lookup unavailable; that update state is unknown.");
        if ((!needsLatestLookup || !latestReShade.isEmpty()) &&
            (!needsRecommendedLookup || !recommended.isEmpty()))
            reshadeSummary += reshadeUpdateAvailable ? QStringLiteral(". Update available.") : QStringLiteral(". Up to date.");
        if (reshadeSkipped)
            reshadeSummary += QStringLiteral(" Skipping target %1.").arg(reshadeTarget);
    } else {
        reshadeSummary = QStringLiteral("External ReShade detected%1")
            .arg(installedReShadeVersion.isEmpty() ? QString() : QStringLiteral(" %1").arg(installedReShadeVersion));
        if (!latestReShade.isEmpty())
            reshadeSummary += QStringLiteral(". Latest: %1").arg(latestReShade);
        reshadeSummary += QStringLiteral(". External installs are informational until you explicitly install over them.");
    }
    info[QStringLiteral("reshadeSummary")] = reshadeSummary;

    QString renodxSummary;
    if (!game.renodxInstalled)
        renodxSummary = QStringLiteral("RenoDX not installed.");
    else if (game.renodxExternal)
        renodxSummary = QStringLiteral("External RenoDX detected. Update status is informational because Reno119 does not own this addon.");
    else if (desiredReno.isEmpty())
        renodxSummary = QStringLiteral("RenoDX installed. No comparison source is available for this title.");
    else
        renodxSummary = renoUpdateAvailable ? QStringLiteral("RenoDX update available or source changed.")
                                             : QStringLiteral("RenoDX appears current.");
    if (renodxSkipped)
        renodxSummary += QStringLiteral(" This catalog target is currently skipped.");
    info[QStringLiteral("renodxSummary")] = renodxSummary;

    QString reframeworkSummary;
    if (!game.reframeworkInstalled) {
        reframeworkSummary = game.reframeworkSupported
            ? QStringLiteral("REFramework is supported for this title but is not installed.")
            : QStringLiteral("REFramework is not installed.");
    } else if (game.reframeworkExternal) {
        reframeworkSummary = QStringLiteral("External REFramework detected. Reno119 will not assume ownership or update it automatically.");
        if (!latestReFramework.isEmpty())
            reframeworkSummary += QStringLiteral(" Latest nightly: %1.").arg(latestReFramework);
    } else if (latestReFramework.isEmpty()) {
        reframeworkSummary = QStringLiteral("Managed REFramework %1. Latest-nightly lookup unavailable; update state is unknown.")
            .arg(game.reframeworkVersion.isEmpty() ? QStringLiteral("unknown") : game.reframeworkVersion);
    } else {
        reframeworkSummary = QStringLiteral("Managed REFramework %1. Latest: %2.%3")
            .arg(game.reframeworkVersion.isEmpty() ? QStringLiteral("unknown") : game.reframeworkVersion,
                 latestReFramework,
                 reframeworkUpdateAvailable ? QStringLiteral(" Update available.") : QStringLiteral(" Up to date."));
    }
    if (reframeworkSkipped)
        reframeworkSummary += QStringLiteral(" This nightly is currently skipped.");
    info[QStringLiteral("reframeworkSummary")] = reframeworkSummary;

    QString optiSummary;
    if (!game.optiScalerInstalled) {
        optiSummary = QStringLiteral("OptiScaler not detected.");
    } else if (m_latestOptiScalerVersion.isEmpty()) {
        optiSummary = QStringLiteral("OptiScaler detected. Latest-release lookup unavailable.");
    } else {
        optiSummary = QStringLiteral("OptiScaler detected. Latest stable release: %1. Reno119 does not own or auto-update OptiScaler itself.")
            .arg(m_latestOptiScalerVersion);
        if (optiScalerSkipped)
            optiSummary += QStringLiteral(" This release is currently skipped in Update Center.");
    }
    info[QStringLiteral("optiscalerSummary")] = optiSummary;

    return info;
}

namespace {
QSet<QString> updateSourcesForGames(const QVector<GameInfo> &games) {
    QSet<QString> sources;
    for (const GameInfo &game : games) {
        const bool directExternalReShade = (game.reshadeInstalled && !game.reshadeManaged) ||
                                           (game.reshade64Installed && !game.reshade64Managed);
        const bool latestReShade = (game.reshadeManaged && game.reshadeChannel == QStringLiteral("latest")) ||
                                   (game.reshade64Managed && game.reshade64Channel == QStringLiteral("latest"));
        const bool recommendedReShade = (game.reshadeManaged && game.reshadeChannel == QStringLiteral("recommended")) ||
                                        (game.reshade64Managed && game.reshade64Channel == QStringLiteral("recommended"));
        if (directExternalReShade || latestReShade)
            sources.insert(QStringLiteral("ReShade"));
        if (recommendedReShade || game.renodxManaged)
            sources.insert(QStringLiteral("catalog"));
        if (game.reframeworkInstalled)
            sources.insert(QStringLiteral("REFramework"));
        if (game.optiScalerInstalled)
            sources.insert(QStringLiteral("OptiScaler"));
    }
    return sources;
}
}

void InstallerManager::checkUpdates(int row) {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return;
    if (m_bulkUpdateBusy) {
        setStatus(QStringLiteral("A library-wide update check is already running."));
        return;
    }
    if (m_catalog && !m_catalog->renoDxCatalogFinished()) {
        setStatus(QStringLiteral("The RenoDX catalog is still loading. Retry the update check in a moment."));
        return;
    }

    const GameInfo game = *g;
    const QJsonObject state = readStateForGame(game);
    setStatus(QStringLiteral("Checking update status for %1...").arg(game.name));
    fetchLatestVersions([this, game, state](const QString &latestReShade, const QString &latestReFramework) {
        const QVariantMap info = evaluateUpdateInfo(game, state, latestReShade, latestReFramework);
        recordUpdateInfo(game.appId, info);
        if (m_games) {
            m_games->setUpdateStatus(game.appId, true,
                                     info.value(QStringLiteral("reshadeUpdateAvailable")).toBool(),
                                     info.value(QStringLiteral("renodxUpdateAvailable")).toBool(),
                                     info.value(QStringLiteral("reframeworkUpdateAvailable")).toBool());
        }
        setStatus(QStringLiteral("Finished checking updates for %1.").arg(game.name));
    });
}

void InstallerManager::checkAllUpdates(bool forceRefresh) {
    if (m_bulkUpdateBusy || m_busy || m_updateQueueActive)
        return;

    QVector<GameInfo> candidates;
    if (m_games) {
        for (const GameInfo &game : m_games->allGamesSnapshot()) {
            if (game.reshadeInstalled || game.reshade64Installed || game.renodxInstalled ||
                game.reframeworkInstalled || game.optiScalerInstalled)
                candidates.push_back(game);
        }
    }

    m_bulkUpdateBusy = true;
    m_bulkUpdateChecked = 0;
    m_bulkUpdateTotal = candidates.size();
    m_bulkUpdateAvailableGames = 0;
    m_bulkUpdateAvailableComponents = 0;
    m_bulkUpdateSummary = candidates.isEmpty()
        ? QStringLiteral("No Reno119-managed components are currently installed.")
        : QStringLiteral("Checking %1 game(s) with Reno119-managed components...").arg(candidates.size());
    emit bulkUpdateChanged();

    if (candidates.isEmpty()) {
        m_bulkUpdateBusy = false;
        emit bulkUpdateChanged();
        return;
    }

    struct UpdatePassState {
        QVector<GameInfo> candidates;
        QString latestReShade;
        QString latestReFramework;
        bool complete = false;
        int index = 0;
        int gamesWithUpdates = 0;
        int componentUpdates = 0;
        int unresolvedComponents = 0;
    };

    const auto runPass = [this](const QVector<GameInfo> &passCandidates,
                                const QString &latestReShade,
                                const QString &latestReFramework,
                                bool complete,
                                std::function<void()> finished) {
        auto state = QSharedPointer<UpdatePassState>::create();
        state->candidates = passCandidates;
        state->latestReShade = latestReShade;
        state->latestReFramework = latestReFramework;
        state->complete = complete;

        m_bulkUpdateChecked = 0;
        emit bulkUpdateChanged();

        auto step = QSharedPointer<std::function<void()>>::create();
        *step = [this, state, step, finished = std::move(finished)]() mutable {
            // Keep each slice deliberately small. readStateForGame(), RenoDX target
            // resolution and model updates all run on the Qt GUI thread, so doing
            // the entire library in one loop makes the window appear frozen.
            constexpr int kGamesPerSlice = 1;
            int processedThisSlice = 0;

            while (state->index < state->candidates.size() && processedThisSlice < kGamesPerSlice) {
                const GameInfo &game = state->candidates.at(state->index++);
                const QJsonObject stateForGame = readStateForGame(game);
                const QVariantMap info = evaluateUpdateInfo(game, stateForGame,
                                                            state->latestReShade,
                                                            state->latestReFramework);
                recordUpdateInfo(game.appId, info);

                const bool reshadeUpdate = info.value(QStringLiteral("reshadeUpdateAvailable")).toBool();
                const bool renodxUpdate = info.value(QStringLiteral("renodxUpdateAvailable")).toBool();
                const bool reframeworkUpdate = info.value(QStringLiteral("reframeworkUpdateAvailable")).toBool();
                const int gameComponentUpdates = int(reshadeUpdate) + int(renodxUpdate) + int(reframeworkUpdate);
                if (gameComponentUpdates > 0)
                    ++state->gamesWithUpdates;
                state->componentUpdates += gameComponentUpdates;

                const bool managedLatestReShade =
                    (game.reshadeManaged && game.reshadeChannel == QStringLiteral("latest")) ||
                    (game.reshade64Managed && game.reshade64Channel == QStringLiteral("latest"));
                const bool managedRecommendedReShade =
                    (game.reshadeManaged && game.reshadeChannel == QStringLiteral("recommended")) ||
                    (game.reshade64Managed && game.reshade64Channel == QStringLiteral("recommended"));
                if (managedLatestReShade && state->latestReShade.isEmpty())
                    ++state->unresolvedComponents;
                if (managedRecommendedReShade && info.value(QStringLiteral("recommendedReShadeVersion")).toString().isEmpty())
                    ++state->unresolvedComponents;
                if (game.reframeworkManaged && state->latestReFramework.isEmpty())
                    ++state->unresolvedComponents;
                if (game.renodxManaged && info.value(QStringLiteral("desiredRenoDxUrl")).toString().isEmpty())
                    ++state->unresolvedComponents;

                ++m_bulkUpdateChecked;
                if (m_games)
                    m_games->setUpdateStatus(game.appId, true, reshadeUpdate, renodxUpdate, reframeworkUpdate);
                ++processedThisSlice;
            }

            m_bulkUpdateAvailableGames = state->gamesWithUpdates;
            m_bulkUpdateAvailableComponents = state->componentUpdates;
            emit bulkUpdateChanged();

            if (state->index < state->candidates.size()) {
                QTimer::singleShot(0, this, [step] { (*step)(); });
                return;
            }

            if (state->complete)
                m_lastUpdateCheck = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            m_bulkUpdateBusy = !state->complete;

            if (state->componentUpdates == 0) {
                m_bulkUpdateSummary = state->unresolvedComponents > 0
                    ? QStringLiteral("Checked %1 game(s). No confirmed updates; %2 component(s) could not be fully checked.")
                          .arg(state->candidates.size()).arg(state->unresolvedComponents)
                    : QStringLiteral("Checked %1 game(s). No managed component updates found.").arg(state->candidates.size());
            } else {
                m_bulkUpdateSummary = QStringLiteral("Checked %1 game(s). %2 update(s) available across %3 game(s).")
                    .arg(state->candidates.size()).arg(state->componentUpdates).arg(state->gamesWithUpdates);
                if (state->unresolvedComponents > 0)
                    m_bulkUpdateSummary += QStringLiteral(" %1 additional component(s) could not be fully checked.").arg(state->unresolvedComponents);
            }

            emit bulkUpdateChanged();
            if (state->complete)
                setStatus(m_bulkUpdateSummary);
            if (finished)
                finished();
        };

        // Yield once before touching the first game so the Update Center can paint
        // its loading state and remain interactive from the start of the check.
        QTimer::singleShot(0, this, [step] { (*step)(); });
    };

    // Resolve release/catalog data first, then evaluate the library exactly once.
    // fetchLatestVersions() already reuses fresh cached release entries and the
    // RenoDX catalog cache, so a separate cached per-game pass only reset the
    // progress counter and made one logical check appear to run twice.
    const QSet<QString> requiredSources = updateSourcesForGames(candidates);
    if (requiredSources.isEmpty()) {
        runPass(candidates, QString(), QString(), true, {});
        return;
    }
    fetchLatestVersions([candidates, runPass](const QString &reshade, const QString &ref) {
        runPass(candidates, reshade, ref, true, {});
    }, forceRefresh, requiredSources);
}

int InstallerManager::rowForAppId(const QString &appId) const {
    if (!m_games || appId.isEmpty())
        return -1;
    for (int row = 0; row < m_games->rowCount(); ++row) {
        const auto *game = m_games->game(row);
        if (game && game->appId == appId)
            return row;
    }
    return -1;
}

QString InstallerManager::updateTargetFor(const QVariantMap &info, const QString &component) const {
    const QString normalized = component.trimmed().toLower();
    if (normalized == QStringLiteral("reshade") || normalized == QStringLiteral("reshade64"))
        return info.value(QStringLiteral("reshadeTarget")).toString();
    if (normalized == QStringLiteral("renodx"))
        return info.value(QStringLiteral("renodxTarget")).toString();
    if (normalized == QStringLiteral("reframework"))
        return info.value(QStringLiteral("reframeworkTarget")).toString();
    if (normalized == QStringLiteral("optiscaler"))
        return info.value(QStringLiteral("optiscalerTarget")).toString();
    return {};
}

QVariantList InstallerManager::updateCenterItems() const {
    QVariantList result;
    if (!m_games)
        return result;

    const auto shortUrlName = [](const QString &value) {
        if (value.isEmpty())
            return QString();
        const QUrl url(value);
        const QString name = QFileInfo(url.path()).fileName();
        return name.isEmpty() ? value : name;
    };

    for (int row = 0; row < m_games->rowCount(); ++row) {
        const auto *game = m_games->game(row);
        if (!game)
            continue;
        const QVariantMap info = m_updateInfo.value(game->appId);
        const bool hasRelevantComponent = game->reshadeInstalled || game->reshade64Installed || game->renodxInstalled ||
                                          game->reframeworkInstalled || game->optiScalerInstalled;
        if (!hasRelevantComponent)
            continue;

        QVariantList components;
        int actionableUpdates = 0;
        int detectedUpdates = 0;

        if (game->reshadeInstalled || game->reshade64Installed) {
            QStringList installed;
            if (game->reshadeInstalled)
                installed << QStringLiteral("Direct %1").arg(game->reshadeVersion.isEmpty() ? QStringLiteral("unknown") : game->reshadeVersion);
            if (game->reshade64Installed)
                installed << QStringLiteral("ReShade64 %1").arg(game->reshade64Version.isEmpty() ? QStringLiteral("unknown") : game->reshade64Version);
            const bool detected = info.value(QStringLiteral("reshadeUpdateDetected")).toBool();
            const bool available = info.value(QStringLiteral("reshadeUpdateAvailable")).toBool();
            const bool skipped = info.value(QStringLiteral("reshadeSkipped")).toBool();
            detectedUpdates += detected ? 1 : 0;
            actionableUpdates += available ? 1 : 0;
            components << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("reshade")},
                {QStringLiteral("name"), QStringLiteral("ReShade")},
                {QStringLiteral("installed"), installed.join(QStringLiteral(" + "))},
                {QStringLiteral("available"), info.value(QStringLiteral("reshadeTarget")).toString()},
                {QStringLiteral("managed"), game->reshadeManaged || game->reshade64Managed},
                {QStringLiteral("external"), (game->reshadeInstalled && game->reshadeExternal) || (game->reshade64Installed && game->reshade64External)},
                {QStringLiteral("updateDetected"), detected},
                {QStringLiteral("updateAvailable"), available},
                {QStringLiteral("skipped"), skipped},
                {QStringLiteral("canUpdate"), available && (game->reshadeManaged || game->reshade64Managed)},
                {QStringLiteral("summary"), info.value(QStringLiteral("reshadeSummary")).toString()},
                {QStringLiteral("releaseUrl"), QStringLiteral("https://reshade.me")},
                {QStringLiteral("linkLabel"), QStringLiteral("ReShade site")}
            };
        }

        if (game->renodxInstalled) {
            const bool detected = info.value(QStringLiteral("renodxUpdateDetected")).toBool();
            const bool available = info.value(QStringLiteral("renodxUpdateAvailable")).toBool();
            const bool skipped = info.value(QStringLiteral("renodxSkipped")).toBool();
            const QVariantMap resolution = renoDxResolutionInfo(row);
            detectedUpdates += detected ? 1 : 0;
            actionableUpdates += available ? 1 : 0;
            QString installed = shortUrlName(info.value(QStringLiteral("installedRenoDxUrl")).toString());
            if (installed.isEmpty())
                installed = game->renodxFile.isEmpty() ? QStringLiteral("Detected") : game->renodxFile;
            components << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("renodx")},
                {QStringLiteral("name"), QStringLiteral("RenoDX")},
                {QStringLiteral("installed"), installed},
                {QStringLiteral("available"), shortUrlName(info.value(QStringLiteral("desiredRenoDxUrl")).toString())},
                {QStringLiteral("managed"), game->renodxManaged},
                {QStringLiteral("external"), game->renodxExternal},
                {QStringLiteral("updateDetected"), detected},
                {QStringLiteral("updateAvailable"), available},
                {QStringLiteral("skipped"), skipped},
                {QStringLiteral("canUpdate"), available && game->renodxManaged},
                {QStringLiteral("summary"), info.value(QStringLiteral("renodxSummary")).toString()},
                {QStringLiteral("matchTitle"), resolution.value(QStringLiteral("matchedCatalogTitle"))},
                {QStringLiteral("matchMethod"), resolution.value(QStringLiteral("matchMethod"))},
                {QStringLiteral("matchRequiresConfirmation"), resolution.value(QStringLiteral("requiresConfirmation"))},
                {QStringLiteral("releaseUrl"), info.value(QStringLiteral("desiredRenoDxUrl")).toString()},
                {QStringLiteral("linkLabel"), QStringLiteral("Open source")}
            };
        }

        if (game->reframeworkInstalled) {
            const bool detected = info.value(QStringLiteral("reframeworkUpdateDetected")).toBool();
            const bool available = info.value(QStringLiteral("reframeworkUpdateAvailable")).toBool();
            const bool skipped = info.value(QStringLiteral("reframeworkSkipped")).toBool();
            detectedUpdates += detected ? 1 : 0;
            actionableUpdates += available ? 1 : 0;
            components << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("reframework")},
                {QStringLiteral("name"), QStringLiteral("REFramework")},
                {QStringLiteral("installed"), game->reframeworkVersion.isEmpty() ? QStringLiteral("unknown") : game->reframeworkVersion},
                {QStringLiteral("available"), info.value(QStringLiteral("latestReFrameworkVersion")).toString()},
                {QStringLiteral("managed"), game->reframeworkManaged},
                {QStringLiteral("external"), game->reframeworkExternal},
                {QStringLiteral("updateDetected"), detected},
                {QStringLiteral("updateAvailable"), available},
                {QStringLiteral("skipped"), skipped},
                {QStringLiteral("canUpdate"), available && game->reframeworkManaged},
                {QStringLiteral("summary"), info.value(QStringLiteral("reframeworkSummary")).toString()},
                {QStringLiteral("releaseUrl"), info.value(QStringLiteral("latestReFrameworkUrl")).toString()},
                {QStringLiteral("publishedUtc"), info.value(QStringLiteral("latestReFrameworkPublishedUtc")).toString()},
                {QStringLiteral("linkLabel"), QStringLiteral("Release notes")}
            };
        }

        if (game->optiScalerInstalled) {
            const bool skipped = info.value(QStringLiteral("optiscalerSkipped")).toBool();
            components << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("optiscaler")},
                {QStringLiteral("name"), QStringLiteral("OptiScaler")},
                {QStringLiteral("installed"), QStringLiteral("Detected (version unknown)")},
                {QStringLiteral("available"), info.value(QStringLiteral("latestOptiScalerVersion")).toString()},
                {QStringLiteral("managed"), false},
                {QStringLiteral("external"), true},
                {QStringLiteral("updateDetected"), false},
                {QStringLiteral("updateAvailable"), false},
                {QStringLiteral("skipped"), skipped},
                {QStringLiteral("canUpdate"), false},
                {QStringLiteral("summary"), info.value(QStringLiteral("optiscalerSummary")).toString()},
                {QStringLiteral("releaseUrl"), info.value(QStringLiteral("latestOptiScalerUrl")).toString()},
                {QStringLiteral("publishedUtc"), info.value(QStringLiteral("latestOptiScalerPublishedUtc")).toString()},
                {QStringLiteral("linkLabel"), QStringLiteral("Release notes")}
            };
        }

        bool stale = false;
        for (auto &value : components) {
            auto component = value.toMap();
            const auto id = component.value(QStringLiteral("id")).toString();
            bool old = false;
            if (id == QStringLiteral("renodx")) old = !m_catalog || m_catalog->catalogStale();
            if (id == QStringLiteral("reframework")) old = !releaseCacheFresh(QStringLiteral("REFramework"));
            if (id == QStringLiteral("optiscaler")) old = !releaseCacheFresh(QStringLiteral("OptiScaler"));
            if (id == QStringLiteral("reshade")) {
                const bool latest = game->reshadeChannel == QStringLiteral("latest") || game->reshade64Channel == QStringLiteral("latest");
                old = latest ? !releaseCacheFresh(QStringLiteral("ReShade")) : (!m_catalog || m_catalog->catalogStale());
            }
            component.insert(QStringLiteral("stale"), old);
            stale = stale || old;
            value = component;
        }
        const auto support = renoDxResolutionInfo(row);
        result << QVariantMap{
            {QStringLiteral("row"), row},
            {QStringLiteral("appId"), game->appId},
            {QStringLiteral("name"), game->name},
            {QStringLiteral("source"), game->source},
            {QStringLiteral("supportNotice"), support.value(QStringLiteral("dedicatedAvailable")).toBool() ? support.value(QStringLiteral("supportDetail")).toString() : QString()},
            {QStringLiteral("checked"), game->updateChecked},
            {QStringLiteral("actionableUpdates"), actionableUpdates},
            {QStringLiteral("detectedUpdates"), detectedUpdates},
            {QStringLiteral("stale"), stale},
            {QStringLiteral("components"), components}
        };
    }
    return result;
}

bool InstallerManager::skipUpdate(int row, const QString &component) {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game || !m_settings)
        return false;
    QVariantMap info = m_updateInfo.value(game->appId);
    const QString target = updateTargetFor(info, component);
    if (target.isEmpty()) {
        setStatus(QStringLiteral("There is no resolved update target to skip."));
        return false;
    }
    if (!m_settings->skipUpdateTarget(game->appId, component, target)) {
        setStatus(QStringLiteral("Could not save the skipped update target."));
        return false;
    }
    const QString c = component.toLower();
    if (c == QStringLiteral("reshade")) {
        info[QStringLiteral("reshadeSkipped")] = true;
        info[QStringLiteral("reshadeUpdateAvailable")] = false;
    } else if (c == QStringLiteral("renodx")) {
        info[QStringLiteral("renodxSkipped")] = true;
        info[QStringLiteral("renodxUpdateAvailable")] = false;
    } else if (c == QStringLiteral("reframework")) {
        info[QStringLiteral("reframeworkSkipped")] = true;
        info[QStringLiteral("reframeworkUpdateAvailable")] = false;
    } else if (c == QStringLiteral("optiscaler")) {
        info[QStringLiteral("optiscalerSkipped")] = true;
    }
    recordUpdateInfo(game->appId, info);
    if (m_games)
        m_games->setUpdateStatus(game->appId, true,
                                 info.value(QStringLiteral("reshadeUpdateAvailable")).toBool(),
                                 info.value(QStringLiteral("renodxUpdateAvailable")).toBool(),
                                 info.value(QStringLiteral("reframeworkUpdateAvailable")).toBool());
    setStatus(QStringLiteral("Skipped this %1 update target for %2.").arg(component, game->name));
    return true;
}

bool InstallerManager::clearSkippedUpdate(int row, const QString &component) {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game || !m_settings)
        return false;
    if (!m_settings->clearSkippedUpdateTarget(game->appId, component))
        return false;
    checkUpdates(row);
    return true;
}

QVariantList InstallerManager::updateHistory(int row) const {
    QVariantList result;
    const QVariantList history = backupHistory(row);
    for (const QVariant &entry : history) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("reason")).toString().startsWith(QStringLiteral("before-update-center-")))
            result << map;
    }
    return result;
}

bool InstallerManager::rollbackUpdate(int row, const QString &backupDir) {
    if (!restoreBackup(row, backupDir))
        return false;
    checkUpdates(row);
    return true;
}

void InstallerManager::enqueueUpdate(const QString &appId, const QString &component) {
    if (appId.isEmpty() || component.isEmpty())
        return;
    const QPair<QString, QString> item{appId, component};
    if (!m_updateQueue.contains(item))
        m_updateQueue.append(item);
}

bool InstallerManager::startQueuedUpdate(int row, const QString &component) {
    const auto *game = m_games ? m_games->game(row) : nullptr;
    if (!game)
        return false;

    const QString c = component.toLower();
    const QString backup = createBackup(row, QStringLiteral("before-update-center-") + c, true);
    if (backup.isEmpty()) {
        setStatus(QStringLiteral("Could not create the required pre-update snapshot. Nothing was changed."));
        return false;
    }
    if (!writeState(row, {{QStringLiteral("lastBackupDir"), backup}})) {
        setStatus(QStringLiteral("Could not record the pre-update snapshot. Nothing was changed."));
        return false;
    }

    if (c == QStringLiteral("reshade")) {
        if (!game->reshadeManaged)
            return false;
        installReShade(row, game->reshadeChannel.isEmpty() ? QStringLiteral("recommended") : game->reshadeChannel);
        return true;
    }
    if (c == QStringLiteral("reshade64")) {
        if (!game->reshade64Managed)
            return false;
        installReShade64(row, game->reshade64Channel.isEmpty() ? QStringLiteral("recommended") : game->reshade64Channel);
        return true;
    }
    if (c == QStringLiteral("renodx")) {
        if (!game->renodxManaged)
            return false;
        if (m_updateQueueAllowNonExactRenoDx)
            installRenoDxConfirmed(row, m_updateQueueConfirmedRenoDxUrls.value(game->appId));
        else
            installRenoDx(row);
        return true;
    }
    if (c == QStringLiteral("reframework")) {
        if (!game->reframeworkManaged)
            return false;
        installReFramework(row);
        return true;
    }
    return false;
}

void InstallerManager::processNextQueuedUpdate() {
    if (!m_updateQueueActive || m_busy)
        return;
    if (!m_activeUpdateResult.isEmpty()) {
        m_activeUpdateResult.insert(QStringLiteral("outcome"), m_activeUpdateSucceeded ? QStringLiteral("Succeeded") : QStringLiteral("Failed"));
        m_activeUpdateResult.insert(QStringLiteral("message"), m_status);
        m_updateResults.append(m_activeUpdateResult);
        m_activeUpdateResult.clear();
        emit updateQueueChanged();
    }
    if (!m_updateQueue.isEmpty()) {
        const auto item = m_updateQueue.takeFirst();
        const int row = rowForAppId(item.first);
        const auto *game = row >= 0 ? m_games->game(row) : nullptr;
        m_activeUpdateSucceeded = false;
        m_activeUpdateResult = {{QStringLiteral("appId"), item.first},
                               {QStringLiteral("component"), item.second},
                               {QStringLiteral("name"), game ? game->name : item.first}};
        const QString base = item.second == QStringLiteral("reshade64") ? QStringLiteral("reshade") : item.second;
        const auto info = m_updateInfo.value(item.first);
        const bool managed = game && (item.second == QStringLiteral("reshade") ? game->reshadeManaged :
            item.second == QStringLiteral("reshade64") ? game->reshade64Managed :
            item.second == QStringLiteral("renodx") ? game->renodxManaged : game->reframeworkManaged);
        const bool targetDetected = item.second == QStringLiteral("reshade")
            ? info.value(QStringLiteral("directReShadeUpdateDetected")).toBool()
            : item.second == QStringLiteral("reshade64")
              ? info.value(QStringLiteral("reShade64UpdateDetected")).toBool() : true;
        if (!managed || !targetDetected || !info.value(base + QStringLiteral("UpdateAvailable")).toBool()) {
            m_activeUpdateResult.insert(QStringLiteral("outcome"), QStringLiteral("Skipped"));
            m_activeUpdateResult.insert(QStringLiteral("message"), QStringLiteral("Game unavailable, no longer managed, or update no longer eligible."));
            m_updateResults.append(m_activeUpdateResult);
            m_activeUpdateResult.clear();
        } else {
            setStatus(QStringLiteral("Update Center: updating %1 for %2...").arg(item.second, game->name));
            startQueuedUpdate(row, item.second);
        }
        emit updateQueueChanged();
        if (!m_busy)
            QTimer::singleShot(0, this, &InstallerManager::processNextQueuedUpdate);
        return;
    }
    m_updateQueueActive = false;
    m_updateQueueAllowNonExactRenoDx = false;
    m_updateQueueConfirmedRenoDxUrls.clear();
    int succeeded = 0, failed = 0, skipped = 0;
    for (const auto &value : m_updateResults) {
        const auto outcome = value.toMap().value(QStringLiteral("outcome")).toString();
        succeeded += outcome == QStringLiteral("Succeeded");
        failed += outcome == QStringLiteral("Failed");
        skipped += outcome == QStringLiteral("Skipped");
    }
    emit updateQueueChanged();
    setStatus(QStringLiteral("Update Center: %1 succeeded, %2 failed, %3 skipped.").arg(succeeded).arg(failed).arg(skipped));
    QTimer::singleShot(150, this, [this] {
        if (!m_busy && !m_bulkUpdateBusy && !m_updateQueueActive)
            checkAllUpdates();
    });
}

void InstallerManager::retryFailedUpdates(bool allowNonExactRenoDx) {
    if (m_busy || m_bulkUpdateBusy || m_updateQueueActive)
        return;
    const auto previous = m_updateResults;
    for (const auto &value : previous) {
        const auto result = value.toMap();
        if (result.value(QStringLiteral("outcome")).toString() == QStringLiteral("Failed"))
            enqueueUpdate(result.value(QStringLiteral("appId")).toString(), result.value(QStringLiteral("component")).toString());
    }
    if (m_updateQueue.isEmpty())
        return;
    m_updateResults.clear();
    m_updateQueueConfirmedRenoDxUrls.clear();
    if (allowNonExactRenoDx) {
        for (const auto &item : std::as_const(m_updateQueue)) {
            if (item.second != QStringLiteral("renodx"))
                continue;
            const int row = rowForAppId(item.first);
            const auto resolution = renoDxResolutionInfo(row);
            if (resolution.value(QStringLiteral("requiresConfirmation")).toBool())
                m_updateQueueConfirmedRenoDxUrls.insert(item.first, resolution.value(QStringLiteral("url")).toString());
        }
    }
    m_updateQueueAllowNonExactRenoDx = allowNonExactRenoDx;
    m_updateQueueActive = true;
    emit updateQueueChanged();
    processNextQueuedUpdate();
}

void InstallerManager::updateGames(const QVariantList &rows, bool allowNonExactRenoDx) {
    if (!m_games)
        return;
    if (m_updateQueueActive || m_busy || m_bulkUpdateBusy) {
        setStatus(QStringLiteral("An update operation is already running."));
        return;
    }
    for (const QVariant &value : rows) {
        bool ok = false;
        const int row = value.toInt(&ok);
        if (!ok)
            continue;
        const auto *game = m_games->game(row);
        if (!game)
            continue;
        const QVariantMap info = m_updateInfo.value(game->appId);
        if (info.value(QStringLiteral("reshadeUpdateAvailable")).toBool()) {
            if (info.value(QStringLiteral("directReShadeUpdateDetected")).toBool() && game->reshadeManaged)
                enqueueUpdate(game->appId, QStringLiteral("reshade"));
            if (info.value(QStringLiteral("reShade64UpdateDetected")).toBool() && game->reshade64Managed)
                enqueueUpdate(game->appId, QStringLiteral("reshade64"));
        }
        if (info.value(QStringLiteral("renodxUpdateAvailable")).toBool() && game->renodxManaged)
            enqueueUpdate(game->appId, QStringLiteral("renodx"));
        if (info.value(QStringLiteral("reframeworkUpdateAvailable")).toBool() && game->reframeworkManaged)
            enqueueUpdate(game->appId, QStringLiteral("reframework"));
    }
    if (m_updateQueue.isEmpty()) {
        setStatus(QStringLiteral("No compatible, unskipped Reno119-managed updates were selected."));
        return;
    }
    m_updateResults.clear();
    m_updateQueueConfirmedRenoDxUrls.clear();
    if (allowNonExactRenoDx) {
        for (const auto &item : std::as_const(m_updateQueue)) {
            if (item.second != QStringLiteral("renodx"))
                continue;
            const int row = rowForAppId(item.first);
            const auto resolution = renoDxResolutionInfo(row);
            if (resolution.value(QStringLiteral("requiresConfirmation")).toBool())
                m_updateQueueConfirmedRenoDxUrls.insert(item.first, resolution.value(QStringLiteral("url")).toString());
        }
    }
    m_updateQueueAllowNonExactRenoDx = allowNonExactRenoDx;
    m_updateQueueActive = true;
    emit updateQueueChanged();
    processNextQueuedUpdate();
}
