#include "GameModel.h"
#include "../graphics/PeParser.h"
#include "../steam/SteamScanner.h"

#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QUuid>

namespace {
QString displayNameFor(const GameInfo &game) {
    const QString nickname = game.nickname.trimmed();
    return nickname.isEmpty() ? game.name : nickname;
}

QString normalizedGameKey(const QString &value) {
    const QString normalized = value.normalized(QString::NormalizationForm_KD).toLower();
    QString out;
    out.reserve(normalized.size());
    for (const QChar ch : normalized) {
        if (ch.isLetterOrNumber())
            out.append(ch);
    }
    return out;
}
}

GameInfo GameModel::makeCustomProgram(const QString &id, const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath) {
    GameInfo g;
    g.name = name.trimmed().isEmpty() ? QFileInfo(exePath).completeBaseName() : name.trimmed();
    g.appId = id.startsWith(QStringLiteral("custom:")) ? id : QStringLiteral("custom:") + id;
    g.source = QStringLiteral("Custom");
    g.customProgram = true;
    g.exePath = QFileInfo(exePath).absoluteFilePath();
    g.detectedExePath = g.exePath;
    g.installPath = QFileInfo(g.exePath).absolutePath();
    g.protonPrefix = prefixPath.trimmed();
    g.detectedProtonPrefix = g.protonPrefix;

    // A Custom entry may intentionally point at Ubisoft Connect because the
    // actual games live inside that launcher's Wine prefix. When the configured
    // executable is clearly a Ubisoft/Uplay launcher, use the custom entry's
    // display name to resolve the real game executable under the prefix instead
    // of treating the launcher itself as the game.
    if (!g.protonPrefix.isEmpty() && SteamScanner::isLikelyLauncherExecutable(g.exePath)) {
        QString ubisoftInstallPath;
        QString ubisoftApi;
        QString ubisoftArch;
        QStringList ubisoftCandidates;
        const QString ubisoftExe = SteamScanner::findBestUbisoftExecutable(g.protonPrefix, g.name,
                                                                           ubisoftApi, ubisoftArch,
                                                                           ubisoftInstallPath, &ubisoftCandidates);
        if (!ubisoftExe.isEmpty()) {
            g.sourceMetadata.insert(QStringLiteral("configuredLauncherExecutable"), g.exePath);
            g.sourceMetadata.insert(QStringLiteral("ubisoftExecutableCandidates"), ubisoftCandidates);
            g.exePath = ubisoftExe;
            g.detectedExePath = ubisoftExe;
            g.installPath = ubisoftInstallPath;
            g.graphicsApi = ubisoftApi;
            g.architecture = ubisoftArch;
            g.importDiagnostics << QStringLiteral("Resolved the Custom entry from Ubisoft Connect to: %1").arg(ubisoftExe);
        }
    }
    const QString art = artworkPath.trimmed();
    if (!art.isEmpty()) {
        const QFileInfo artInfo(art);
        if (artInfo.isFile()) {
            g.coverArtPath = artInfo.absoluteFilePath();
            g.bannerArtPath = g.coverArtPath;
        }
    }

    const auto pe = PeParser::inspect(g.exePath);
    if (pe.valid) {
        g.graphicsApi = pe.graphicsApi;
        if (g.graphicsApi == QStringLiteral("Unknown"))
            g.graphicsApi = PeParser::detectGraphicsApiFallback(g.exePath);
        g.architecture = pe.architecture;
    } else {
        g.graphicsApi = QStringLiteral("Unknown");
        g.architecture = QStringLiteral("Unknown");
    }
    g.detectedGraphicsApi = g.graphicsApi.isEmpty() ? QStringLiteral("Unknown") : g.graphicsApi;
    g.engine = SteamScanner::detectEngine(g.installPath, g.exePath);
    g.reframeworkSupported = SteamScanner::supportsReFramework(g.name, g.engine);
    g.sourceMetadata.insert(QStringLiteral("configuredExecutable"), exePath);
    g.sourceMetadata.insert(QStringLiteral("configuredPrefix"), prefixPath);
    g.sourceMetadata.insert(QStringLiteral("configuredArtwork"), artworkPath);
    return g;
}

QVector<GameInfo> GameModel::loadCustomPrograms() {
    QVector<GameInfo> result;
    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    settings.beginGroup(QStringLiteral("customPrograms"));
    const auto ids = settings.childGroups();
    for (const QString &rawId : ids) {
        settings.beginGroup(rawId);
        const QString name = settings.value(QStringLiteral("name")).toString();
        const QString exe = settings.value(QStringLiteral("exePath")).toString();
        const QString prefix = settings.value(QStringLiteral("prefixPath")).toString();
        const QString artwork = settings.value(QStringLiteral("artworkPath")).toString();
        settings.endGroup();
        if (exe.isEmpty() || !QFileInfo::exists(exe))
            continue;
        GameInfo g = makeCustomProgram(QStringLiteral("custom:") + rawId, name, exe, prefix, artwork);
        refreshInstallState(g);
        result.push_back(g);
    }
    settings.endGroup();
    return result;
}

