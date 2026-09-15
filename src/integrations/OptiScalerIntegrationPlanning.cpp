#include "OptiScalerIntegration.h"
#include "../core/GameModel.h"
#include "../core/ModDetectionService.h"

#include <QFileInfo>

QVariantMap OptiScalerIntegration::analysisMap(int row) const {
    QVariantMap out;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty()) {
        out["detected"] = false;
        out["summary"] = QStringLiteral("No game executable selected.");
        out["proxyScanSummary"] = QStringLiteral("No executable is available to scan.");
        return out;
    }

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    const QString normalReShadePath = readManagedReShadePath(row);
    const QString reshade64Path = readReno119ReShade64Path(row);
    const QString ini = findOptiIni(exeDir);
    const QString optiProxy = detectOptiProxy(row, exeDir, normalReShadePath);
    const bool detected = !ini.isEmpty() || QFileInfo::exists(exeDir + QStringLiteral("/OptiScaler.dll")) || !optiProxy.isEmpty();
    const QString loadReshade = ini.isEmpty() ? QString() : readIniValue(ini, QStringLiteral("Plugins"), QStringLiteral("LoadReshade"));
    const bool loadReShadeEnabled = loadReshade.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    const QString activeReShadePath = loadReShadeEnabled && !reshade64Path.isEmpty() ? reshade64Path : normalReShadePath;
    const QVariantMap state = readIntegrationState(row);

    bool drift = false;
    QStringList driftDetails;
    const QString expectedLoad = state.value(QStringLiteral("expectedLoadReshade")).toString();
    if (!expectedLoad.isEmpty() && expectedLoad.compare(loadReshade, Qt::CaseInsensitive) != 0) {
        drift = true;
        driftDetails << QStringLiteral("OptiScaler.ini LoadReshade changed (%1 → %2)").arg(expectedLoad, loadReshade.isEmpty() ? QStringLiteral("missing") : loadReshade);
    }
    const QString expectedOpti = state.value(QStringLiteral("optiProxy")).toString();
    if (!expectedOpti.isEmpty() && !QFileInfo::exists(exeDir + QStringLiteral("/") + expectedOpti)) {
        drift = true;
        driftDetails << QStringLiteral("Expected OptiScaler proxy is missing: %1").arg(expectedOpti);
    }
    const QString expectedReshade = state.value(QStringLiteral("reshadePath")).toString();
    if (!expectedReshade.isEmpty() && !QFileInfo::exists(exeDir + QStringLiteral("/") + expectedReshade)) {
        drift = true;
        driftDetails << QStringLiteral("Expected ReShade integration file is missing: %1").arg(expectedReshade);
    }

    bool conflict = false;
    QString conflictText;
    QStringList issues;
    QStringList proxyWarnings;
    QStringList proxyConflicts;
    QVariantList proxyScan;
    int optiLikeCount = 0;

    QStringList scanNames = supportedProxies();
    scanNames << QStringLiteral("OptiScaler.dll") << QStringLiteral("ReShade64.dll");
    scanNames.removeDuplicates();
    for (const QString &name : scanNames) {
        const QString path = exeDir + QStringLiteral("/") + name;
        if (!QFileInfo::exists(path) || !QFileInfo(path).isFile())
            continue;

        QString role = QStringLiteral("Unidentified proxy / game DLL");
        QString severity = QStringLiteral("info");
        const bool isRecordedReShade = !normalReShadePath.isEmpty() && name.compare(normalReShadePath, Qt::CaseInsensitive) == 0;
        const bool isRecordedReShade64 = !reshade64Path.isEmpty() && name.compare(reshade64Path, Qt::CaseInsensitive) == 0;
        const bool isOpti = name.compare(optiProxy, Qt::CaseInsensitive) == 0 || ModDetectionService::looksLikeOptiScalerBinary(path);
        const bool isReShade = isRecordedReShade || isRecordedReShade64 || ModDetectionService::looksLikeReShadeBinary(path);

        if (isOpti) {
            const bool isCoreDll = name.compare(QStringLiteral("OptiScaler.dll"), Qt::CaseInsensitive) == 0;
            role = isCoreDll ? QStringLiteral("OptiScaler core DLL") : QStringLiteral("OptiScaler loader/proxy");
            if (!isCoreDll)
                ++optiLikeCount;
        } else if (isRecordedReShade64) {
            role = QStringLiteral("Reno119-managed ReShade64 chain-loader");
        } else if (isRecordedReShade) {
            role = QStringLiteral("Reno119-managed ReShade proxy");
        } else if (isReShade) {
            role = QStringLiteral("ReShade-like DLL (not owned by this integration state)");
            severity = QStringLiteral("warning");
        } else if (supportedProxies().contains(name, Qt::CaseInsensitive)) {
            severity = QStringLiteral("warning");
            proxyWarnings << QStringLiteral("%1 exists but Reno119 cannot identify its owner. It will never be overwritten implicitly.").arg(name);
        }

        QVariantMap entry;
        entry[QStringLiteral("file")] = name;
        entry[QStringLiteral("role")] = role;
        entry[QStringLiteral("severity")] = severity;
        proxyScan << entry;
    }

    if (!normalReShadePath.isEmpty() && !optiProxy.isEmpty() && normalReShadePath.compare(optiProxy, Qt::CaseInsensitive) == 0) {
        conflict = true;
        conflictText = QStringLiteral("Normal ReShade and OptiScaler appear to target the same proxy filename.");
        proxyConflicts << conflictText;
        issues << conflictText;
    }
    if (optiLikeCount > 1) {
        const QString text = QStringLiteral("Multiple OptiScaler-like DLLs are present in the game directory. Verify which one is actually being loaded before applying proxy changes.");
        proxyConflicts << text;
        issues << text;
        conflict = true;
        if (conflictText.isEmpty())
            conflictText = text;
    }
    if (detected && ini.isEmpty())
        issues << QStringLiteral("OptiScaler was detected, but OptiScaler.ini was not found beside this executable.");
    if (detected && optiProxy.isEmpty())
        issues << QStringLiteral("OptiScaler is present, but its active proxy DLL could not be identified safely.");
    if (!normalReShadePath.isEmpty() && !QFileInfo::exists(exeDir + QStringLiteral("/") + normalReShadePath))
        issues << QStringLiteral("Reno119 has a normal ReShade proxy recorded, but that file is missing.");
    if (!reshade64Path.isEmpty() && !QFileInfo::exists(exeDir + QStringLiteral("/") + reshade64Path))
        issues << QStringLiteral("Reno119 has ReShade64.dll recorded, but that file is missing.");
    if (g->renodxInstalled && normalReShadePath.isEmpty() && reshade64Path.isEmpty())
        issues << QStringLiteral("RenoDX is recorded as installed without a Reno119-managed ReShade loader.");
    if (loadReShadeEnabled && reshade64Path.compare(QStringLiteral("ReShade64.dll"), Qt::CaseInsensitive) != 0)
        issues << QStringLiteral("OptiScaler LoadReshade is enabled, but Reno119-managed ReShade64.dll is not installed.");
    if (loadReShadeEnabled && !normalReShadePath.isEmpty())
        issues << QStringLiteral("A normal ReShade proxy is also installed while OptiScaler LoadReshade is enabled; ensure your launch options do not load both paths.");

    const bool hasAppliedChoices = !state.value(QStringLiteral("proxyChoice")).toString().isEmpty() &&
                                   !state.value(QStringLiteral("methodChoice")).toString().isEmpty();
    Plan repairPlan;
    if (hasAppliedChoices)
        repairPlan = makePlan(row,
                              state.value(QStringLiteral("optiProxy"), state.value(QStringLiteral("proxyChoice"))).toString(),
                              state.value(QStringLiteral("method"), state.value(QStringLiteral("methodChoice"))).toString());
    const bool ambiguousOptiLoaders = optiLikeCount > 1;
    const bool fixAvailable = detected && hasAppliedChoices && !ambiguousOptiLoaders &&
                              repairPlan.canApply && (drift || conflict);

    out["detected"] = detected;
    out["iniPath"] = ini;
    out["optiProxy"] = optiProxy;
    out["reshadePath"] = activeReShadePath;
    out["normalReShadePath"] = normalReShadePath;
    out["reshade64Path"] = reshade64Path;
    out["loadReshade"] = loadReshade;
    out["renodxInstalled"] = g->renodxInstalled;
    out["conflict"] = conflict;
    out["conflictText"] = conflictText;
    out["issues"] = issues;
    out["health"] = !detected ? QStringLiteral("Not detected") : (issues.isEmpty() ? QStringLiteral("No obvious ReShade/RenoDX integration conflicts") : QStringLiteral("Review suggested"));
    out["drift"] = drift;
    out["driftDetails"] = driftDetails;
    out["appliedMethod"] = state.value(QStringLiteral("method")).toString();
    out["lastTransactionDir"] = state.value(QStringLiteral("lastTransactionDir")).toString();
    out["workingSaved"] = state.contains(QStringLiteral("workingConfig"));
    out["wineOverrides"] = state.value(QStringLiteral("wineOverrides")).toString();
    out["steamLaunchOptions"] = out.value(QStringLiteral("wineOverrides")).toString().isEmpty()
        ? QString()
        : out.value(QStringLiteral("wineOverrides")).toString() + QStringLiteral(" %command%");
    out["proxyScan"] = proxyScan;
    out["proxyWarnings"] = proxyWarnings;
    out["proxyConflicts"] = proxyConflicts;
    out["proxyConflictCount"] = proxyConflicts.size();
    out["proxyScanSummary"] = proxyScan.isEmpty()
        ? QStringLiteral("No common ReShade/OptiScaler proxy filenames were found beside the game executable.")
        : QStringLiteral("Scanned %1 relevant DLL%2: %3 conflict%4, %5 caution%6.")
              .arg(proxyScan.size())
              .arg(proxyScan.size() == 1 ? QString() : QStringLiteral("s"))
              .arg(proxyConflicts.size())
              .arg(proxyConflicts.size() == 1 ? QString() : QStringLiteral("s"))
              .arg(proxyWarnings.size())
              .arg(proxyWarnings.size() == 1 ? QString() : QStringLiteral("s"));
    out["fixAvailable"] = fixAvailable;
    out["fixSummary"] = !hasAppliedChoices
        ? QStringLiteral("Apply an OptiScaler proxy/integration choice once before Reno119 can restore that known-good layout automatically.")
        : (ambiguousOptiLoaders
            ? QStringLiteral("Automatic repair is disabled while multiple OptiScaler-like loader proxies are present. Resolve the ambiguity first.")
            : (fixAvailable
                ? QStringLiteral("Reno119 can re-apply the last successfully applied proxy and integration method as a new rollback-safe transaction.")
                : ((drift || conflict) && !repairPlan.canApply
                    ? QStringLiteral("Automatic repair cannot safely run yet: ") + repairPlan.summary
                    : QStringLiteral("The last applied proxy/integration layout does not currently need an automatic repair."))));
    out["summary"] = !detected
        ? QStringLiteral("OptiScaler not detected beside this executable.")
        : QStringLiteral("OptiScaler detected%1%2.")
              .arg(optiProxy.isEmpty() ? QString() : QStringLiteral(" as ") + optiProxy,
                   drift ? QStringLiteral(" • relevant integration settings changed externally") : QString());
    return out;
}
QVariantMap OptiScalerIntegration::analyze(int row) const {
    return analysisMap(row);
}

