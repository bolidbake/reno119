#include "OptiScalerIntegration.h"
#include "../core/GameModel.h"
#include "../core/ModDetectionService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>


OptiScalerIntegration::OptiScalerIntegration(GameModel *games, QObject *parent)
    : QObject(parent), m_games(games) {}

QStringList OptiScalerIntegration::supportedProxies() {
    return ModDetectionService::optiScalerProxyNames();
}

QString OptiScalerIntegration::exeDirFor(int row) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    return (!g || g->exePath.isEmpty()) ? QString() : QFileInfo(g->exePath).absolutePath();
}

QString OptiScalerIntegration::integrationStatePath(int row) const {
    const QString dir = exeDirFor(row);
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/.reno119-optiscaler.json");
}

QString OptiScalerIntegration::appStatePath(int row) const {
    const QString dir = exeDirFor(row);
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/.reno119-state.json");
}

QVariantMap OptiScalerIntegration::readIntegrationState(int row) const {
    QFile f(integrationStatePath(row));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}

bool OptiScalerIntegration::writeIntegrationState(int row, const QVariantMap &state) const {
    const QString path = integrationStatePath(row);
    if (path.isEmpty())
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(QJsonObject::fromVariantMap(state)).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString OptiScalerIntegration::readManagedReShadePath(int row) const {
    QFile f(appStatePath(row));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    const QString path = obj.value(QStringLiteral("reshadeProxy")).toString();
    // v0.5.5 tracks the explicit OptiScaler chain-loader separately.  Treat a
    // legacy state that stored ReShade64.dll in reshadeProxy as chain-loader
    // state rather than as a normal game proxy.
    if (path.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0)
        return {};
    return path;
}

QString OptiScalerIntegration::readReno119ReShade64Path(int row) const {
    QFile f(appStatePath(row));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    QString path = obj.value(QStringLiteral("reshade64File")).toString();
    if (path.isEmpty() && obj.value(QStringLiteral("reshadeProxy")).toString().compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0)
        path = QStringLiteral("ReShade64.dll");
    return path;
}

bool OptiScalerIntegration::writeManagedReShadePath(int row, const QString &relativePath) const {
    const QString path = appStatePath(row);
    if (path.isEmpty())
        return false;
    QFile f(path);
    QJsonObject obj;
    if (f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    obj.insert(QStringLiteral("reshadeProxy"), relativePath);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return out.commit();
}

QString OptiScalerIntegration::findOptiIni(const QString &exeDir) const {
    QDir d(exeDir);
    const auto files = d.entryInfoList(QDir::Files | QDir::NoSymLinks);
    for (const auto &fi : files) {
        if (fi.fileName().compare(QStringLiteral("OptiScaler.ini"), Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath();
    }
    return {};
}


QString OptiScalerIntegration::detectOptiProxy(int row, const QString &exeDir, const QString &reshadePath) const {
    const QVariantMap state = readIntegrationState(row);
    const QString expected = state.value(QStringLiteral("optiProxy")).toString();
    return ModDetectionService::detectOptiScalerProxy(exeDir,
                                                       reshadePath,
                                                       expected,
                                                       !findOptiIni(exeDir).isEmpty());
}

QString OptiScalerIntegration::normalizeProxyChoice(const QString &choice) {
    const QString trimmed = choice.trimmed();
    for (const QString &proxy : supportedProxies()) {
        if (trimmed.compare(proxy, Qt::CaseInsensitive) == 0)
            return proxy;
    }
    const QString lower = trimmed.toLower();
    if (lower == QStringLiteral("auto"))
        return QStringLiteral("auto");
    return QStringLiteral("recommended");
}


QString OptiScalerIntegration::normalizeMethodChoice(const QString &choice) {
    const QString lower = choice.trimmed().toLower();
    if (lower == QStringLiteral("separate") || lower == QStringLiteral("loadreshade") || lower == QStringLiteral("plugins"))
        return lower;
    return QStringLiteral("recommended");
}

QString OptiScalerIntegration::proxyChoiceFromIndex(int index) {
    const QStringList values = {
        QStringLiteral("recommended"), QStringLiteral("auto"), QStringLiteral("dxgi.dll"),
        QStringLiteral("winmm.dll"), QStringLiteral("d3d12.dll"), QStringLiteral("dbghelp.dll"),
        QStringLiteral("version.dll"), QStringLiteral("wininet.dll"), QStringLiteral("winhttp.dll"),
        QStringLiteral("OptiScaler.asi")
    };
    return values.value(qBound(0, index, int(values.size()) - 1), QStringLiteral("recommended"));
}

QString OptiScalerIntegration::methodChoiceFromIndex(int index) {
    const QStringList values = {
        QStringLiteral("recommended"), QStringLiteral("separate"),
        QStringLiteral("loadreshade"), QStringLiteral("plugins")
    };
    return values.value(qBound(0, index, int(values.size()) - 1), QStringLiteral("recommended"));
}

int OptiScalerIntegration::proxyChoiceIndex(const QString &choice) {
    const QString normalized = normalizeProxyChoice(choice);
    const QStringList values = {
        QStringLiteral("recommended"), QStringLiteral("auto"), QStringLiteral("dxgi.dll"),
        QStringLiteral("winmm.dll"), QStringLiteral("d3d12.dll"), QStringLiteral("dbghelp.dll"),
        QStringLiteral("version.dll"), QStringLiteral("wininet.dll"), QStringLiteral("winhttp.dll"),
        QStringLiteral("OptiScaler.asi")
    };
    const int index = values.indexOf(normalized);
    return index < 0 ? 0 : index;
}

int OptiScalerIntegration::methodChoiceIndex(const QString &choice) {
    const QString normalized = normalizeMethodChoice(choice);
    if (normalized == QStringLiteral("separate")) return 1;
    if (normalized == QStringLiteral("loadreshade")) return 2;
    if (normalized == QStringLiteral("plugins")) return 3;
    return 0;
}

QString OptiScalerIntegration::resolveProxy(const QString &choice, const QString &currentProxy, const QString &reshadePath, const QString &exeDir) const {
    const QString normalized = normalizeProxyChoice(choice);
    if (normalized != QStringLiteral("recommended") && normalized != QStringLiteral("auto"))
        return normalized;

    const auto safeExisting = [&]() -> QString {
        for (const QString &proxy : supportedProxies()) {
            if (proxy.compare(currentProxy, Qt::CaseInsensitive) == 0 &&
                proxy.compare(reshadePath, Qt::CaseInsensitive) != 0)
                return proxy;
        }
        return {};
    };

    if (normalized == QStringLiteral("recommended")) {
        const QString existing = safeExisting();
        if (!existing.isEmpty())
            return existing;
    }

    const QStringList order = normalized == QStringLiteral("recommended")
        ? QStringList{QStringLiteral("dxgi.dll"), QStringLiteral("winmm.dll"), QStringLiteral("d3d12.dll"), QStringLiteral("version.dll"), QStringLiteral("dbghelp.dll"), QStringLiteral("wininet.dll"), QStringLiteral("winhttp.dll")}
        : QStringList{QStringLiteral("dxgi.dll"), QStringLiteral("winmm.dll"), QStringLiteral("d3d12.dll"), QStringLiteral("dbghelp.dll"), QStringLiteral("version.dll"), QStringLiteral("wininet.dll"), QStringLiteral("winhttp.dll")};

    for (const QString &candidate : order) {
        if (candidate.compare(reshadePath, Qt::CaseInsensitive) == 0)
            continue;
        const QString candidatePath = exeDir + QStringLiteral("/") + candidate;
        if (!QFileInfo::exists(candidatePath) || candidate.compare(currentProxy, Qt::CaseInsensitive) == 0)
            return candidate;
    }
    return QStringLiteral("winmm.dll");
}

QString OptiScalerIntegration::resolveMethod(const QString &choice, const QString &targetProxy, const QString &reshadePath) const {
    const QString lower = choice.trimmed().toLower();
    if (lower == QStringLiteral("separate") || lower == QStringLiteral("loadreshade") || lower == QStringLiteral("plugins"))
        return lower;
    if (reshadePath.isEmpty())
        return QStringLiteral("separate");
    if (targetProxy.compare(reshadePath, Qt::CaseInsensitive) != 0)
        return QStringLiteral("separate");
    return QStringLiteral("loadreshade");
}

QString OptiScalerIntegration::proxyBaseName(const QString &filename) {
    QString base = QFileInfo(filename).completeBaseName();
    if (base.isEmpty())
        base = filename;
    return base;
}

QString OptiScalerIntegration::hashFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

bool OptiScalerIntegration::moveRelative(const QString &exeDir, const QString &from, const QString &to) const {
    if (from == to)
        return true;
    const QString src = exeDir + QStringLiteral("/") + from;
    const QString dst = exeDir + QStringLiteral("/") + to;
    if (!QFileInfo::exists(src) || QFileInfo::exists(dst))
        return false;
    QDir().mkpath(QFileInfo(dst).absolutePath());
    return QFile::rename(src, dst);
}

void OptiScalerIntegration::appendHistory(QVariantMap &state, const QString &entry) {
    QVariantList historyList = state.value(QStringLiteral("history")).toList();
    historyList.prepend(QDateTime::currentDateTime().toString(Qt::ISODate) + QStringLiteral(" — ") + entry);
    while (historyList.size() > 20)
        historyList.removeLast();
    state["history"] = historyList;
}

bool OptiScalerIntegration::apply(int row, const QString &proxyChoice, const QString &methodChoice) {
    beginStatusForRow(row);
    Plan plan = makePlan(row, proxyChoice, methodChoice);
    if (!plan.canApply) {
        setStatus(plan.summary);
        return false;
    }

    // Keep the selected UI choices in the same transaction state as the files.
    // Older builds stored these only in QSettings, which made Revert restore the
    // filesystem while leaving the dropdowns pointing at the newer selection.
    QVariantMap preApplyState = readIntegrationState(row);
    bool migratedLegacyChoices = false;
    if (!preApplyState.contains(QStringLiteral("proxyChoice"))) {
        preApplyState["proxyChoice"] = proxyChoiceFromIndex(m_games ? m_games->optiProxyChoiceIndex(row) : 0);
        migratedLegacyChoices = true;
    }
    if (!preApplyState.contains(QStringLiteral("methodChoice"))) {
        preApplyState["methodChoice"] = methodChoiceFromIndex(m_games ? m_games->optiMethodChoiceIndex(row) : 0);
        migratedLegacyChoices = true;
    }
    if (migratedLegacyChoices && !writeIntegrationState(row, preApplyState)) {
        setStatus(QStringLiteral("Could not prepare the OptiScaler integration state; no changes were applied."));
        return false;
    }

    const QString snapshot = createTransactionSnapshot(row, plan.touchedRelativePaths);
    if (snapshot.isEmpty()) {
        setStatus(QStringLiteral("Could not create the integration transaction backup; no changes were applied."));
        return false;
    }

    const auto rollback = [&]() {
        restoreTransactionSnapshot(row, snapshot);
        m_games->refreshInstallState(row);
    };

    const bool reshadeMoves = !plan.currentReShadePath.isEmpty() && !plan.targetReShadePath.isEmpty() &&
        plan.currentReShadePath != plan.targetReShadePath;
    const bool optiMoves = plan.currentOptiProxy != plan.targetOptiProxy;

    const auto moveReShade = [&]() -> bool {
        if (!reshadeMoves)
            return true;
        return moveRelative(plan.exeDir, plan.currentReShadePath, plan.targetReShadePath) &&
            writeManagedReShadePath(row, plan.targetReShadePath);
    };
    const auto moveOpti = [&]() -> bool {
        if (!optiMoves)
            return true;
        return moveRelative(plan.exeDir, plan.currentOptiProxy, plan.targetOptiProxy);
    };

    // Choose the move order based on which current filename must be freed.
    // Separate -> LoadReshade commonly needs ReShade moved first so OptiScaler
    // can take dxgi.dll. LoadReshade -> separate commonly needs the opposite.
    const bool optiMustMoveFirst = reshadeMoves && optiMoves &&
        plan.targetReShadePath.compare(plan.currentOptiProxy, Qt::CaseInsensitive) == 0;

    if (optiMustMoveFirst) {
        if (!moveOpti() || !moveReShade()) {
            rollback();
            setStatus(QStringLiteral("Could not rearrange the OptiScaler/ReShade proxies; the transaction was reverted."));
            return false;
        }
    } else {
        if (!moveReShade() || !moveOpti()) {
            rollback();
            setStatus(QStringLiteral("Could not rearrange the OptiScaler/ReShade proxies; the transaction was reverted."));
            return false;
        }
    }

    if (!plan.iniPath.isEmpty() && !plan.targetLoadReShade.isEmpty() &&
        plan.currentLoadReShade.compare(plan.targetLoadReShade, Qt::CaseInsensitive) != 0) {
        if (!writeIniValue(plan.iniPath, QStringLiteral("Plugins"), QStringLiteral("LoadReshade"), plan.targetLoadReShade)) {
            rollback();
            setStatus(QStringLiteral("Could not update OptiScaler.ini; the transaction was reverted."));
            return false;
        }
    }

    QVariantMap state = readIntegrationState(row);
    if (!state.contains(QStringLiteral("originalReshadePath")) && !plan.currentReShadePath.isEmpty()) {
        const bool indirect = plan.currentReShadePath.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) == 0 ||
            plan.currentReShadePath.startsWith(QStringLiteral("OptiScaler/plugins/"), Qt::CaseInsensitive);
        QString original = plan.currentReShadePath;
        if (indirect) {
            const auto *game = m_games ? m_games->game(row) : nullptr;
            if (game) {
                if (game->graphicsApi == QStringLiteral("DirectX 9")) original = QStringLiteral("d3d9.dll");
                else if (game->graphicsApi == QStringLiteral("OpenGL")) original = QStringLiteral("opengl32.dll");
                else if (game->graphicsApi.startsWith(QStringLiteral("DirectX"))) original = QStringLiteral("dxgi.dll");
            }
        }
        state["originalReshadePath"] = original;
    }
    state["method"] = plan.method;
    state["proxyChoice"] = normalizeProxyChoice(proxyChoice);
    state["methodChoice"] = normalizeMethodChoice(methodChoice);
    state["optiProxy"] = plan.targetOptiProxy;
    state["reshadePath"] = plan.targetReShadePath;
    state["expectedLoadReshade"] = plan.targetLoadReShade;
    state["lastTransactionDir"] = snapshot;
    state["wineOverrides"] = plan.wineOverrides;
    state["iniHashAfterApply"] = plan.iniPath.isEmpty() ? QString() : hashFile(plan.iniPath);
    QVariantList restorePointList = state.value(QStringLiteral("restorePoints")).toList();
    QVariantMap restorePoint;
    restorePoint[QStringLiteral("snapshotDir")] = snapshot;
    restorePoint[QStringLiteral("createdAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    restorePoint[QStringLiteral("label")] = QStringLiteral("Before %1 integration using %2").arg(plan.method, plan.targetOptiProxy);
    restorePoint[QStringLiteral("proxy")] = plan.currentOptiProxy;
    restorePoint[QStringLiteral("method")] = preApplyState.value(QStringLiteral("method")).toString();
    restorePointList.prepend(restorePoint);
    while (restorePointList.size() > 12)
        restorePointList.removeLast();
    state[QStringLiteral("restorePoints")] = restorePointList;
    appendHistory(state, QStringLiteral("Applied %1 integration using %2").arg(plan.method, plan.targetOptiProxy));
    if (!writeIntegrationState(row, state)) {
        rollback();
        setStatus(QStringLiteral("Could not record integration state; the transaction was reverted."));
        return false;
    }

    if (m_games) {
        m_games->setOptiProxyChoiceIndex(row, proxyChoiceIndex(proxyChoice));
        m_games->setOptiMethodChoiceIndex(row, methodChoiceIndex(methodChoice));
        m_games->refreshInstallState(row);
    }
    bumpRevision();
    setStatus(QStringLiteral("Applied proxy and integration method. The previous state can be restored with Revert."));
    return true;
}

bool OptiScalerIntegration::fixSetup(int row) {
    const QVariantMap before = readIntegrationState(row);
    const QString savedProxyChoice = before.value(QStringLiteral("proxyChoice")).toString();
    const QString savedMethodChoice = before.value(QStringLiteral("methodChoice")).toString();
    const QString appliedProxy = before.value(QStringLiteral("optiProxy")).toString();
    const QString appliedMethod = before.value(QStringLiteral("method")).toString();
    if (savedProxyChoice.isEmpty() || savedMethodChoice.isEmpty() || appliedProxy.isEmpty() || appliedMethod.isEmpty()) {
        beginStatusForRow(row);
        setStatus(QStringLiteral("No previously applied OptiScaler proxy/integration layout is available to fix."));
        return false;
    }

    // Repair the exact last applied layout rather than resolving Recommended/Auto
    // again. This matters when a loader was renamed externally: a fresh Auto
    // resolution might accept the new filename instead of restoring the known-good
    // target that Reno119 previously applied.
    if (!apply(row, appliedProxy, appliedMethod))
        return false;

    QVariantMap repaired = readIntegrationState(row);
    repaired[QStringLiteral("proxyChoice")] = savedProxyChoice;
    repaired[QStringLiteral("methodChoice")] = savedMethodChoice;
    appendHistory(repaired, QStringLiteral("Repaired drift back to the last known-good proxy/integration layout"));
    if (!writeIntegrationState(row, repaired)) {
        setStatus(QStringLiteral("The integration files were repaired, but Reno119 could not restore the saved dropdown-choice metadata."));
        return false;
    }
    if (m_games) {
        m_games->setOptiProxyChoiceIndex(row, proxyChoiceIndex(savedProxyChoice));
        m_games->setOptiMethodChoiceIndex(row, methodChoiceIndex(savedMethodChoice));
        m_games->refreshInstallState(row);
    }
    bumpRevision();
    setStatus(QStringLiteral("Fixed the ReShade/OptiScaler integration by restoring the last known-good proxy and integration method."));
    return true;
}

bool OptiScalerIntegration::revert(int row) {
    beginStatusForRow(row);
    const QVariantMap before = readIntegrationState(row);
    const QString snapshot = before.value(QStringLiteral("lastTransactionDir")).toString();
    if (snapshot.isEmpty()) {
        setStatus(QStringLiteral("No OptiScaler integration transaction is available to revert."));
        return false;
    }
    if (!restoreTransactionSnapshot(row, snapshot)) {
        setStatus(QStringLiteral("Could not restore the previous OptiScaler/ReShade integration state."));
        return false;
    }
    QVariantMap restored = readIntegrationState(row);
    appendHistory(restored, QStringLiteral("Reverted the last OptiScaler/ReShade integration transaction"));
    if (!writeIntegrationState(row, restored)) {
        setStatus(QStringLiteral("Files were restored, but Reno119 could not record the reverted integration state."));
        return false;
    }
    if (m_games) {
        const QString restoredProxyChoice = restored.value(QStringLiteral("proxyChoice"), QStringLiteral("recommended")).toString();
        const QString restoredMethodChoice = restored.value(QStringLiteral("methodChoice"), QStringLiteral("recommended")).toString();
        m_games->setOptiProxyChoiceIndex(row, proxyChoiceIndex(restoredProxyChoice));
        m_games->setOptiMethodChoiceIndex(row, methodChoiceIndex(restoredMethodChoice));
        m_games->refreshInstallState(row);
    }
    bumpRevision();
    setStatus(QStringLiteral("Reverted the last OptiScaler/ReShade integration transaction and restored its selections."));
    return true;
}

QString OptiScalerIntegration::statusLevel(const QString &status) {
    const QString lower = status.toLower();
    if (lower.contains(QStringLiteral("could not")) ||
        lower.startsWith(QStringLiteral("no ")) ||
        lower.contains(QStringLiteral("not detected")) ||
        lower.contains(QStringLiteral("cannot")) ||
        lower.contains(QStringLiteral("requires")) ||
        lower.startsWith(QStringLiteral("install ")) ||
        lower.contains(QStringLiteral("will not overwrite")) ||
        lower.contains(QStringLiteral("unknown")) ||
        lower.contains(QStringLiteral("conflict")))
        return QStringLiteral("error");
    if (lower.startsWith(QStringLiteral("applied ")) ||
        lower.startsWith(QStringLiteral("reverted ")) ||
        lower.startsWith(QStringLiteral("saved ")) ||
        lower.startsWith(QStringLiteral("restored ")))
        return QStringLiteral("success");
    return QStringLiteral("info");
}

void OptiScalerIntegration::beginStatusForRow(int row) {
    m_activeStatusAppId.clear();
    if (const auto *g = m_games ? m_games->game(row) : nullptr)
        m_activeStatusAppId = g->appId;
}

QVariantMap OptiScalerIntegration::inlineStatus(int row) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return {};
    return m_inlineStatus.value(g->appId);
}

void OptiScalerIntegration::setStatus(const QString &status) {
    if (!m_activeStatusAppId.isEmpty()) {
        const QVariantMap next{{QStringLiteral("text"), status},
                               {QStringLiteral("level"), statusLevel(status)}};
        if (m_inlineStatus.value(m_activeStatusAppId) != next) {
            m_inlineStatus.insert(m_activeStatusAppId, next);
            ++m_statusRevision;
            emit statusRevisionChanged();
        }
    }
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void OptiScalerIntegration::bumpRevision() {
    ++m_revision;
    emit revisionChanged();
}