void GameModel::saveCustomProgram(const GameInfo &game) {
    if (!game.customProgram)
        return;
    QString id = game.appId;
    if (id.startsWith(QStringLiteral("custom:")))
        id.remove(0, 7);
    m_settings.beginGroup(QStringLiteral("customPrograms/%1").arg(id));
    m_settings.setValue(QStringLiteral("name"), game.name);
    m_settings.setValue(QStringLiteral("exePath"), game.detectedExePath.isEmpty() ? game.exePath : game.detectedExePath);
    m_settings.setValue(QStringLiteral("prefixPath"), game.detectedProtonPrefix.isEmpty() ? game.protonPrefix : game.detectedProtonPrefix);
    m_settings.setValue(QStringLiteral("artworkPath"), game.coverArtPath);
    m_settings.endGroup();
}

void GameModel::deleteCustomProgram(const QString &appId) {
    QString id = appId;
    if (id.startsWith(QStringLiteral("custom:")))
        id.remove(0, 7);
    m_settings.remove(QStringLiteral("customPrograms/%1").arg(id));
    m_settings.remove(overrideKey(appId));
    m_settings.remove(prefixOverrideKey(appId));
    m_settings.remove(graphicsApiOverrideKey(appId));
    m_settings.remove(hiddenKey(appId));
    m_settings.remove(favoriteKey(appId));
    m_settings.remove(nicknameKey(appId));
    m_settings.remove(artworkOverrideKey(appId));
    m_settings.remove(alternateGroupKey(appId));
}

QString GameModel::addCustomProgram(const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath) {
    const QString normalizedExe = QFileInfo(exePath.trimmed()).absoluteFilePath();
    if (!QFileInfo::exists(normalizedExe))
        return {};
    const auto pe = PeParser::inspect(normalizedExe);
    if (!pe.valid)
        return {};

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    GameInfo g = makeCustomProgram(QStringLiteral("custom:") + id, name, normalizedExe, prefixPath, artworkPath);
    g.originalCoverArtPath = g.coverArtPath;
    g.originalBannerArtPath = g.bannerArtPath;
    refreshInstallState(g);
    saveCustomProgram(g);
    m_allGames.push_back(g);
    rebuildDuplicateMetadata();
    sortGames(m_allGames);
    emit totalCountChanged();
    applyFilter();
    return g.appId;
}

bool GameModel::updateCustomProgram(int row, const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath) {
    if (row < 0 || row >= m_games.size() || !m_games[row].customProgram)
        return false;
    const QString normalizedExe = QFileInfo(exePath.trimmed()).absoluteFilePath();
    if (!QFileInfo::exists(normalizedExe))
        return false;
    const auto pe = PeParser::inspect(normalizedExe);
    if (!pe.valid)
        return false;

    const QString id = m_games[row].appId;
    GameInfo updated = makeCustomProgram(id, name, normalizedExe, prefixPath, artworkPath);
    updated.hidden = m_games[row].hidden;
    updated.favorite = m_games[row].favorite;
    updated.nickname = m_games[row].nickname;
    updated.alternateGroupId = m_games[row].alternateGroupId;
    updated.originalCoverArtPath = updated.coverArtPath;
    updated.originalBannerArtPath = updated.bannerArtPath;
    refreshInstallState(updated);
    saveCustomProgram(updated);
    m_games[row] = updated;
    syncGameBackToAllGames(updated);
    // Reapply any Reno119-only EXE/prefix/API overrides after editing the
    // underlying Custom entry so the base configuration and overrides remain separate.
    rescanGame(row);
    rebuildDuplicateMetadata();
    applyFilter();
    return true;
}

bool GameModel::setGameNickname(int row, const QString &nickname) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty())
        return clearGameNickname(row);

    const QString appId = m_games[row].appId;
    m_settings.setValue(nicknameKey(appId), trimmed);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    m_games[row].nickname = trimmed;
    for (auto &game : m_allGames) {
        if (game.appId == appId) {
            game.nickname = trimmed;
            break;
        }
    }

    rebuildDuplicateMetadata();
    applyFilter();
    return true;
}

