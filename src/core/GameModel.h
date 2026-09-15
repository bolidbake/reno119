#pragma once

#include "GameInfo.h"
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QTimer>
#include <QStringList>
#include <QVector>

class GameModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool cachedSnapshot READ cachedSnapshot NOTIFY cachedSnapshotChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(bool favoritesFirst READ favoritesFirst WRITE setFavoritesFirst NOTIFY librarySettingsChanged)
    Q_PROPERTY(bool scanSteamEnabled READ scanSteamEnabled WRITE setScanSteamEnabled NOTIFY librarySettingsChanged)
    Q_PROPERTY(bool scanHeroicEnabled READ scanHeroicEnabled WRITE setScanHeroicEnabled NOTIFY librarySettingsChanged)
    Q_PROPERTY(bool scanLutrisEnabled READ scanLutrisEnabled WRITE setScanLutrisEnabled NOTIFY librarySettingsChanged)
    Q_PROPERTY(QString heroicRootOverride READ heroicRootOverride WRITE setHeroicRootOverride NOTIFY librarySettingsChanged)
    Q_PROPERTY(QString lutrisDataRootOverride READ lutrisDataRootOverride WRITE setLutrisDataRootOverride NOTIFY librarySettingsChanged)
    Q_PROPERTY(QString lutrisConfigRootOverride READ lutrisConfigRootOverride WRITE setLutrisConfigRootOverride NOTIFY librarySettingsChanged)
    Q_PROPERTY(int bulkSelectionCount READ bulkSelectionCount NOTIFY bulkSelectionChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        OriginalNameRole,
        NicknameRole,
        AppIdRole,
        SourceRole,
        CustomProgramRole,
        FavoriteRole,
        DuplicateCandidateCountRole,
        LinkedAlternateCountRole,
        InstallPathRole,
        SteamLibraryRole,
        ProtonPrefixRole,
        DetectedProtonPrefixRole,
        PrefixOverriddenRole,
        ExePathRole,
        GraphicsApiRole,
        DetectedGraphicsApiRole,
        GraphicsApiOverriddenRole,
        ArchitectureRole,
        EngineRole,
        CoverArtSourceRole,
        BannerArtSourceRole,
        CoverArtPathRole,
        BannerArtPathRole,
        ArtworkOverriddenRole,
        HiddenRole,
        ExeOverriddenRole,
        DetectedExePathRole,
        ReShadeInstalledRole,
        ReShadeManagedRole,
        ReShadeExternalRole,
        ReShadeVersionRole,
        ReShadeChannelRole,
        ReShadeProxyRole,
        ReShade64InstalledRole,
        ReShade64ManagedRole,
        ReShade64ExternalRole,
        ReShade64VersionRole,
        ReShade64ChannelRole,
        RenoDxInstalledRole,
        RenoDxManagedRole,
        RenoDxExternalRole,
        RenoDxFileRole,
        UpdateCheckedRole,
        ReShadeUpdateAvailableRole,
        RenoDxUpdateAvailableRole,
        ReFrameworkUpdateAvailableRole,
        ReFrameworkSupportedRole,
        ReFrameworkInstalledRole,
        ReFrameworkManagedRole,
        ReFrameworkExternalRole,
        ReFrameworkVersionRole,
        OptiScalerInstalledRole,
        IntegrityWarningRole,
        IntegritySummaryRole,
        DiagnosticLevelRole,
        DiagnosticSummaryRole,
        HealthLevelRole,
        HealthSummaryRole,
        HealthTargetRole,
        OverrideCountRole,
        DetectionChangeCountRole,
        DetectionHistoryRole,
        BulkSelectedRole,
        LaunchOptionsRole,
        WineOverridesRole
    };

    explicit GameModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool scanning() const { return m_scanning; }
    bool cachedSnapshot() const { return m_cachedSnapshot; }
    int count() const { return m_games.size(); }
    int totalCount() const { return m_allGames.size(); }
    int revision() const { return m_revision; }
    QString searchText() const { return m_searchText; }
    QString sortMode() const { return m_sortMode; }
    QString filterMode() const { return m_filterMode; }
    QString sourceFilter() const { return m_sourceFilter; }
    bool favoritesFirst() const { return m_favoritesFirst; }
    bool scanSteamEnabled() const { return m_scanSteamEnabled; }
    bool scanHeroicEnabled() const { return m_scanHeroicEnabled; }
    bool scanLutrisEnabled() const { return m_scanLutrisEnabled; }
    QString heroicRootOverride() const { return m_heroicRootOverride; }
    QString lutrisDataRootOverride() const { return m_lutrisDataRootOverride; }
    QString lutrisConfigRootOverride() const { return m_lutrisConfigRootOverride; }
    int bulkSelectionCount() const { return m_bulkSelection.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap gameAt(int row) const;
    Q_INVOKABLE QStringList steamAppIds() const;
    QVector<GameInfo> allGamesSnapshot() const { return m_allGames; }
#ifdef RENO119_TESTING
    void setGamesForTesting(const QVector<GameInfo> &games);
#endif
    Q_INVOKABLE QString executableOverride(int row) const;
    Q_INVOKABLE bool setExecutableOverride(int row, const QString &path);
    Q_INVOKABLE void clearExecutableOverride(int row);
    Q_INVOKABLE QString prefixOverride(int row) const;
    Q_INVOKABLE bool setPrefixOverride(int row, const QString &path);
    Q_INVOKABLE void clearPrefixOverride(int row);
    Q_INVOKABLE QString graphicsApiOverride(int row) const;
    Q_INVOKABLE bool setGraphicsApiOverride(int row, const QString &api);
    Q_INVOKABLE void clearGraphicsApiOverride(int row);
    Q_INVOKABLE bool rescanGame(int row);
    Q_INVOKABLE void reloadConfiguration();
    void setReFrameworkSupportedTitles(const QStringList &titles);

    Q_INVOKABLE int reShadeChoiceIndex(int row) const;
    void recordReShadeInstalledChannel(int row, const QString &channel);
    Q_INVOKABLE int optiProxyChoiceIndex(int row) const;
    Q_INVOKABLE void setOptiProxyChoiceIndex(int row, int index);
    Q_INVOKABLE int optiMethodChoiceIndex(int row) const;
    Q_INVOKABLE void setOptiMethodChoiceIndex(int row, int index);

    Q_INVOKABLE QString addCustomProgram(const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath = QString());
    Q_INVOKABLE bool updateCustomProgram(int row, const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath = QString());
    Q_INVOKABLE bool setGameNickname(int row, const QString &nickname);
    Q_INVOKABLE bool clearGameNickname(int row);
    Q_INVOKABLE bool setArtworkOverride(int row, const QString &path);
    Q_INVOKABLE bool clearArtworkOverride(int row);
    Q_INVOKABLE bool setCustomArtwork(int row, const QString &path);
    Q_INVOKABLE bool clearCustomArtwork(int row);
    Q_INVOKABLE bool setGameFavorite(int row, bool favorite);
    Q_INVOKABLE bool bulkSelected(int row) const;
    Q_INVOKABLE void toggleBulkSelected(int row);
    Q_INVOKABLE void clearBulkSelection();
    Q_INVOKABLE QStringList bulkSelectedAppIds() const;
    Q_INVOKABLE QStringList bulkSelectedSteamAppIds() const;
    Q_INVOKABLE bool bulkSetFavorite(bool favorite);
    Q_INVOKABLE bool bulkSetHidden(bool hidden);
    Q_INVOKABLE bool bulkClearDetectionOverrides();
    Q_INVOKABLE int bulkRescanSelected();
    Q_INVOKABLE QVariantMap duplicateInfo(int row) const;
    Q_INVOKABLE QVariantMap diagnosticInfo(int row) const;
    Q_INVOKABLE bool clearDetectionHistory(int row);
    Q_INVOKABLE bool linkAlternateInstall(int row, const QString &otherAppId);
    Q_INVOKABLE bool unlinkAlternateInstall(int row);
    Q_INVOKABLE bool setGameHidden(int row, bool hidden);
    Q_INVOKABLE bool removeCustomProgram(int row);

    const GameInfo *game(int row) const;
    void refreshInstallState(int row);
    void setUpdateStatus(const QString &appId, bool checked, bool reshadeUpdateAvailable, bool renodxUpdateAvailable, bool reframeworkUpdateAvailable);

public slots:
    void setSearchText(const QString &text);
    void setSortMode(const QString &mode);
    void setFilterMode(const QString &mode);
    void setSourceFilter(const QString &source);
    void setFavoritesFirst(bool enabled);
    void setScanSteamEnabled(bool enabled);
    void setScanHeroicEnabled(bool enabled);
    void setScanLutrisEnabled(bool enabled);
    void setHeroicRootOverride(const QString &path);
    void setLutrisDataRootOverride(const QString &path);
    void setLutrisConfigRootOverride(const QString &path);

signals:
    void scanningChanged();
    void cachedSnapshotChanged();
    void countChanged();
    void totalCountChanged();
    void revisionChanged();
    void searchTextChanged();
    void sortModeChanged();
    void filterModeChanged();
    void sourceFilterChanged();
    void librarySettingsChanged();
    void bulkSelectionChanged();

private:
    static QString wineOverridesFor(const GameInfo &g);
    static QString launchOptionsFor(const GameInfo &g);
    bool reFrameworkSupportedFor(const GameInfo &g) const;
    void applyFilter();
    void sortGames(QVector<GameInfo> &games) const;
    void applyExecutableOverrides();
    void syncGameBackToAllGames(const GameInfo &game);
    void refreshInstallState(GameInfo &g);
    QString overrideKey(const QString &appId) const;
    QString prefixOverrideKey(const QString &appId) const;
    QString graphicsApiOverrideKey(const QString &appId) const;
    QString hiddenKey(const QString &appId) const;
    QString favoriteKey(const QString &appId) const;
    QString nicknameKey(const QString &appId) const;
    QString artworkOverrideKey(const QString &appId) const;
    QString alternateGroupKey(const QString &appId) const;
    QString uiChoiceKey(const QString &appId, const QString &name) const;
    int uiChoiceIndex(int row, const QString &name, int defaultValue, int maxValue) const;
    void setUiChoiceIndex(int row, const QString &name, int value, int maxValue);
    void requestRefresh();
    bool loadLibraryCache();
    void saveLibraryCache() const;
    QString libraryCachePath() const;
    void scheduleLibraryCacheWrite();
    void rebuildDuplicateMetadata();
    static QString duplicateMatchKey(const GameInfo &game);
    QVariantMap diagnosticInfoFor(const GameInfo &game) const;

    QVector<GameInfo> loadCustomPrograms();
    void saveCustomProgram(const GameInfo &game);
    void deleteCustomProgram(const QString &appId);
    static GameInfo makeCustomProgram(const QString &id, const QString &name, const QString &exePath, const QString &prefixPath, const QString &artworkPath = QString());

    QVector<GameInfo> m_allGames;
    QVector<GameInfo> m_games;
    QString m_searchText;
    QString m_sortMode = QStringLiteral("name");
    QString m_filterMode = QStringLiteral("all");
    QString m_sourceFilter = QStringLiteral("all");
    bool m_favoritesFirst = true;
    bool m_scanSteamEnabled = true;
    bool m_scanHeroicEnabled = true;
    bool m_scanLutrisEnabled = true;
    QString m_heroicRootOverride;
    QString m_lutrisDataRootOverride;
    QString m_lutrisConfigRootOverride;
    bool m_scanning = false;
    bool m_cachedSnapshot = false;
    bool m_refreshPending = false;
    int m_revision = 0;
    QFutureWatcher<QVector<GameInfo>> m_watcher;
    QTimer m_cacheWriteTimer;
    QSettings m_settings;
    QStringList m_reFrameworkSupportedTitles;
    QSet<QString> m_bulkSelection;
};
