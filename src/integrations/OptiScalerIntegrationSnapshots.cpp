#include "OptiScalerIntegration.h"
#include "../core/GameModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

QString OptiScalerIntegration::createTransactionSnapshot(int row, const QStringList &relativePaths) const {
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g)
        return {};
    QString root = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (root.isEmpty())
        root = QDir::homePath() + QStringLiteral("/.cache");
    root += QStringLiteral("/reno119");
    const QString safeId = QString(g->appId).replace(':', '_').replace('/', '_');
    const QString dir = root + QStringLiteral("/integration-backups/") + safeId + QStringLiteral("/") +
        QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss-zzz"));
    QDir().mkpath(dir + QStringLiteral("/files"));

    const QString exeDir = QFileInfo(g->exePath).absolutePath();
    QJsonArray items;
    QStringList unique = relativePaths;
    unique << QStringLiteral(".reno119-optiscaler.json");
    unique.removeDuplicates();

    for (const QString &rel : unique) {
        if (rel.isEmpty())
            continue;
        const QString src = exeDir + QStringLiteral("/") + rel;
        const bool existed = QFileInfo::exists(src);
        QJsonObject item{{QStringLiteral("path"), rel}, {QStringLiteral("existed"), existed}};
        if (existed && QFileInfo(src).isFile()) {
            const QString dst = dir + QStringLiteral("/files/") + rel;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            if (!QFile::copy(src, dst))
                return {};
        }
        items.append(item);
    }

    QSaveFile manifest(dir + QStringLiteral("/snapshot.json"));
    if (!manifest.open(QIODevice::WriteOnly))
        return {};
    QJsonObject rootObj{{QStringLiteral("exeDir"), exeDir}, {QStringLiteral("items"), items}};
    manifest.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
    return manifest.commit() ? dir : QString();
}

bool OptiScalerIntegration::restoreTransactionSnapshot(int row, const QString &snapshotDir) const {
    const QString exeDir = exeDirFor(row);
    if (exeDir.isEmpty())
        return false;
    QFile manifest(snapshotDir + QStringLiteral("/snapshot.json"));
    if (!manifest.open(QIODevice::ReadOnly))
        return false;
    const QJsonArray items = QJsonDocument::fromJson(manifest.readAll()).object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &v : items) {
        const QJsonObject item = v.toObject();
        const QString rel = item.value(QStringLiteral("path")).toString();
        const bool existed = item.value(QStringLiteral("existed")).toBool();
        const QString dst = exeDir + QStringLiteral("/") + rel;
        if (existed) {
            const QString src = snapshotDir + QStringLiteral("/files/") + rel;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            QFile::remove(dst);
            if (!QFile::copy(src, dst))
                return false;
        } else {
            QFile::remove(dst);
        }
    }
    return true;
}

