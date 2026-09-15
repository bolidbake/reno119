#include "GameModel.h"

#include <utility>

bool GameModel::setGameFavorite(int row, bool favorite) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString appId = m_games[row].appId;
    m_settings.setValue(favoriteKey(appId), favorite);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    m_games[row].favorite = favorite;
    for (auto &game : m_allGames) {
        if (game.appId == appId) {
            game.favorite = favorite;
            break;
        }
    }
    applyFilter();
    return true;
}

bool GameModel::bulkSelected(int row) const {
    const auto *g = game(row);
    return g && m_bulkSelection.contains(g->appId);
}

void GameModel::toggleBulkSelected(int row) {
    const auto *g = game(row);
    if (!g)
        return;
    if (m_bulkSelection.contains(g->appId))
        m_bulkSelection.remove(g->appId);
    else
        m_bulkSelection.insert(g->appId);
    emit dataChanged(index(row), index(row), {BulkSelectedRole});
    emit bulkSelectionChanged();
}

void GameModel::clearBulkSelection() {
    if (m_bulkSelection.isEmpty())
        return;
    m_bulkSelection.clear();
    if (!m_games.isEmpty())
        emit dataChanged(index(0), index(m_games.size() - 1), {BulkSelectedRole});
    emit bulkSelectionChanged();
}

QStringList GameModel::bulkSelectedAppIds() const {
    QStringList ids;
    ids.reserve(m_bulkSelection.size());
    for (const QString &id : m_bulkSelection)
        ids << id;
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

QStringList GameModel::bulkSelectedSteamAppIds() const {
    QStringList ids;
    for (const auto &g : m_allGames) {
        if (m_bulkSelection.contains(g.appId) &&
            g.source.compare(QStringLiteral("Steam"), Qt::CaseInsensitive) == 0 && !g.appId.isEmpty())
            ids << g.appId;
    }
    ids.removeDuplicates();
    return ids;
}

bool GameModel::bulkSetFavorite(bool favorite) {
    if (m_bulkSelection.isEmpty())
        return false;
    bool changed = false;
    for (auto &g : m_allGames) {
        if (!m_bulkSelection.contains(g.appId))
            continue;
        g.favorite = favorite;
        m_settings.setValue(favoriteKey(g.appId), favorite);
        changed = true;
    }
    m_settings.sync();
    if (!changed || m_settings.status() != QSettings::NoError)
        return false;
    applyFilter();
    return true;
}

bool GameModel::bulkSetHidden(bool hidden) {
    if (m_bulkSelection.isEmpty())
        return false;
    bool changed = false;
    for (auto &g : m_allGames) {
        if (!m_bulkSelection.contains(g.appId))
            continue;
        g.hidden = hidden;
        m_settings.setValue(hiddenKey(g.appId), hidden);
        changed = true;
    }
    m_settings.sync();
    if (!changed || m_settings.status() != QSettings::NoError)
        return false;
    if (hidden)
        clearBulkSelection();
    applyFilter();
    return true;
}

bool GameModel::bulkClearDetectionOverrides() {
    if (m_bulkSelection.isEmpty())
        return false;
    bool changed = false;
    for (const auto &g : std::as_const(m_allGames)) {
        if (!m_bulkSelection.contains(g.appId))
            continue;
        m_settings.remove(overrideKey(g.appId));
        m_settings.remove(prefixOverrideKey(g.appId));
        m_settings.remove(graphicsApiOverrideKey(g.appId));
        changed = true;
    }
    m_settings.sync();
    if (!changed || m_settings.status() != QSettings::NoError)
        return false;
    applyExecutableOverrides();
    applyFilter();
    return true;
}

int GameModel::bulkRescanSelected() {
    if (m_bulkSelection.isEmpty())
        return 0;
    const QStringList ids = bulkSelectedAppIds();
    int rescanned = 0;
    for (const QString &appId : ids) {
        for (int row = 0; row < m_games.size(); ++row) {
            if (m_games[row].appId == appId) {
                if (rescanGame(row))
                    ++rescanned;
                break;
            }
        }
    }
    if (rescanned > 0)
        applyFilter();
    return rescanned;
}

bool GameModel::setGameHidden(int row, bool hidden) {
    if (row < 0 || row >= m_games.size())
        return false;

    const QString appId = m_games[row].appId;
    m_settings.setValue(hiddenKey(appId), hidden);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        return false;

    m_games[row].hidden = hidden;
    for (auto &game : m_allGames) {
        if (game.appId == appId) {
            game.hidden = hidden;
            break;
        }
    }

    applyFilter();
    return true;
}