OptiScalerIntegration::Plan OptiScalerIntegration::makePlan(int row, const QString &proxyChoice, const QString &methodChoice) const {
    Plan plan;
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g || g->exePath.isEmpty()) {
        plan.summary = QStringLiteral("No game executable selected.");
        return plan;
    }

    plan.exeDir = QFileInfo(g->exePath).absolutePath();
    plan.iniPath = findOptiIni(plan.exeDir);
    const QString normalReShadePath = readManagedReShadePath(row);
    const QString reshade64Path = readReno119ReShade64Path(row);
    const QVariantMap integrationState = readIntegrationState(row);
    const QString requestedMethod = normalizeMethodChoice(methodChoice);
    const QString appliedMethod = integrationState.value(QStringLiteral("method")).toString();

    if (requestedMethod == QStringLiteral("recommended")) {
        if (!appliedMethod.isEmpty())
            plan.method = appliedMethod;
        else if (!reshade64Path.isEmpty() && !plan.iniPath.isEmpty())
            plan.method = QStringLiteral("loadreshade");
        else
            plan.method = QStringLiteral("separate");
    } else {
        plan.method = requestedMethod;
    }

    plan.currentReShadePath = plan.method == QStringLiteral("loadreshade") ? reshade64Path : normalReShadePath;
    plan.currentOptiProxy = detectOptiProxy(row, plan.exeDir, normalReShadePath);
    plan.detected = !plan.iniPath.isEmpty() || QFileInfo::exists(plan.exeDir + QStringLiteral("/OptiScaler.dll")) || !plan.currentOptiProxy.isEmpty();
    if (!plan.detected) {
        plan.summary = QStringLiteral("OptiScaler is not detected. Install it with your preferred tool first, then analyze again.");
        return plan;
    }

    if (plan.method == QStringLiteral("loadreshade")) {
        if (g->architecture != QStringLiteral("x64")) {
            plan.summary = QStringLiteral("Load ReShade via OptiScaler requires an x64 game and ReShade64.dll.");
            return plan;
        }
        if (reshade64Path.isEmpty() || !QFileInfo::exists(plan.exeDir + QStringLiteral("/") + reshade64Path)) {
            plan.summary = QStringLiteral("Install ReShade64 in the OptiScaler section first. Reno119 no longer renames the normal ReShade proxy behind the scenes.");
            return plan;
        }
        if (plan.iniPath.isEmpty()) {
            plan.summary = QStringLiteral("Load ReShade via OptiScaler requires OptiScaler.ini so Reno119 can set [Plugins] LoadReshade=true safely.");
            return plan;
        }
        plan.currentReShadePath = reshade64Path;
    } else if (plan.method == QStringLiteral("plugins")) {
        if (normalReShadePath.isEmpty() || !QFileInfo::exists(plan.exeDir + QStringLiteral("/") + normalReShadePath)) {
            plan.summary = QStringLiteral("Install normal Reno119-managed ReShade before using the OptiScaler plugins-folder method.");
            return plan;
        }
        plan.currentReShadePath = normalReShadePath;
    } else if (plan.method == QStringLiteral("separate")) {
        plan.currentReShadePath = normalReShadePath;
    }

    plan.targetOptiProxy = resolveProxy(proxyChoice, plan.currentOptiProxy, plan.currentReShadePath, plan.exeDir);
    plan.currentLoadReShade = plan.iniPath.isEmpty() ? QString() : readIniValue(plan.iniPath, QStringLiteral("Plugins"), QStringLiteral("LoadReshade"));

    if (plan.targetOptiProxy.compare(QStringLiteral("OptiScaler.asi"), Qt::CaseInsensitive) == 0) {
        plan.summary = QStringLiteral("OptiScaler.asi is available as an advanced proxy choice, but automatic ASI-loader rewiring is intentionally out of scope for this ReShade/RenoDX helper.");
        return plan;
    }
    if (plan.currentOptiProxy.isEmpty()) {
        plan.summary = QStringLiteral("OptiScaler.ini was found, but Reno119 could not identify the currently loaded OptiScaler DLL. Choose/rename it manually first or place OptiScaler.dll beside the game.");
        return plan;
    }

    if (plan.currentOptiProxy != plan.targetOptiProxy) {
        const bool targetExists = QFileInfo::exists(plan.exeDir + QStringLiteral("/") + plan.targetOptiProxy);
        const bool targetIsManagedReShade = !plan.currentReShadePath.isEmpty() &&
            plan.targetOptiProxy.compare(plan.currentReShadePath, Qt::CaseInsensitive) == 0 &&
            plan.method == QStringLiteral("plugins");
        if (targetExists && !targetIsManagedReShade) {
            plan.summary = QStringLiteral("Cannot rename OptiScaler because %1 already exists.").arg(plan.targetOptiProxy);
            return plan;
        }
        plan.changes << QStringLiteral("OptiScaler proxy: %1 → %2").arg(plan.currentOptiProxy, plan.targetOptiProxy);
        plan.hasFileChanges = true;
        plan.touchedRelativePaths << plan.currentOptiProxy << plan.targetOptiProxy;
    }

    const auto fallbackReShadeProxy = [&]() -> QString {
        if (g->graphicsApi == QStringLiteral("DirectX 9")) return QStringLiteral("d3d9.dll");
        if (g->graphicsApi == QStringLiteral("OpenGL")) return QStringLiteral("opengl32.dll");
        if (g->graphicsApi.startsWith(QStringLiteral("DirectX"))) return QStringLiteral("dxgi.dll");
        return {};
    };
    QString originalReShadePath = integrationState.value(QStringLiteral("originalReshadePath")).toString();
    const bool currentReShadeIsIndirect = normalReShadePath.startsWith(QStringLiteral("OptiScaler/plugins/"), Qt::CaseInsensitive);
    if (originalReShadePath.isEmpty())
        originalReShadePath = currentReShadeIsIndirect ? fallbackReShadeProxy() : normalReShadePath;

    QStringList overrideDlls;
    if (plan.method == QStringLiteral("separate")) {
        plan.targetReShadePath = normalReShadePath.isEmpty() ? QString() : originalReShadePath;
        if (!plan.targetReShadePath.isEmpty() && plan.targetOptiProxy.compare(plan.targetReShadePath, Qt::CaseInsensitive) == 0) {
            plan.summary = QStringLiteral("Separate proxies cannot use the same filename for ReShade and OptiScaler. Choose another OptiScaler proxy or use ReShade64 with Load ReShade via OptiScaler.");
            return plan;
        }
        plan.targetLoadReShade = QStringLiteral("false");
        if (!normalReShadePath.isEmpty() && normalReShadePath != plan.targetReShadePath) {
            if (QFileInfo::exists(plan.exeDir + QStringLiteral("/") + plan.targetReShadePath)) {
                plan.summary = QStringLiteral("Cannot restore ReShade to %1 because that file already exists.").arg(plan.targetReShadePath);
                return plan;
            }
            plan.changes << QStringLiteral("ReShade: %1 → %2").arg(normalReShadePath, plan.targetReShadePath);
            plan.hasFileChanges = true;
            plan.touchedRelativePaths << normalReShadePath << plan.targetReShadePath << QStringLiteral(".reno119-state.json");
        }
        overrideDlls << proxyBaseName(plan.targetOptiProxy);
        if (!plan.targetReShadePath.isEmpty() && !plan.targetReShadePath.contains('/') && !plan.targetReShadePath.contains('\\'))
            overrideDlls << proxyBaseName(plan.targetReShadePath);
    } else if (plan.method == QStringLiteral("loadreshade")) {
        plan.targetReShadePath = QStringLiteral("ReShade64.dll");
        plan.targetLoadReShade = QStringLiteral("true");
        // ReShade64.dll is installed explicitly by InstallerManager. Apply only
        // configures OptiScaler to load it; it never renames the normal proxy.
        overrideDlls << proxyBaseName(plan.targetOptiProxy);
    } else if (plan.method == QStringLiteral("plugins")) {
        plan.targetReShadePath = QStringLiteral("OptiScaler/plugins/") + plan.targetOptiProxy;
        plan.targetLoadReShade = QStringLiteral("false");
        if (normalReShadePath != plan.targetReShadePath) {
            if (QFileInfo::exists(plan.exeDir + QStringLiteral("/") + plan.targetReShadePath)) {
                plan.summary = QStringLiteral("The target OptiScaler plugins file already exists, so Reno119 will not overwrite it.");
                return plan;
            }
            plan.changes << QStringLiteral("ReShade: %1 → %2").arg(normalReShadePath, plan.targetReShadePath);
            plan.hasFileChanges = true;
            plan.touchedRelativePaths << normalReShadePath << plan.targetReShadePath << QStringLiteral(".reno119-state.json");
        }
        overrideDlls << proxyBaseName(plan.targetOptiProxy);
    } else {
        plan.summary = QStringLiteral("Unknown integration method.");
        return plan;
    }

    if (!plan.iniPath.isEmpty() && plan.currentLoadReShade.compare(plan.targetLoadReShade, Qt::CaseInsensitive) != 0) {
        plan.changes << QStringLiteral("OptiScaler.ini [Plugins] LoadReshade: %1 → %2")
                            .arg(plan.currentLoadReShade.isEmpty() ? QStringLiteral("missing") : plan.currentLoadReShade,
                                 plan.targetLoadReShade);
        plan.hasFileChanges = true;
        plan.touchedRelativePaths << QFileInfo(plan.iniPath).fileName();
    }

    if (g->reframeworkInstalled)
        overrideDlls << QStringLiteral("dinput8");

    overrideDlls.removeDuplicates();
    if (!overrideDlls.isEmpty()) {
        QStringList pairs;
        for (const QString &dll : overrideDlls)
            pairs << dll + QStringLiteral("=n,b");
        // Advisory only: Reno119 never edits Steam launch options or injects
        // WINEDLLOVERRIDES into a launcher. The UI may offer this string for
        // manual copy when a user's Proton/Wine setup needs it.
        plan.wineOverrides = QStringLiteral("WINEDLLOVERRIDES=\"") + pairs.join(';') + QStringLiteral("\"");
        plan.steamLaunchOptions = plan.wineOverrides + QStringLiteral(" %command%");
    }

    plan.touchedRelativePaths.removeDuplicates();
    if (plan.changes.isEmpty())
        plan.changes << QStringLiteral("No file changes are required; the current integration layout already matches this selection.");
    plan.canApply = true;
    plan.summary = QStringLiteral("Preview ready. Nothing changes until Apply is pressed.");
    return plan;
}