bool GameModel::clearGameNickname(int row) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString appId = m_games[row].appId;
    m_settings.remove(nicknameKey(appId));
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    m_games[row].nickname.clear();
    for (auto &game : m_allGames) {
        if (game.appId == appId) {
            game.nickname.clear();
            break;
        }
    }

    rebuildDuplicateMetadata();
    applyFilter();
    return true;
}

bool GameModel::setArtworkOverride(int row, const QString &path) {
    if (row < 0 || row >= m_games.size())
        return false;
    const QFileInfo info(path.trimmed());
    if (!info.isFile())
        return false;

    const QString absolutePath = info.absoluteFilePath();
    auto &g = m_games[row];
    m_settings.setValue(artworkOverrideKey(g.appId), absolutePath);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    g.coverArtPath = absolutePath;
    g.bannerArtPath = absolutePath;
    g.artworkOverridden = true;
    syncGameBackToAllGames(g);
    const QModelIndex idx = index(row);
    ++m_revision;
    emit dataChanged(idx, idx, {CoverArtSourceRole, BannerArtSourceRole, CoverArtPathRole, BannerArtPathRole, ArtworkOverriddenRole});
    emit revisionChanged();
    return true;
}

bool GameModel::clearArtworkOverride(int row) {
    if (row < 0 || row >= m_games.size())
        return false;

    auto &g = m_games[row];
    m_settings.remove(artworkOverrideKey(g.appId));
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    g.coverArtPath = g.originalCoverArtPath;
    g.bannerArtPath = g.originalBannerArtPath.isEmpty() ? g.originalCoverArtPath : g.originalBannerArtPath;
    g.artworkOverridden = false;
    syncGameBackToAllGames(g);
    const QModelIndex idx = index(row);
    ++m_revision;
    emit dataChanged(idx, idx, {CoverArtSourceRole, BannerArtSourceRole, CoverArtPathRole, BannerArtPathRole, ArtworkOverriddenRole});
    emit revisionChanged();
    return true;
}

bool GameModel::setCustomArtwork(int row, const QString &path) {
    if (row < 0 || row >= m_games.size() || !m_games[row].customProgram)
        return false;
    const QFileInfo info(path.trimmed());
    if (!info.isFile())
        return false;

    m_games[row].coverArtPath = info.absoluteFilePath();
    m_games[row].bannerArtPath = m_games[row].coverArtPath;
    m_games[row].originalCoverArtPath = m_games[row].coverArtPath;
    m_games[row].originalBannerArtPath = m_games[row].bannerArtPath;
    saveCustomProgram(m_games[row]);
    return setArtworkOverride(row, path);
}

bool GameModel::clearCustomArtwork(int row) {
    if (row < 0 || row >= m_games.size() || !m_games[row].customProgram)
        return false;

    m_games[row].coverArtPath.clear();
    m_games[row].bannerArtPath.clear();
    m_games[row].originalCoverArtPath.clear();
    m_games[row].originalBannerArtPath.clear();
    saveCustomProgram(m_games[row]);
    return clearArtworkOverride(row);
}

QString GameModel::duplicateMatchKey(const GameInfo &game) {
    const QString key = normalizedGameKey(displayNameFor(game));
    // Very short launcher labels/initialisms are too ambiguous to auto-suggest.
    return key.size() >= 5 ? key : QString();
}

void GameModel::rebuildDuplicateMetadata() {
    for (auto &game : m_allGames) {
        game.duplicateCandidateCount = 0;
        game.linkedAlternateCount = 0;
    }

    QHash<QString, QVector<int>> linkedGroups;
    for (int i = 0; i < m_allGames.size(); ++i) {
        const QString group = m_allGames[i].alternateGroupId.trimmed();
        if (!group.isEmpty())
            linkedGroups[group].push_back(i);
    }

    for (auto it = linkedGroups.constBegin(); it != linkedGroups.constEnd(); ++it) {
        for (const int index : it.value())
            m_allGames[index].linkedAlternateCount = qMax(0, it.value().size() - 1);
    }

    QHash<QString, QVector<int>> probableGroups;
    for (int i = 0; i < m_allGames.size(); ++i) {
        const QString key = duplicateMatchKey(m_allGames[i]);
        if (!key.isEmpty())
            probableGroups[key].push_back(i);
    }

    for (auto it = probableGroups.constBegin(); it != probableGroups.constEnd(); ++it) {
        const auto &indexes = it.value();
        for (int a = 0; a < indexes.size(); ++a) {
            for (int b = a + 1; b < indexes.size(); ++b) {
                auto &left = m_allGames[indexes[a]];
                auto &right = m_allGames[indexes[b]];
                if (left.appId == right.appId ||
                    left.source.compare(right.source, Qt::CaseInsensitive) == 0)
                    continue;
                if (!left.alternateGroupId.isEmpty() && left.alternateGroupId == right.alternateGroupId)
                    continue;
                ++left.duplicateCandidateCount;
                ++right.duplicateCandidateCount;
            }
        }
    }

}