bool OptiScalerIntegration::saveWorkingConfiguration(int row) {
    beginStatusForRow(row);
    const QVariantMap analysis = analysisMap(row);
    if (!analysis.value(QStringLiteral("detected")).toBool()) {
        setStatus(QStringLiteral("OptiScaler is not detected for this game."));
        return false;
    }

    QStringList paths;
    const QString optiProxy = analysis.value(QStringLiteral("optiProxy")).toString();
    const QString reshadePath = analysis.value(QStringLiteral("reshadePath")).toString();
    const QString iniPath = analysis.value(QStringLiteral("iniPath")).toString();
    if (!optiProxy.isEmpty()) paths << optiProxy;
    if (!reshadePath.isEmpty()) paths << reshadePath;
    if (!iniPath.isEmpty()) paths << QFileInfo(iniPath).fileName();
    paths << QStringLiteral(".reno119-state.json");
    paths.removeDuplicates();

    const QString snapshot = createTransactionSnapshot(row, paths);
    if (snapshot.isEmpty()) {
        setStatus(QStringLiteral("Could not snapshot the current working integration."));
        return false;
    }

    QVariantMap state = readIntegrationState(row);
    QVariantMap working;
    working["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    working["optiProxy"] = optiProxy;
    working["reshadePath"] = reshadePath;
    working["loadReshade"] = analysis.value(QStringLiteral("loadReshade"));
    working["method"] = state.value(QStringLiteral("method"));
    working["snapshotDir"] = snapshot;
    state["workingConfig"] = working;
    appendHistory(state, QStringLiteral("Saved current ReShade/OptiScaler integration as working"));
    if (!writeIntegrationState(row, state)) {
        setStatus(QStringLiteral("Could not save the working integration configuration."));
        return false;
    }
    bumpRevision();
    setStatus(QStringLiteral("Saved the current ReShade/OptiScaler integration as a working configuration."));
    return true;
}

bool OptiScalerIntegration::restoreWorkingConfiguration(int row) {
    beginStatusForRow(row);
    const QVariantMap before = readIntegrationState(row);
    const QVariantMap working = before.value(QStringLiteral("workingConfig")).toMap();
    const QString snapshot = working.value(QStringLiteral("snapshotDir")).toString();
    if (snapshot.isEmpty()) {
        setStatus(QStringLiteral("No saved working OptiScaler/ReShade configuration is available."));
        return false;
    }

    const QString currentOpti = before.value(QStringLiteral("optiProxy")).toString();
    const QString currentReShade = before.value(QStringLiteral("reshadePath")).toString();
    const QString workingOpti = working.value(QStringLiteral("optiProxy")).toString();
    const QString workingReShade = working.value(QStringLiteral("reshadePath")).toString();
    const QString exeDir = exeDirFor(row);

    if (!restoreTransactionSnapshot(row, snapshot)) {
        setStatus(QStringLiteral("Could not restore the saved working integration configuration."));
        return false;
    }

    // Remove paths created by later Reno119 integration changes that were not
    // part of the saved working layout. Only paths recorded as Reno119-managed are
    // touched here; unrelated OptiScaler/game files are never swept broadly.
    if (!currentOpti.isEmpty() && currentOpti != workingOpti)
        QFile::remove(exeDir + QStringLiteral("/") + currentOpti);
    if (!currentReShade.isEmpty() && currentReShade != workingReShade)
        QFile::remove(exeDir + QStringLiteral("/") + currentReShade);

    QVariantMap restored = readIntegrationState(row);
    restored["workingConfig"] = working;
    appendHistory(restored, QStringLiteral("Restored saved working ReShade/OptiScaler integration"));
    writeIntegrationState(row, restored);
    if (m_games) {
        m_games->setOptiProxyChoiceIndex(row, proxyChoiceIndex(restored.value(QStringLiteral("proxyChoice"), QStringLiteral("recommended")).toString()));
        m_games->setOptiMethodChoiceIndex(row, methodChoiceIndex(restored.value(QStringLiteral("methodChoice"), QStringLiteral("recommended")).toString()));
        m_games->refreshInstallState(row);
    }
    bumpRevision();
    setStatus(QStringLiteral("Restored the saved working ReShade/OptiScaler integration."));
    return true;
}

QVariantList OptiScalerIntegration::restorePoints(int row) const {
    const QVariantList stored = readIntegrationState(row).value(QStringLiteral("restorePoints")).toList();
    QVariantList out;
    for (const QVariant &value : stored) {
        QVariantMap item = value.toMap();
        const QString createdAt = item.value(QStringLiteral("createdAt")).toString();
        QDateTime created = QDateTime::fromString(createdAt, Qt::ISODate);
        if (created.isValid())
            created = created.toLocalTime();
        item[QStringLiteral("createdDisplay")] = created.isValid()
            ? created.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
            : createdAt;
        out << item;
    }
    return out;
}

bool OptiScalerIntegration::restorePoint(int row, const QString &snapshotDir) {
    beginStatusForRow(row);
    const auto *g = m_games ? m_games->game(row) : nullptr;
    if (!g) {
        setStatus(QStringLiteral("No game is selected."));
        return false;
    }

    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cacheRoot.isEmpty())
        cacheRoot = QDir::homePath() + QStringLiteral("/.cache");
    const QString safeId = QString(g->appId).replace(':', '_').replace('/', '_');
    const QString allowedRoot = QDir(cacheRoot + QStringLiteral("/reno119/integration-backups/") + safeId).canonicalPath();
    const QString candidate = QDir(snapshotDir).canonicalPath();
    if (allowedRoot.isEmpty() || candidate.isEmpty() ||
        (candidate != allowedRoot && !candidate.startsWith(allowedRoot + QLatin1Char('/')))) {
        setStatus(QStringLiteral("Refusing to restore an integration snapshot outside this game's Reno119 history."));
        return false;
    }

    const QVariantMap before = readIntegrationState(row);
    const QVariantList preservedRestorePoints = before.value(QStringLiteral("restorePoints")).toList();
    const QVariantList preservedHistory = before.value(QStringLiteral("history")).toList();
    const QVariant preservedWorking = before.value(QStringLiteral("workingConfig"));
    const QString currentOpti = before.value(QStringLiteral("optiProxy")).toString();
    const QString currentReShade = before.value(QStringLiteral("reshadePath")).toString();

    if (!restoreTransactionSnapshot(row, candidate)) {
        setStatus(QStringLiteral("Could not restore the selected OptiScaler integration history point."));
        return false;
    }

    QVariantMap restored = readIntegrationState(row);
    const QString restoredOpti = restored.value(QStringLiteral("optiProxy")).toString();
    const QString restoredReShade = restored.value(QStringLiteral("reshadePath")).toString();
    const QString exeDir = exeDirFor(row);

    // Remove a later Reno119-selected OptiScaler proxy that is not part of the
    // restored layout. ReShade64 is an independent explicit install, so it is
    // deliberately not deleted here. Only integration-moved plugin-folder
    // ReShade files are eligible for this cleanup.
    if (!currentOpti.isEmpty() && currentOpti != restoredOpti && supportedProxies().contains(currentOpti, Qt::CaseInsensitive))
        QFile::remove(exeDir + QStringLiteral("/") + currentOpti);
    if (!currentReShade.isEmpty() && currentReShade != restoredReShade &&
        currentReShade.startsWith(QStringLiteral("OptiScaler/plugins/"), Qt::CaseInsensitive))
        QFile::remove(exeDir + QStringLiteral("/") + currentReShade);

    restored[QStringLiteral("restorePoints")] = preservedRestorePoints;
    restored[QStringLiteral("history")] = preservedHistory;
    if (preservedWorking.isValid())
        restored[QStringLiteral("workingConfig")] = preservedWorking;
    appendHistory(restored, QStringLiteral("Restored a selected OptiScaler/ReShade integration history point"));
    if (!writeIntegrationState(row, restored)) {
        setStatus(QStringLiteral("Integration files were restored, but Reno119 could not preserve the history metadata."));
        return false;
    }

    if (m_games) {
        m_games->setOptiProxyChoiceIndex(row, proxyChoiceIndex(restored.value(QStringLiteral("proxyChoice"), QStringLiteral("recommended")).toString()));
        m_games->setOptiMethodChoiceIndex(row, methodChoiceIndex(restored.value(QStringLiteral("methodChoice"), QStringLiteral("recommended")).toString()));
        m_games->refreshInstallState(row);
    }
    bumpRevision();
    setStatus(QStringLiteral("Restored the selected OptiScaler/ReShade integration history point."));
    return true;
}

QStringList OptiScalerIntegration::history(int row) const {
    const QVariantList values = readIntegrationState(row).value(QStringLiteral("history")).toList();
    QStringList out;
    for (const QVariant &v : values)
        out << v.toString();
    return out;
}

