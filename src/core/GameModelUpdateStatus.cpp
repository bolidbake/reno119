#include "GameModel.h"

void GameModel::setUpdateStatus(const QString &appId, bool checked, bool reshadeUpdateAvailable, bool renodxUpdateAvailable, bool reframeworkUpdateAvailable) {
    for (auto &game : m_allGames) {
        if (game.appId == appId) {
            game.updateChecked = checked;
            game.reshadeUpdateAvailable = reshadeUpdateAvailable;
            game.renodxUpdateAvailable = renodxUpdateAvailable;
            game.reframeworkUpdateAvailable = reframeworkUpdateAvailable;
            break;
        }
    }

    // The Updates filter depends on these flags for membership, so it still
    // needs a filtered-model rebuild as results arrive. Other library views do
    // not: resetting the whole model here destroys every QML delegate and makes
    // unchanged cover art visibly reload during an Update Center check.
    if (m_filterMode == QStringLiteral("updates")) {
        applyFilter();
        return;
    }

    for (int row = 0; row < m_games.size(); ++row) {
        auto &game = m_games[row];
        if (game.appId != appId)
            continue;
        game.updateChecked = checked;
        game.reshadeUpdateAvailable = reshadeUpdateAvailable;
        game.renodxUpdateAvailable = renodxUpdateAvailable;
        game.reframeworkUpdateAvailable = reframeworkUpdateAvailable;
        const QModelIndex modelIndex = index(row, 0);
        emit dataChanged(modelIndex, modelIndex, {
            UpdateCheckedRole,
            ReShadeUpdateAvailableRole,
            RenoDxUpdateAvailableRole,
            ReFrameworkUpdateAvailableRole
        });
        ++m_revision;
        emit revisionChanged();
        break;
    }
}
