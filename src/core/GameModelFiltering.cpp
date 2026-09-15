#include "GameModel.h"

#include <algorithm>
#include <utility>

namespace {
QString displayNameFor(const GameInfo &game) {
    const QString nickname = game.nickname.trimmed();
    return nickname.isEmpty() ? game.name : nickname;
}
}

void GameModel::setSearchText(const QString &text) {
    if (m_searchText == text)
        return;
    m_searchText = text;
    emit searchTextChanged();
    applyFilter();
}

void GameModel::setSortMode(const QString &mode) {
    const QString normalized = mode.trimmed().toLower();
    if (m_sortMode == normalized)
        return;
    m_sortMode = normalized;
    emit sortModeChanged();
    applyFilter();
}

void GameModel::setFilterMode(const QString &mode) {
    const QString normalized = mode.trimmed().toLower();
    if (m_filterMode == normalized)
        return;
    m_filterMode = normalized;
    emit filterModeChanged();
    applyFilter();
}

void GameModel::setSourceFilter(const QString &source) {
    const QString normalized = source.trimmed().toLower();
    if (m_sourceFilter == normalized)
        return;
    m_sourceFilter = normalized;
    emit sourceFilterChanged();
    applyFilter();
}

void GameModel::sortGames(QVector<GameInfo> &games) const {
    const QString mode = m_sortMode;
    const bool favoritesFirst = m_favoritesFirst;
    std::sort(games.begin(), games.end(), [mode, favoritesFirst](const GameInfo &a, const GameInfo &b) {
        if (favoritesFirst && a.favorite != b.favorite)
            return a.favorite > b.favorite;
        auto cmpName = [&] { return QString::localeAwareCompare(displayNameFor(a), displayNameFor(b)) < 0; };
        if (mode == QStringLiteral("source")) {
            const int c = QString::localeAwareCompare(a.source, b.source);
            return c == 0 ? cmpName() : c < 0;
        }
        if (mode == QStringLiteral("api")) {
            const int c = QString::localeAwareCompare(a.graphicsApi, b.graphicsApi);
            return c == 0 ? cmpName() : c < 0;
        }
        if (mode == QStringLiteral("engine")) {
            const int c = QString::localeAwareCompare(a.engine, b.engine);
            return c == 0 ? cmpName() : c < 0;
        }
        if (mode == QStringLiteral("reshade")) {
            const bool aReShadeAny = a.reshadeInstalled || a.reshade64Installed;
            const bool bReShadeAny = b.reshadeInstalled || b.reshade64Installed;
            if (aReShadeAny != bReShadeAny)
                return aReShadeAny > bReShadeAny;
            const bool aExternalAny = a.reshadeExternal || a.reshade64External;
            const bool bExternalAny = b.reshadeExternal || b.reshade64External;
            if (aExternalAny != bExternalAny)
                return aExternalAny > bExternalAny;
            return cmpName();
        }
        if (mode == QStringLiteral("renodx")) {
            if (a.renodxInstalled != b.renodxInstalled)
                return a.renodxInstalled > b.renodxInstalled;
            if (a.renodxExternal != b.renodxExternal)
                return a.renodxExternal > b.renodxExternal;
            return cmpName();
        }
        return cmpName();
    });
}

void GameModel::applyFilter() {
    const QString needle = m_searchText.trimmed();
    QVector<GameInfo> filtered;
    filtered.reserve(m_allGames.size());

    for (const auto &game : m_allGames) {
        const bool searchMatch = needle.isEmpty() ||
            displayNameFor(game).contains(needle, Qt::CaseInsensitive) ||
            game.name.contains(needle, Qt::CaseInsensitive) ||
            game.appId.contains(needle, Qt::CaseInsensitive) ||
            game.source.contains(needle, Qt::CaseInsensitive);
        if (!searchMatch)
            continue;

        if (m_sourceFilter != QStringLiteral("all") &&
            game.source.compare(m_sourceFilter, Qt::CaseInsensitive) != 0)
            continue;

        // Hidden entries stay out of every normal view. The explicit Hidden
        // filter is the recovery path for reviewing and restoring them.
        if (m_filterMode == QStringLiteral("hidden")) {
            if (!game.hidden)
                continue;
        } else if (game.hidden) {
            continue;
        }

        bool filterMatch = true;
        if (m_filterMode == QStringLiteral("favorites"))
            filterMatch = game.favorite;
        else if (m_filterMode == QStringLiteral("reshade"))
            filterMatch = game.reshadeInstalled || game.reshade64Installed;
        else if (m_filterMode == QStringLiteral("renodx"))
            filterMatch = game.renodxInstalled;
        else if (m_filterMode == QStringLiteral("external"))
            filterMatch = game.reshadeExternal || game.reshade64External || game.renodxExternal || game.reframeworkExternal;
        else if (m_filterMode == QStringLiteral("updates"))
            filterMatch = game.updateChecked && (game.reshadeUpdateAvailable || game.renodxUpdateAvailable || game.reframeworkUpdateAvailable);
        else if (m_filterMode == QStringLiteral("none"))
            filterMatch = !game.reshadeInstalled && !game.reshade64Installed && !game.renodxInstalled;

        if (filterMatch)
            filtered.push_back(game);
    }

    sortGames(filtered);
    beginResetModel();
    m_games = std::move(filtered);
    endResetModel();
    ++m_revision;
    emit countChanged();
    emit revisionChanged();
}