QVariantMap OptiScalerIntegration::preview(int row, const QString &proxyChoice, const QString &methodChoice) const {
    const Plan plan = makePlan(row, proxyChoice, methodChoice);
    QVariantMap out;
    out["detected"] = plan.detected;
    out["canApply"] = plan.canApply;
    out["hasFileChanges"] = plan.hasFileChanges;
    out["currentProxy"] = plan.currentOptiProxy;
    out["targetProxy"] = plan.targetOptiProxy;
    out["method"] = plan.method;
    out["reshadePath"] = plan.currentReShadePath;
    out["targetReShadePath"] = plan.targetReShadePath;
    out["changes"] = plan.changes;
    out["wineOverrides"] = plan.wineOverrides;
    out["steamLaunchOptions"] = plan.steamLaunchOptions;
    out["summary"] = plan.summary;
    return out;
}

QVariantMap OptiScalerIntegration::appliedChoices(int row) const {
    const QVariantMap state = readIntegrationState(row);
    QString proxyChoice = state.value(QStringLiteral("proxyChoice")).toString();
    QString methodChoice = state.value(QStringLiteral("methodChoice")).toString();

    if (proxyChoice.isEmpty() && m_games)
        proxyChoice = proxyChoiceFromIndex(m_games->optiProxyChoiceIndex(row));
    if (methodChoice.isEmpty() && m_games)
        methodChoice = methodChoiceFromIndex(m_games->optiMethodChoiceIndex(row));

    proxyChoice = normalizeProxyChoice(proxyChoice);
    methodChoice = normalizeMethodChoice(methodChoice);

    QVariantMap out;
    out["proxyChoice"] = proxyChoice;
    out["methodChoice"] = methodChoice;
    out["proxyIndex"] = proxyChoiceIndex(proxyChoice);
    out["methodIndex"] = methodChoiceIndex(methodChoice);
    return out;
}