QVariantMap GameModel::duplicateInfo(int row) const {
    QVariantMap result;
    const auto *selected = game(row);
    if (!selected)
        return result;

    QVariantList linked;
    QVariantList suggestions;
    QVariantList others;
    const QString selectedKey = duplicateMatchKey(*selected);

    auto makeEntry = [](const GameInfo &candidate) {
        return QVariantMap{
            {QStringLiteral("appId"), candidate.appId},
            {QStringLiteral("name"), displayNameFor(candidate)},
            {QStringLiteral("originalName"), candidate.name},
            {QStringLiteral("source"), candidate.source},
            {QStringLiteral("exePath"), candidate.exePath},
            {QStringLiteral("protonPrefix"), candidate.protonPrefix}
        };
    };

    for (const auto &candidate : m_allGames) {
        if (candidate.appId == selected->appId)
            continue;

        if (!selected->alternateGroupId.isEmpty() &&
            candidate.alternateGroupId == selected->alternateGroupId) {
            linked.push_back(makeEntry(candidate));
            continue;
        }

        QVariantMap entry = makeEntry(candidate);
        entry.insert(QStringLiteral("label"), displayNameFor(candidate) + QStringLiteral(" · ") + candidate.source);
        others.push_back(entry);

        if (!selectedKey.isEmpty() &&
            candidate.source.compare(selected->source, Qt::CaseInsensitive) != 0 &&
            duplicateMatchKey(candidate) == selectedKey) {
            suggestions.push_back(entry);
        }
    }

    result.insert(QStringLiteral("appId"), selected->appId);
    result.insert(QStringLiteral("name"), displayNameFor(*selected));
    result.insert(QStringLiteral("source"), selected->source);
    result.insert(QStringLiteral("groupId"), selected->alternateGroupId);
    result.insert(QStringLiteral("linked"), linked);
    result.insert(QStringLiteral("suggestions"), suggestions);
    result.insert(QStringLiteral("others"), others);
    result.insert(QStringLiteral("linkedCount"), linked.size());
    result.insert(QStringLiteral("suggestionCount"), suggestions.size());
    return result;
}

bool GameModel::linkAlternateInstall(int row, const QString &otherAppId) {
    if (row < 0 || row >= m_games.size() || otherAppId.trimmed().isEmpty())
        return false;

    const QString selectedId = m_games[row].appId;
    if (selectedId == otherAppId)
        return false;

    GameInfo *selected = nullptr;
    GameInfo *other = nullptr;
    for (auto &game : m_allGames) {
        if (game.appId == selectedId)
            selected = &game;
        if (game.appId == otherAppId)
            other = &game;
    }
    if (!selected || !other)
        return false;

    QString group = selected->alternateGroupId.trimmed();
    const QString otherGroup = other->alternateGroupId.trimmed();
    if (group.isEmpty())
        group = otherGroup;
    if (group.isEmpty())
        group = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Joining two existing alternate-install groups merges them into one. Keep
    // metadata for temporarily unavailable/disabled-source installs too.
    if (!otherGroup.isEmpty() && otherGroup != group) {
        const QStringList keys = m_settings.allKeys();
        for (const QString &key : keys) {
            if (key.startsWith(QStringLiteral("games/")) &&
                key.endsWith(QStringLiteral("/alternateGroup")) &&
                m_settings.value(key).toString() == otherGroup)
                m_settings.setValue(key, group);
        }
    }
    for (auto &game : m_allGames) {
        if (game.appId == selectedId || game.appId == otherAppId ||
            (!otherGroup.isEmpty() && game.alternateGroupId == otherGroup)) {
            game.alternateGroupId = group;
            m_settings.setValue(alternateGroupKey(game.appId), group);
        }
    }
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    rebuildDuplicateMetadata();
    applyFilter();
    return true;
}

bool GameModel::unlinkAlternateInstall(int row) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString selectedId = m_games[row].appId;
    QString group;
    for (const auto &game : m_allGames) {
        if (game.appId == selectedId) {
            group = game.alternateGroupId;
            break;
        }
    }
    if (group.isEmpty())
        return false;

    for (auto &game : m_allGames) {
        if (game.appId == selectedId) {
            game.alternateGroupId.clear();
            m_settings.remove(alternateGroupKey(game.appId));
            break;
        }
    }
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    // Keep the remaining group metadata intact; a temporarily unavailable linked
    // install may reappear after a source is re-enabled or a library is mounted.
    rebuildDuplicateMetadata();
    applyFilter();
    return true;
}

