import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    width: 1280
    height: 840
    visible: true
    title: "Reno119 — ReShade / RenoDX Manager (v" + Qt.application.version + ")"
    color: windowBg

    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }

    property bool systemTheme: appSettings.themeMode === "system"
    property bool amoledTheme: appSettings.themeMode === "amoled"
    property bool materialTheme: appSettings.themeMode === "material"
    property bool noctaliaTheme: appSettings.themeMode === "noctalia"
    property bool roleTheme: materialTheme || noctaliaTheme
    property var activeRolePalette: noctaliaTheme ? themeManager.noctaliaPalette : themeManager.materialPalette
    property bool activeRoleDark: noctaliaTheme ? themeManager.noctaliaDark : themeManager.materialDark
    property bool activeRolePureBlack: roleTheme && (noctaliaTheme ? themeManager.noctaliaPureBlack : themeManager.materialPureBlack)
    property bool systemDark: (systemPalette.window.r * 0.299 + systemPalette.window.g * 0.587 + systemPalette.window.b * 0.114) < 0.5

    function themeRole(name, fallbackValue) {
        const value = root.activeRolePalette ? root.activeRolePalette[name] : ""
        return value !== undefined && value !== null && String(value).length > 0 ? value : fallbackValue
    }

    function themeIndex(mode) {
        if (mode === "dark") return 1
        if (mode === "amoled") return 2
        if (mode === "material") return 3
        if (mode === "noctalia") return 4
        return 0
    }

    function themeModeForIndex(index) {
        if (index === 1) return "dark"
        if (index === 2) return "amoled"
        if (index === 3) return "material"
        if (index === 4) return "noctalia"
        return "system"
    }

    // Material roles already encode elevation and contrast.  Keep large surfaces on
    // background/surface, and only use container roles for actual controls/elevation.
    // This is especially important for Noctalia's Pure Black palette: painting the
    // whole UI with tinted surface_container roles defeats the user's black surface.
    property color windowBg: roleTheme ? root.themeRole("background", root.themeRole("surface", "#1c1b1f"))
                                         : (systemTheme ? systemPalette.window : (amoledTheme ? "#000000" : "#17191d"))
    property color panelBg: roleTheme ? (root.activeRolePureBlack
                                            ? root.themeRole("surface", "#000000")
                                            : root.themeRole("surface_container_low", root.themeRole("surface", "#1c1b1f")))
                                        : (systemTheme ? systemPalette.window : (amoledTheme ? "#000000" : "#1d2025"))
    property color controlBg: roleTheme ? (root.activeRolePureBlack
                                              ? root.themeRole("surface_container_low", root.themeRole("surface", "#000000"))
                                              : root.themeRole("surface_container", root.themeRole("surface_variant", "#211f26")))
                                          : (systemTheme ? systemPalette.base : (amoledTheme ? "#080808" : "#202329"))
    property color alternateBg: roleTheme ? (root.activeRolePureBlack
                                                ? root.themeRole("surface_container", root.themeRole("surface_container_low", "#080808"))
                                                : root.themeRole("surface_container_high", root.themeRole("surface_variant", "#2b2930")))
                                            : (systemTheme ? systemPalette.alternateBase : (amoledTheme ? "#101010" : "#272b32"))
    property color buttonBg: roleTheme ? root.themeRole(root.activeRolePureBlack ? "surface_container" : "surface_container_high", "#2b2930")
                                       : (systemTheme ? systemPalette.button : (amoledTheme ? "#111111" : "#2b3038"))
    property color selectedBg: roleTheme ? root.themeRole("primary_container", "#4f378b")
                                         : (systemTheme ? Qt.rgba(systemPalette.highlight.r, systemPalette.highlight.g, systemPalette.highlight.b, 0.22) : (amoledTheme ? "#241b38" : "#343043"))
    property color hoverBg: roleTheme ? root.themeRole(root.activeRolePureBlack ? "surface_container_high" : "surface_container_highest",
                                                       root.themeRole("surface_container_high", "#36343b"))
                                      : (systemTheme ? systemPalette.alternateBase : (amoledTheme ? "#101010" : "#292d34"))
    property color dividerColor: roleTheme ? root.themeRole("outline_variant", root.themeRole("outline", "#49454f"))
                                           : (systemTheme ? systemPalette.button : (amoledTheme ? "#242424" : "#343841"))
    property color statusBg: roleTheme ? (root.activeRolePureBlack
                                             ? root.themeRole("surface", "#000000")
                                             : root.themeRole("surface_container", "#211f26"))
                                       : (systemTheme ? systemPalette.base : (amoledTheme ? "#080808" : "#252932"))
    property color textColor: roleTheme ? root.themeRole("on_surface", "#e6e1e5") : (systemTheme ? systemPalette.windowText : "#f0f0f0")
    property color mutedTextColor: roleTheme ? root.themeRole("on_surface_variant", "#cac4d0") : (systemTheme ? systemPalette.text : "#a8adb7")
    property color successColor: roleTheme ? root.themeRole("tertiary", activeRoleDark ? "#9be9a8" : "#176b2c")
                                           : (systemTheme ? (systemDark ? "#9be9a8" : "#176b2c") : "#9be9a8")
    property color warningColor: roleTheme ? root.themeRole("secondary", activeRoleDark ? "#e7d57f" : "#8a5b00")
                                           : (systemTheme ? (systemDark ? "#e7d57f" : "#8a5b00") : "#e7d57f")
    property color infoColor: roleTheme ? root.themeRole("primary", activeRoleDark ? "#b9c6ff" : "#3157a4")
                                        : (systemTheme ? (systemDark ? "#b9c6ff" : "#3157a4") : "#b9c6ff")

    // Keep compact status badges on Reno119's original semantic palette even when
    // Material/Noctalia themes the surrounding UI. This preserves their familiar
    // at-a-glance meaning and avoids turning every badge into the current accent.
    property color badgeSuccessColor: systemTheme ? (systemDark ? "#9be9a8" : "#176b2c")
                                                   : (roleTheme && !activeRoleDark ? "#176b2c" : "#9be9a8")
    property color badgeWarningColor: systemTheme ? (systemDark ? "#e7d57f" : "#8a5b00")
                                                   : (roleTheme && !activeRoleDark ? "#8a5b00" : "#e7d57f")
    property color badgeInfoColor: systemTheme ? (systemDark ? "#b9c6ff" : "#3157a4")
                                                : (roleTheme && !activeRoleDark ? "#3157a4" : "#b9c6ff")
    property color badgeMutedColor: systemTheme ? systemPalette.text
                                                 : (roleTheme && !activeRoleDark ? "#5f636b" : "#a8adb7")

    property int optiRefreshNonce: 0
    property int reframeworkRefreshNonce: 0
    property int appliedOptiProxyIndex: 0
    property int appliedOptiMethodIndex: 0
    property bool optiChoicesDirty: root.selectedRow >= 0 &&
                                    (optiProxyChoice.currentIndex !== root.appliedOptiProxyIndex ||
                                     optiMethodChoice.currentIndex !== root.appliedOptiMethodIndex)
    property int selectedRow: gameModel.count > 0 ? gameList.currentIndex : -1
    property var selectedGame: {
        gameModel.revision
        return selectedRow >= 0 ? gameModel.gameAt(selectedRow) : ({})
    }
    property string selectedGameKey: selectedGame.appId || ""
    property string recommendedReShadeVersion: {
        renoDxCatalog.ready
        return selectedRow >= 0 ? installer.recommendedReShadeVersion(selectedRow) : ""
    }
    property var selectedUpdateInfo: {
        installer.updateRevision
        return selectedRow >= 0 ? installer.updateInfo(selectedRow) : ({})
    }
    property var selectedOptiAnalysis: {
        optiIntegration.revision
        root.optiRefreshNonce
        gameModel.revision
        return selectedRow >= 0 ? optiIntegration.analyze(selectedRow) : ({})
    }
    property var selectedOptiPreview: {
        optiIntegration.revision
        root.optiRefreshNonce
        gameModel.revision
        return selectedRow >= 0
                ? optiIntegration.preview(selectedRow,
                                          root.optiProxyValue(optiProxyChoice.currentIndex),
                                          root.optiMethodValue(optiMethodChoice.currentIndex))
                : ({})
    }
    property var selectedReShadeInlineStatus: {
        installer.inlineStatusRevision
        return selectedRow >= 0 ? installer.inlineStatus(selectedRow, "reshade") : ({})
    }
    property var selectedReShade64InlineStatus: {
        installer.inlineStatusRevision
        return selectedRow >= 0 ? installer.inlineStatus(selectedRow, "reshade64") : ({})
    }
    property var selectedRenoDxInlineStatus: {
        installer.inlineStatusRevision
        return selectedRow >= 0 ? installer.inlineStatus(selectedRow, "renodx") : ({})
    }
    property var selectedRenoDxTweakInfo: {
        renoDxCatalog.renoDxCatalogFinished
        renoDxCatalog.rhiManifestReady
        renoDxCatalog.rhiManifestFinished
        gameModel.revision
        installer.inlineStatusRevision
        return selectedRow >= 0 ? installer.renoDxTweaksInfo(selectedRow) : ({})
    }
    property var selectedRenoDxResolutionInfo: {
        renoDxCatalog.renoDxCatalogReady
        renoDxCatalog.renoDxCatalogFinished
        renoDxCatalog.catalogStale
        renoDxCatalog.catalogRefreshing
        gameModel.revision
        return selectedRow >= 0 ? installer.renoDxResolutionInfo(selectedRow) : ({})
    }
    property string selectedRhiGameNote: {
        renoDxCatalog.rhiManifestReady
        renoDxCatalog.rhiManifestFinished
        gameModel.revision
        if (selectedRow < 0) return ""
        const title = ((selectedGame.originalName || selectedGame.name || "") + "").trim()
        return title.length > 0 ? renoDxCatalog.rhiGameNote(title) : ""
    }
    property var selectedOptiInlineStatus: {
        optiIntegration.statusRevision
        return selectedRow >= 0 ? optiIntegration.inlineStatus(selectedRow) : ({})
    }
    property var selectedReFrameworkInlineStatus: {
        installer.inlineStatusRevision
        return selectedRow >= 0 ? installer.inlineStatus(selectedRow, "reframework") : ({})
    }
    property var selectedOptiHotkey: {
        optiIntegration.revision
        root.optiRefreshNonce
        return selectedRow >= 0 ? optiIntegration.overlayHotkey(selectedRow) : ({})
    }
    property var selectedReFrameworkHotkey: {
        installer.inlineStatusRevision
        root.reframeworkRefreshNonce
        gameModel.revision
        return selectedRow >= 0 ? installer.reFrameworkHotkey(selectedRow) : ({})
    }
    property var selectedPcgwInfo: {
        pcGamingWiki.revision
        return selectedRow >= 0 ? pcGamingWiki.info(root.selectedGameKey) : ({})
    }
    property var hotkeyOptions: [
        { name: "Insert", code: 0x2D },
        { name: "Delete", code: 0x2E },
        { name: "Home", code: 0x24 },
        { name: "End", code: 0x23 },
        { name: "Page Up", code: 0x21 },
        { name: "Page Down", code: 0x22 },
        { name: "F1", code: 0x70 },
        { name: "F2", code: 0x71 },
        { name: "F3", code: 0x72 },
        { name: "F4", code: 0x73 },
        { name: "F5", code: 0x74 },
        { name: "F6", code: 0x75 },
        { name: "F7", code: 0x76 },
        { name: "F8", code: 0x77 },
        { name: "F9", code: 0x78 },
        { name: "F10", code: 0x79 },
        { name: "F11", code: 0x7A },
        { name: "F12", code: 0x7B }
    ]
    property var selectedOptiRestorePoints: {
        optiIntegration.revision
        return selectedRow >= 0 ? optiIntegration.restorePoints(selectedRow) : []
    }
    property int backupRefreshNonce: 0
    property var selectedBackupHistory: {
        installer.status
        root.backupRefreshNonce
        return selectedRow >= 0 ? installer.backupHistory(selectedRow) : []
    }
    property var selectedReFrameworkBackupHistory: {
        const source = root.selectedBackupHistory || []
        const out = []
        for (let i = 0; i < source.length; ++i) {
            if ((source[i].component || "") === "reframework")
                out.push(source[i])
        }
        return out
    }
    property var selectedOtherBackupHistory: {
        const source = root.selectedBackupHistory || []
        const out = []
        for (let i = 0; i < source.length; ++i) {
            if ((source[i].component || "") !== "reframework")
                out.push(source[i])
        }
        return out
    }
    property url pendingSettingsImportUrl: ""
    property int pendingBackupRestoreRow: -1
    property string pendingBackupRestorePath: ""
    property string pendingBackupRestoreLabel: ""
    property string pendingBackupRestoreKind: "component"
    property int pendingArtworkRow: -1
    property var optiExpandedByGame: ({})
    property bool optiExpanded: false
    property var restoreExpandedByGame: ({})
    property bool restoreExpanded: false
    property var componentExpandedByGame: ({})
    property bool detectionOverridesExpanded: false
    property bool reshadeExpanded: true
    property bool renodxExpanded: true
    property bool rhiNoteExpanded: false
    property bool reframeworkExpanded: true
    property bool bulkMode: false
    property bool advancedLibrarySettingsExpanded: false
    property bool advancedStorageSettingsExpanded: false
    property var updateCenterSelection: ({})
    property int updateCenterNonce: 0
    property var updateCenterItems: {
        installer.updateRevision
        installer.bulkUpdateAvailableComponents
        installer.updateQueueRemaining
        installer.updateCacheStatus
        renoDxCatalog.catalogRefreshing
        root.updateCenterNonce
        return installer.updateCenterItems()
    }
    property int updateCenterFilter: 0
    property var failedUpdateResults: (installer.updateResults || []).filter(function(r) { return r.outcome === "Failed" })
    property var filteredUpdateItems: (root.updateCenterItems || []).filter(function(game) {
        const components = game.components || []
        if (root.updateCenterFilter === 1) return game.actionableUpdates > 0
        if (root.updateCenterFilter === 2) return game.checked && !game.stale && game.detectedUpdates === 0
            && components.some(function(c) { return c.managed === true })
            && components.every(function(c) { return !c.managed || ((c.available || "").length > 0 && c.available !== "unknown") })
        if (root.updateCenterFilter === 3) return components.some(function(c) { return c.external || !c.managed })
        if (root.updateCenterFilter === 4) return root.failedUpdateResults.some(function(r) { return r.appId === game.appId })
        return true
    })
    function updatePreviewItems(ids, retry) {
        const items = []
        for (const game of root.updateCenterItems || []) {
            if (ids.indexOf(game.appId) < 0) continue
            for (const c of game.components || []) {
                if (!c.canUpdate) continue
                if (retry && !root.failedUpdateResults.some(function(r) {
                    return r.appId === game.appId && (r.component === c.id || (r.component === "reshade64" && c.id === "reshade"))
                })) continue
                items.push({appId: game.appId, name: game.name, component: c.id,
                            installed: c.installed, available: c.available,
                            matchTitle: c.matchTitle || "", matchMethod: c.matchMethod || "",
                            matchUrl: c.releaseUrl || "",
                            matchRequiresConfirmation: c.matchRequiresConfirmation === true})
            }
        }
        return items
    }
    function previewUpdates(rows, retry) {
        const ids = []
        for (const game of root.updateCenterItems || []) {
            if (rows.indexOf(game.row) >= 0) ids.push(game.appId)
        }
        updatePreviewDialog.gameIds = ids
        updatePreviewDialog.retry = retry
        updatePreviewDialog.items = root.updatePreviewItems(ids, retry)
        updatePreviewDialog.errorText = ""
        if (updatePreviewDialog.items.length > 0) updatePreviewDialog.open()
    }

    property int updateCenterActionableCount: {
        const items = root.updateCenterItems || []
        let total = 0
        for (let i = 0; i < items.length; ++i)
            total += items[i].actionableUpdates || 0
        return total
    }
    property int updateCenterActionableGameCount: {
        const items = root.updateCenterItems || []
        let total = 0
        for (let i = 0; i < items.length; ++i) {
            if ((items[i].actionableUpdates || 0) > 0)
                total++
        }
        return total
    }
    function updateCenterCountText() {
        const updates = root.updateCenterActionableCount
        const games = root.updateCenterActionableGameCount
        return updates + " update" + (updates === 1 ? "" : "s")
             + " · " + games + " game" + (games === 1 ? "" : "s")
    }

    function resetUpdateCenterSelection() {
        const next = ({})
        const items = root.updateCenterItems || []
        for (let i = 0; i < items.length; ++i) {
            if ((items[i].actionableUpdates || 0) > 0)
                next[items[i].appId] = true
        }
        root.updateCenterSelection = next
    }

    function setUpdateCenterSelected(appId, selected) {
        const next = Object.assign({}, root.updateCenterSelection || ({}))
        next[appId] = selected
        root.updateCenterSelection = next
    }

    function selectedUpdateRows() {
        const rows = []
        const items = root.updateCenterItems || []
        for (let i = 0; i < items.length; ++i) {
            if (root.updateCenterSelection[items[i].appId] === true && (items[i].actionableUpdates || 0) > 0)
                rows.push(items[i].row)
        }
        return rows
    }

    palette.window: windowBg
    palette.windowText: textColor
    palette.base: controlBg
    palette.alternateBase: alternateBg
    palette.text: textColor
    palette.button: buttonBg
    palette.buttonText: roleTheme ? root.themeRole("on_surface", textColor) : (systemTheme ? systemPalette.buttonText : textColor)
    palette.highlight: buttonHighlight
    palette.highlightedText: roleTheme ? root.themeRole("on_primary_container", root.themeRole("on_primary", "white")) : (systemTheme ? systemPalette.highlightedText : "white")
    palette.light: roleTheme ? root.themeRole("surface_container_highest", hoverBg) : (systemTheme ? systemPalette.light : alternateBg)
    palette.midlight: roleTheme ? root.themeRole("surface_container_high", alternateBg) : (systemTheme ? systemPalette.midlight : alternateBg)
    palette.mid: roleTheme ? root.themeRole("outline_variant", dividerColor) : (systemTheme ? systemPalette.mid : dividerColor)
    palette.dark: roleTheme ? root.themeRole("outline", dividerColor) : (systemTheme ? systemPalette.dark : dividerColor)
    palette.shadow: roleTheme ? root.themeRole("shadow", "#000000") : (systemTheme ? systemPalette.shadow : "#000000")
    palette.link: roleTheme ? root.themeRole("primary", buttonHighlight) : buttonHighlight
    palette.linkVisited: roleTheme ? root.themeRole("tertiary", buttonHighlight) : buttonHighlight
    palette.toolTipBase: roleTheme ? root.themeRole("surface_container_high", alternateBg) : alternateBg
    palette.toolTipText: textColor
    palette.placeholderText: roleTheme ? root.themeRole("on_surface_variant", mutedTextColor) : mutedTextColor
    property color buttonHighlight: roleTheme ? root.themeRole("primary", "#d0bcff") : (systemTheme ? systemPalette.highlight : "#8257e5")
    property color pursuitRed: "#ff2b2b"
    property color pursuitBlue: "#2f6bff"
    property int logoClickCount: 0
    property bool pursuitBluePhase: false
    property string pursuitNotice: ""

    Timer {
        id: pursuitFlashTimer
        interval: 600
        repeat: true
        running: appSettings.pursuitModeEnabled
        onTriggered: root.pursuitBluePhase = !root.pursuitBluePhase
        onRunningChanged: {
            if (!running) root.pursuitBluePhase = false
        }
    }

    Timer {
        id: pursuitClickResetTimer
        interval: 4000
        repeat: false
        onTriggered: root.logoClickCount = 0
    }

    Timer {
        id: pursuitNoticeTimer
        interval: 2200
        repeat: false
        onTriggered: root.pursuitNotice = ""
    }

    Timer {
        id: startupUpdateCheckTimer
        interval: 1800
        repeat: true
        running: appSettings.updateChecksOnStartup
        onTriggered: {
            if (!gameModel.scanning && !installer.busy && !installer.bulkUpdateBusy && renoDxCatalog.renoDxCatalogFinished) {
                stop()
                installer.checkAllUpdates()
            }
        }
    }

    Timer {
        id: backgroundUpdateCheckTimer
        interval: 6 * 60 * 60 * 1000
        repeat: true
        running: appSettings.updateChecksOnStartup
        onTriggered: {
            if (!gameModel.scanning && !installer.busy && !installer.bulkUpdateBusy && !installer.updateQueueBusy && renoDxCatalog.renoDxCatalogFinished)
                installer.checkAllUpdates()
        }
    }

    component RenoButton: Button {
        id: buttonControl
        hoverEnabled: true
        background: Item {
            implicitWidth: 96
            implicitHeight: 34
            Rectangle {
                x: -5
                y: 2
                width: parent.width + 10
                height: parent.height + 8
                radius: 10
                color: root.buttonHighlight
                opacity: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 0.12 : 0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
            Rectangle {
                x: -2
                y: 1
                width: parent.width + 4
                height: parent.height + 4
                radius: 8
                color: root.buttonHighlight
                opacity: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 0.20 : 0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: !buttonControl.enabled ? root.buttonBg
                      : buttonControl.down ? root.selectedBg
                      : (buttonControl.hovered || buttonControl.activeFocus) ? root.hoverBg
                      : root.buttonBg
                border.width: buttonControl.enabled && (buttonControl.hovered || buttonControl.activeFocus) ? 1 : 0
                border.color: root.buttonHighlight
            }
        }
        opacity: enabled ? 1.0 : 0.55
    }

    component RenoToolButton: ToolButton {
        id: toolButtonControl
        hoverEnabled: true
        background: Item {
            implicitWidth: 34
            implicitHeight: 34
            Rectangle {
                x: -4
                y: 2
                width: parent.width + 8
                height: parent.height + 6
                radius: 9
                color: root.buttonHighlight
                opacity: toolButtonControl.enabled && (toolButtonControl.hovered || toolButtonControl.activeFocus) ? 0.18 : 0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: toolButtonControl.down ? root.selectedBg
                      : (toolButtonControl.hovered || toolButtonControl.activeFocus) ? root.hoverBg
                      : "transparent"
                border.width: toolButtonControl.enabled && (toolButtonControl.hovered || toolButtonControl.activeFocus) ? 1 : 0
                border.color: root.buttonHighlight
            }
        }
        opacity: enabled ? 1.0 : 0.55
    }

    component StatusBadge: Rectangle {
        id: statusBadge
        property alias text: statusBadgeLabel.text
        property color accent: root.badgeMutedColor
        property string tooltip: ""
        implicitWidth: statusBadgeLabel.implicitWidth + 12
        implicitHeight: 20
        radius: 6
        color: Qt.rgba(accent.r, accent.g, accent.b, root.systemTheme ? 0.08 : 0.12)
        border.width: 1
        border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.45)
        Label {
            id: statusBadgeLabel
            anchors.centerIn: parent
            font.pixelSize: 10
            color: statusBadge.accent
        }
        MouseArea {
            id: statusBadgeHover
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
        ToolTip.visible: statusBadgeHover.containsMouse && statusBadge.tooltip.length > 0
        ToolTip.text: statusBadge.tooltip
    }

    component HealthChip: Rectangle {
        id: healthChip
        property string label: ""
        property string stateText: ""
        property color stateColor: root.mutedTextColor
        implicitWidth: healthChipRow.implicitWidth + 18
        implicitHeight: 28
        radius: 7
        color: root.controlBg
        border.width: 1
        border.color: root.dividerColor
        RowLayout {
            id: healthChipRow
            anchors.centerIn: parent
            spacing: 6
            Label { text: healthChip.label; opacity: root.roleTheme ? 0.88 : 0.68; font.pixelSize: 11 }
            Label { text: healthChip.stateText; color: healthChip.stateColor; font.bold: true; font.pixelSize: 11 }
        }
    }

    component RecommendationRow: Rectangle {
        id: recommendationRow
        property string itemName: ""
        property string detailText: ""
        property string stateText: ""
        property color stateColor: root.mutedTextColor
        property string actionText: ""
        property bool actionEnabled: true
        signal actionRequested()
        Layout.fillWidth: true
        implicitHeight: Math.max(48, recommendationContent.implicitHeight + 16)
        radius: 7
        color: root.controlBg
        border.width: 1
        border.color: root.dividerColor

        RowLayout {
            id: recommendationContent
            anchors.fill: parent
            anchors.leftMargin: 11
            anchors.rightMargin: 11
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: recommendationRow.stateColor
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: recommendationRow.itemName
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    visible: recommendationRow.detailText.length > 0
                    text: recommendationRow.detailText
                    wrapMode: Text.Wrap
                    color: root.mutedTextColor
                    font.pixelSize: 11
                    opacity: root.roleTheme ? 1.0 : 0.78
                }
            }
            Label {
                text: recommendationRow.stateText
                color: recommendationRow.stateColor
                font.bold: true
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
            RenoButton {
                visible: recommendationRow.actionText.length > 0
                text: recommendationRow.actionText
                enabled: recommendationRow.actionEnabled && !installer.busy
                onClicked: recommendationRow.actionRequested()
            }
        }
    }

    component CollapsibleHeader: Rectangle {
        id: sectionHeader
        property string titleText: ""
        property string summaryText: ""
        property bool expanded: true
        signal toggleRequested()
        Layout.fillWidth: true
        implicitHeight: 58
        radius: 8
        color: sectionHeaderMouse.containsMouse ? root.hoverBg : root.controlBg
        border.width: 1
        border.color: root.dividerColor

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { Layout.fillWidth: true; text: sectionHeader.titleText; font.bold: true }
                Label {
                    Layout.fillWidth: true
                    text: sectionHeader.summaryText
                    opacity: root.roleTheme ? 0.82 : 0.6
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            Label { text: sectionHeader.expanded ? "▴" : "▾"; font.pixelSize: 18; opacity: 0.72 }
        }
        MouseArea {
            id: sectionHeaderMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sectionHeader.toggleRequested()
        }
    }


    function reshadeBuildModel() {
        return [
            "Recommended" + (root.recommendedReShadeVersion.length > 0 ? " (" + root.recommendedReShadeVersion + ")" : ""),
            "Latest (checked at install)",
            appSettings.customReShadeSummary
        ]
    }

    function gameInitials(title) {
        const words = (title || "").trim().split(/\s+/).filter(function(word) { return word.length > 0 })
        if (words.length === 0)
            return "?"
        if (words.length === 1)
            return words[0].substring(0, 2).toUpperCase()
        return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase()
    }

    function optiProxyValue(index) {
        const values = ["recommended", "auto", "dxgi.dll", "winmm.dll", "d3d12.dll", "dbghelp.dll", "version.dll", "wininet.dll", "winhttp.dll", "OptiScaler.asi"]
        return values[Math.max(0, Math.min(index, values.length - 1))]
    }

    function optiMethodValue(index) {
        const values = ["recommended", "separate", "loadreshade", "plugins"]
        return values[Math.max(0, Math.min(index, values.length - 1))]
    }


    function statusColor(status) {
        const level = (status && status.level) ? status.level : ""
        if (level === "success") return root.successColor
        if (level === "error") return root.warningColor
        return root.infoColor
    }

    function hasInlineError(status) {
        return status && status.level === "error"
    }

    function reShadeHealthState() {
        const direct = root.selectedGame.reshadeInstalled === true
        const chain = root.selectedGame.reshade64Installed === true
        const optiDetected = root.selectedOptiAnalysis.detected === true
        const loadReShade = ((root.selectedOptiAnalysis.loadReshade || "") + "").toLowerCase() === "true"

        if (loadReShade && !chain)
            return ({ text: "⚠", color: root.warningColor })
        if (chain && (!optiDetected || !loadReShade))
            return ({ text: "⚠", color: root.warningColor })
        if (direct && chain && loadReShade)
            return ({ text: "⚠", color: root.warningColor })

        if ((root.selectedGame.reshadeExternal === true || root.selectedGame.reshade64External === true) &&
            root.selectedGame.reshadeManaged !== true && root.selectedGame.reshade64Managed !== true)
            return ({ text: "EXT", color: root.warningColor })
        if (direct && chain)
            return ({ text: "BOTH", color: root.successColor })
        if (chain)
            return ({ text: "64 ✓", color: root.successColor })
        if (direct)
            return ({ text: root.selectedGame.reshadeExternal === true ? "EXT" : "✓",
                      color: root.selectedGame.reshadeExternal === true ? root.warningColor : root.successColor })
        return ({ text: "—", color: root.mutedTextColor })
    }

    function reShadeHealthSummary() {
        const direct = root.selectedGame.reshadeInstalled === true
        const chain = root.selectedGame.reshade64Installed === true
        const optiDetected = root.selectedOptiAnalysis.detected === true
        const loadReShade = ((root.selectedOptiAnalysis.loadReshade || "") + "").toLowerCase() === "true"

        if (loadReShade && !chain)
            return "OptiScaler expects ReShade64.dll, but it is missing"
        if (chain && !optiDetected)
            return "ReShade64 installed · OptiScaler not detected"
        if (chain && !loadReShade)
            return "ReShade64 installed · OptiScaler LoadReshade disabled"
        if (direct && chain && loadReShade)
            return "Direct proxy + ReShade64 · possible double-load"
        if (direct && chain)
            return "Direct proxy + ReShade64 installed"
        if (chain)
            return "ReShade64 · " + (root.selectedGame.reshade64External === true ? "External" : "Managed") + " · OptiScaler chain-load"
        if (direct)
            return "Direct proxy · " + (root.selectedGame.reshadeExternal === true ? "External" : "Managed")
        return "No ReShade loader detected"
    }

    function renoDxHealthState() {
        if (root.selectedGame.renodxExternal === true) return ({ text: "EXT", color: root.warningColor })
        if (root.selectedGame.renodxManaged === true) return ({ text: "✓", color: root.successColor })
        return ({ text: "—", color: root.mutedTextColor })
    }

    function reFrameworkHealthState() {
        if (root.selectedGame.reframeworkExternal === true)
            return ({ text: "EXT", color: root.warningColor })
        if (root.selectedGame.reframeworkManaged === true)
            return ({ text: "✓", color: root.successColor })
        if (root.selectedGame.reframeworkSupported === true &&
            (root.selectedGame.reshadeInstalled === true || root.selectedGame.reshade64Installed === true || root.selectedGame.renodxInstalled === true))
            return ({ text: "⚠", color: root.warningColor })
        return ({ text: "—", color: root.mutedTextColor })
    }

    function reFrameworkHealthSummary() {
        if (root.selectedGame.reframeworkExternal === true) return "REFramework detected externally"
        if (root.selectedGame.reframeworkManaged === true)
            return "REFramework " + ((root.selectedGame.reframeworkVersion || "").length > 0 ? root.reFrameworkVersionDisplay(root.selectedGame.reframeworkVersion) : "Nightly") + " · Managed"
        if (root.selectedGame.reframeworkSupported === true &&
            (root.selectedGame.reshadeInstalled === true || root.selectedGame.reshade64Installed === true || root.selectedGame.renodxInstalled === true))
            return "RE Engine title has ReShade/RenoDX but REFramework is missing"
        return root.selectedGame.reframeworkSupported === true ? ((root.selectedGame.engine || "") === "RE Engine" ? "RE Engine title · REFramework not installed" : "Known REFramework title · not installed") : "REFramework not applicable"
    }

    function reFrameworkVersionDisplay(version) {
        const value = (version || "") + ""
        const match = value.match(/^nightly-([0-9]+)/i)
        return match ? "Nightly " + match[1] : value
    }

    function recommendationColor(level) {
        if (level === "success") return root.successColor
        if (level === "warning") return root.warningColor
        if (level === "info") return root.infoColor
        return root.mutedTextColor
    }

    function openRecommendedRenoDxInstall() {
        if (root.selectedRow < 0) return
        const resolution = root.selectedRenoDxResolutionInfo || ({})
        if (resolution.requiresConfirmation === true) {
            nonExactRenoDxMatchDialog.gameRow = root.selectedRow
            nonExactRenoDxMatchDialog.externalOverwrite = root.selectedGame.renodxExternal === true
            nonExactRenoDxMatchDialog.gameName = root.selectedGame.name || "Selected game"
            nonExactRenoDxMatchDialog.matchTitle = resolution.matchedCatalogTitle || ""
            nonExactRenoDxMatchDialog.matchMethod = resolution.matchMethod || ""
            nonExactRenoDxMatchDialog.addonFile = resolution.file || ""
            nonExactRenoDxMatchDialog.matchUrl = resolution.url || ""
            nonExactRenoDxMatchDialog.open()
        } else if (root.selectedGame.renodxExternal === true) {
            installer.installRenoDxOverExternal(root.selectedRow)
        } else {
            installer.installRenoDx(root.selectedRow)
        }
    }

    function executeRecommendedSetupAction(action) {
        if (root.selectedRow < 0 || installer.busy) return
        switch (action) {
        case "reshade-install":
            installer.installReShade(root.selectedRow, "recommended")
            break
        case "reshade-external":
            externalReShadeOverwriteDialog.open()
            break
        case "reshade64-install":
            installer.installReShade64(root.selectedRow, "recommended")
            break
        case "reshade64-external":
            externalReShade64OverwriteDialog.open()
            break
        case "renodx-install":
            root.openRecommendedRenoDxInstall()
            break
        case "renodx-review":
            root.openRecommendedRenoDxInstall()
            break
        case "tweaks-preview":
            tweakPreviewDialog.reviewGame(root.selectedRow, false)
            break
        case "reframework-install":
            installer.installReFramework(root.selectedRow)
            break
        case "reframework-external":
            externalReFrameworkOverwriteDialog.open()
            break
        case "opti-fix":
            if (optiIntegration.fixSetup(root.selectedRow)) {
                root.optiRefreshNonce++
                Qt.callLater(root.loadGameChoices)
            }
            break
        }
    }

    function recommendedSetupItems() {
        if (root.selectedRow < 0) return []

        const game = root.selectedGame || ({})
        const resolution = root.selectedRenoDxResolutionInfo || ({})
        const tweaks = root.selectedRenoDxTweakInfo || ({})
        const opti = root.selectedOptiAnalysis || ({})
        const items = []

        const directReShade = game.reshadeInstalled === true
        const chainReShade = game.reshade64Installed === true
        const anyReShade = directReShade || chainReShade
        const renoUrl = (resolution.url || "") + ""
        const renoSource = (resolution.source || "") + ""
        const externalReShade = game.reshadeExternal === true || game.reshade64External === true

        if (game.graphicsApi === "Vulkan") {
            items.push({
                name: "ReShade", state: "Manual", level: "muted",
                detail: "Vulkan implicit-layer installation is not managed by Reno119."
            })
        } else if (anyReShade) {
            const layouts = []
            if (directReShade) layouts.push("direct proxy")
            if (chainReShade) layouts.push("ReShade64 chain-loader")
            items.push({
                name: "ReShade", state: externalReShade ? "Installed · External" : "Installed",
                level: externalReShade ? "warning" : "success",
                detail: layouts.join(" + ") + (root.recommendedReShadeVersion.length > 0 ? " · recommended " + root.recommendedReShadeVersion : ""),
                action: game.reshadeExternal === true ? "reshade-external"
                      : game.reshade64External === true ? "reshade64-external"
                      : game.reshadeUpdateAvailable === true && directReShade ? "reshade-install"
                      : game.reshadeUpdateAvailable === true && chainReShade ? "reshade64-install" : "",
                actionText: externalReShade ? "Take over" : (game.reshadeUpdateAvailable === true ? "Update" : "")
            })
        } else if (renoUrl.length > 0) {
            items.push({
                name: "ReShade", state: "Recommended", level: "info",
                detail: "Install a full add-on build first; RenoDX needs ReShade to load its addon.",
                action: "reshade-install", actionText: "Install"
            })
        } else {
            items.push({
                name: "ReShade", state: "Optional", level: "muted",
                detail: root.recommendedReShadeVersion.length > 0 ? "Recommended build: " + root.recommendedReShadeVersion : "Full add-on ReShade can be installed independently."
            })
        }

        if (game.renodxInstalled === true) {
            items.push({
                name: "RenoDX",
                state: game.renodxExternal === true ? "Installed · External" : "Installed",
                level: game.renodxExternal === true ? "warning" : "success",
                detail: ((game.renodxFile || "") + "").length > 0 ? game.renodxFile : (renoSource.length > 0 ? renoSource : "Addon detected"),
                action: game.renodxExternal === true ? "renodx-install" : (game.renodxUpdateAvailable === true ? (resolution.requiresConfirmation === true ? "renodx-review" : "renodx-install") : ""),
                actionText: game.renodxExternal === true ? "Take over" : (game.renodxUpdateAvailable === true ? (resolution.requiresConfirmation === true ? "Review match" : "Update") : "")
            })
        } else if (renoUrl.length > 0) {
            const needsReview = resolution.requiresConfirmation === true
            const generic = resolution.genericAvailable === true
            items.push({
                name: "RenoDX", state: resolution.supportState || "Available",
                level: (resolution.dedicatedMatch || resolution.dedicatedAvailable) ? "info" : "muted",
                detail: resolution.supportDetail || "A RenoDX addon is available.",
                action: needsReview ? "renodx-review" : "renodx-install",
                actionText: needsReview ? "Review match" : (generic ? "Install fallback" : "Install")
            })
        } else {
            items.push({ name: "RenoDX", state: resolution.supportState || "Checking support…",
                         level: (resolution.catalogFinished && !resolution.catalogReady) ? "warning" : "muted",
                         detail: resolution.supportDetail || "Waiting for the RenoDX catalog." })
        }

        if (tweaks.available === true) {
            let tweakState = "Recommended"
            let tweakLevel = "info"
            if (tweaks.profileChanged === true) { tweakState = "Update available"; tweakLevel = "warning" }
            else if (tweaks.applied === true) { tweakState = "Applied"; tweakLevel = "success" }
            items.push({
                name: "Managed tweaks", state: tweakState, level: tweakLevel,
                detail: tweaks.title || tweaks.description || "A game-specific RenoDX configuration profile is available.",
                action: (tweaks.applied !== true || tweaks.profileChanged === true) && tweaks.canApply === true ? "tweaks-preview" : "",
                actionText: tweaks.profileChanged === true ? "Preview update" : ((tweaks.applied !== true && tweaks.canApply === true) ? "Preview" : "")
            })
        } else {
            items.push({ name: "Managed tweaks", state: "None", level: "muted", detail: "No managed Engine.ini/ReShade.ini profile is currently required." })
        }

        const rhiNote = (root.selectedRhiGameNote || "").trim()
        if (rhiNote.length > 0) {
            const compactNote = rhiNote.replace(/\s+/g, " ")
            items.push({
                name: "Game note", state: "Game-specific", level: "info",
                detail: compactNote.length > 180 ? compactNote.substring(0, 177) + "…" : compactNote
            })
        }

        if (game.reframeworkSupported === true || game.reframeworkInstalled === true) {
            if (game.reframeworkInstalled === true) {
                items.push({
                    name: "REFramework",
                    state: game.reframeworkExternal === true ? "Installed · External" : "Installed",
                    level: game.reframeworkExternal === true ? "warning" : "success",
                    detail: (game.reframeworkVersion || "").length > 0 ? root.reFrameworkVersionDisplay(game.reframeworkVersion) : ((game.engine || "") === "RE Engine" ? "RE Engine integration" : "Known REFramework integration"),
                    action: game.reframeworkExternal === true ? "reframework-external" : (game.reframeworkUpdateAvailable === true ? "reframework-install" : ""),
                    actionText: game.reframeworkExternal === true ? "Take over" : (game.reframeworkUpdateAvailable === true ? "Update" : "")
                })
            } else {
                items.push({ name: "REFramework", state: "Recommended", level: "info", detail: (game.engine || "") === "RE Engine" ? "RE Engine title; REFramework is recommended." : "Known REFramework-supported title.",
                             action: "reframework-install", actionText: "Install" })
            }
        }

        if (opti.detected === true) {
            const method = root.optiMethodDisplay(opti.appliedMethod || "")
            const needsAttention = opti.conflict === true || opti.drift === true || (opti.issues || []).length > 0
            items.push({
                name: "OptiScaler", state: needsAttention ? "Needs attention" : "Detected", level: needsAttention ? "warning" : "success",
                detail: (opti.optiProxy || "Proxy detected") + " · " + method,
                action: opti.fixAvailable === true && !root.optiChoicesDirty ? "opti-fix" : "",
                actionText: opti.fixAvailable === true && !root.optiChoicesDirty ? "Fix" : ""
            })
        } else {
            items.push({ name: "OptiScaler", state: "Optional", level: "muted", detail: "Not required for RenoDX; configure it only when you want OptiScaler integration." })
        }

        return items
    }

    function recommendedSetupSafePlan() {
        if (root.selectedRow < 0) return []
        const game = root.selectedGame || ({})
        const resolution = root.selectedRenoDxResolutionInfo || ({})
        const opti = root.selectedOptiAnalysis || ({})
        const plan = []
        const direct = game.reshadeInstalled === true
        const chain = game.reshade64Installed === true
        const anyReShade = direct || chain
        const externalReShade = game.reshadeExternal === true || game.reshade64External === true

        if (game.graphicsApi !== "Vulkan") {
            if (!anyReShade && ((resolution.url || "") + "").length > 0)
                plan.push({ action: "reshade-install", component: "reshade", label: "Install ReShade" })
            else if (!externalReShade && game.reshadeUpdateAvailable === true) {
                if (direct)
                    plan.push({ action: "reshade-install", component: "reshade", label: "Update ReShade" })
                else if (chain)
                    plan.push({ action: "reshade64-install", component: "reshade64", label: "Update ReShade64" })
            }
        }

        const exactDedicatedRenoDx = resolution.dedicatedMatch === true && resolution.requiresConfirmation !== true
        if (game.renodxExternal !== true && exactDedicatedRenoDx) {
            if (game.renodxInstalled !== true)
                plan.push({ action: "renodx-install", component: "renodx", label: "Install RenoDX" })
            else if (game.renodxUpdateAvailable === true)
                plan.push({ action: "renodx-install", component: "renodx", label: "Update RenoDX" })
        }

        if (game.reframeworkExternal !== true && game.reframeworkSupported === true) {
            if (game.reframeworkInstalled !== true)
                plan.push({ action: "reframework-install", component: "reframework", label: "Install REFramework" })
            else if (game.reframeworkUpdateAvailable === true)
                plan.push({ action: "reframework-install", component: "reframework", label: "Update REFramework" })
        }

        if (opti.detected === true && opti.fixAvailable === true && !root.optiChoicesDirty)
            plan.push({ action: "opti-fix", component: "opti", label: "Fix OptiScaler integration" })

        return plan
    }

    function recommendedSetupReviewCount() {
        if (root.selectedRow < 0) return 0
        const game = root.selectedGame || ({})
        const resolution = root.selectedRenoDxResolutionInfo || ({})
        const tweaks = root.selectedRenoDxTweakInfo || ({})
        let count = 0
        if (game.reshadeExternal === true || game.reshade64External === true) count++
        if (game.renodxExternal === true) count++
        if (game.reframeworkExternal === true) count++
        if (game.renodxInstalled !== true && ((resolution.url || "") + "").length > 0 &&
            (resolution.requiresConfirmation === true || resolution.genericAvailable === true)) count++
        if (tweaks.available === true && (tweaks.applied !== true || tweaks.profileChanged === true)) count++
        return count
    }

    function recommendedSetupStatusText() {
        if (root.recommendedSetupActive)
            return root.recommendedSetupCurrent.length > 0 ? root.recommendedSetupCurrent : "Preparing recommended setup…"
        if (root.recommendedSetupRunSummary.length > 0)
            return root.recommendedSetupRunSummary
        const safe = root.recommendedSetupSafePlan().length
        const review = root.recommendedSetupReviewCount()
        if (safe === 0 && review === 0) return "Recommended setup is already satisfied."
        if (safe === 0) return review + " item" + (review === 1 ? " needs" : "s need") + " review."
        return safe + " safe action" + (safe === 1 ? "" : "s") + (review > 0 ? " · " + review + " need review" : "")
    }

    function stopRecommendedSetup(message) {
        root.recommendedSetupActive = false
        root.recommendedSetupQueue = []
        root.recommendedSetupWaitingForBusy = false
        root.recommendedSetupCurrent = ""
        root.recommendedSetupCurrentComponent = ""
        root.recommendedSetupRunSummary = message || "Recommended setup stopped."
    }

    function startRecommendedSetup() {
        if (root.selectedRow < 0 || installer.busy || installer.updateQueueBusy || installer.bulkUpdateBusy || root.recommendedSetupActive)
            return
        const plan = root.recommendedSetupSafePlan()
        if (plan.length === 0) {
            root.recommendedSetupRunSummary = root.recommendedSetupReviewCount() > 0
                ? "No automatic actions are safe right now; review the highlighted items individually."
                : "Recommended setup is already satisfied."
            return
        }
        root.recommendedSetupRow = root.selectedRow
        root.recommendedSetupAppId = (root.selectedGame.appId || "") + ""
        root.recommendedSetupQueue = plan.slice(0)
        root.recommendedSetupCompleted = 0
        root.recommendedSetupWaitingForBusy = false
        root.recommendedSetupRunSummary = ""
        root.recommendedSetupActive = true
        Qt.callLater(root.runNextRecommendedSetupStep)
    }

    function runNextRecommendedSetupStep() {
        if (!root.recommendedSetupActive || installer.busy) return
        const currentGame = gameModel.gameAt(root.recommendedSetupRow) || ({})
        if (((currentGame.appId || "") + "") !== root.recommendedSetupAppId) {
            root.stopRecommendedSetup("Recommended setup stopped because the selected library row changed.")
            return
        }
        if ((root.recommendedSetupQueue || []).length === 0) {
            const completed = root.recommendedSetupCompleted
            root.recommendedSetupActive = false
            root.recommendedSetupCurrent = ""
            root.recommendedSetupCurrentComponent = ""
            root.recommendedSetupRunSummary = "Recommended setup complete · " + completed + " action" + (completed === 1 ? "" : "s") + " applied."
            root.verificationNonce++
            root.optiRefreshNonce++
            return
        }

        const queue = root.recommendedSetupQueue.slice(0)
        const step = queue.shift()
        root.recommendedSetupQueue = queue
        root.recommendedSetupCurrent = step.label || "Applying recommended setup…"
        root.recommendedSetupCurrentComponent = step.component || ""

        if (step.action === "opti-fix") {
            if (optiIntegration.fixSetup(root.recommendedSetupRow)) {
                root.recommendedSetupCompleted++
                root.optiRefreshNonce++
                Qt.callLater(root.loadGameChoices)
                Qt.callLater(root.runNextRecommendedSetupStep)
            } else {
                root.stopRecommendedSetup("Recommended setup stopped: OptiScaler repair could not be applied.")
            }
            return
        }

        if (step.action === "reshade-install")
            installer.installReShade(root.recommendedSetupRow, "recommended")
        else if (step.action === "reshade64-install")
            installer.installReShade64(root.recommendedSetupRow, "recommended")
        else if (step.action === "renodx-install")
            installer.installRenoDx(root.recommendedSetupRow)
        else if (step.action === "reframework-install")
            installer.installReFramework(root.recommendedSetupRow)

        if (installer.busy) {
            root.recommendedSetupWaitingForBusy = true
        } else {
            root.stopRecommendedSetup("Recommended setup stopped before " + (step.label || "the next action") + " could start: " + (installer.status || "unknown reason"))
        }
    }

    function overlayHotkeyConflict() {
        // Only warn about a real overlay collision when both components are
        // actually installed/detected. Matching defaults alone are harmless.
        return root.selectedGame.reframeworkInstalled === true &&
               root.selectedOptiAnalysis.detected === true &&
               root.selectedOptiHotkey.available === true &&
               root.selectedReFrameworkHotkey.code !== undefined &&
               root.selectedOptiHotkey.code === root.selectedReFrameworkHotkey.code
    }

    function hotkeyOptionIndex(code) {
        for (let i = 0; i < root.hotkeyOptions.length; ++i) {
            if (root.hotkeyOptions[i].code === code) return i
        }
        return 0
    }

    function openHotkeyDialog(target) {
        overlayHotkeyDialog.target = target
        const current = target === "opti" ? root.selectedOptiHotkey : root.selectedReFrameworkHotkey
        overlayHotkeyDialog.currentIndex = root.hotkeyOptionIndex(current.code !== undefined ? current.code : 0x2D)
        overlayHotkeyDialog.open()
    }

    function optiHealthState() {
        if (root.optiChoicesDirty) return ({ text: "●", color: root.warningColor })
        if (root.selectedOptiAnalysis.conflict === true ||
            root.selectedOptiAnalysis.drift === true ||
            (root.selectedOptiAnalysis.issues || []).length > 0)
            return ({ text: "⚠", color: root.warningColor })
        if (root.selectedOptiAnalysis.detected === true) return ({ text: "✓", color: root.successColor })
        return ({ text: "—", color: root.mutedTextColor })
    }

    function optiMethodDisplay(method) {
        const value = (method || "").toLowerCase()
        if (value === "loadreshade") return "Load ReShade"
        if (value === "plugins") return "Plugins folder"
        if (value === "separate") return "Separate proxies"
        return value.length > 0 ? method : "Not applied"
    }

    function optiCollapsedSummary() {
        if (root.selectedRow < 0) return ""
        const bits = []
        if (root.selectedOptiAnalysis.detected === true) {
            bits.push(root.selectedOptiAnalysis.optiProxy || "proxy unknown")
            bits.push(root.optiMethodDisplay(root.selectedOptiAnalysis.appliedMethod || ""))
        } else {
            bits.push("Not detected")
        }
        if (root.optiChoicesDirty) bits.push("Pending changes")
        else if (root.hasInlineError(root.selectedOptiInlineStatus) || root.hasInlineError(root.selectedReShade64InlineStatus) || root.selectedOptiAnalysis.conflict === true || root.selectedOptiAnalysis.drift === true || (root.selectedOptiAnalysis.issues || []).length > 0) bits.push("Needs attention")
        else if (root.selectedOptiAnalysis.detected === true) bits.push("Applied")
        return bits.join(" · ")
    }

    function defaultOptiExpanded() {
        return root.selectedOptiAnalysis.detected === true ||
               root.hasInlineError(root.selectedOptiInlineStatus) ||
               root.hasInlineError(root.selectedReShade64InlineStatus) ||
               root.selectedOptiAnalysis.conflict === true ||
               root.selectedOptiAnalysis.drift === true ||
               root.optiChoicesDirty
    }

    function loadOptiExpansionState() {
        const key = root.selectedGameKey
        if (!key) {
            root.optiExpanded = false
            return
        }
        if (root.optiExpandedByGame[key] !== undefined)
            root.optiExpanded = root.optiExpandedByGame[key]
        else
            root.optiExpanded = root.defaultOptiExpanded()
    }

    function setOptiExpanded(value) {
        root.optiExpanded = value
        const key = root.selectedGameKey
        if (!key) return
        const next = Object.assign({}, root.optiExpandedByGame)
        next[key] = value
        root.optiExpandedByGame = next
    }

    function loadRestoreExpansionState() {
        const key = root.selectedGameKey
        if (!key) {
            root.restoreExpanded = false
            return
        }
        root.restoreExpanded = root.restoreExpandedByGame[key] === true
    }

    function setRestoreExpanded(value) {
        root.restoreExpanded = value
        const key = root.selectedGameKey
        if (!key) return
        const next = Object.assign({}, root.restoreExpandedByGame)
        next[key] = value
        root.restoreExpandedByGame = next
    }

    function restoreCollapsedSummary() {
        const componentCount = (root.selectedOtherBackupHistory || []).length
        const reframeworkCount = (root.selectedReFrameworkBackupHistory || []).length
        const integrationCount = (root.selectedOptiRestorePoints || []).length
        return componentCount + " ReShade/RenoDX · " + reframeworkCount + " REFramework · " + integrationCount + " OptiScaler"
    }

    function loadComponentExpansionState() {
        const key = root.selectedGameKey
        if (!key) {
            root.detectionOverridesExpanded = false
            root.reshadeExpanded = true
            root.renodxExpanded = true
            root.rhiNoteExpanded = false
            root.reframeworkExpanded = true
            return
        }
        const state = root.componentExpandedByGame[key] || ({})
        root.detectionOverridesExpanded = state.detectionOverrides !== undefined ? state.detectionOverrides : false
        root.reshadeExpanded = state.reshade !== undefined ? state.reshade : true
        root.renodxExpanded = state.renodx !== undefined ? state.renodx : true
        root.rhiNoteExpanded = state.rhiNote !== undefined ? state.rhiNote : false
        root.reframeworkExpanded = state.reframework !== undefined ? state.reframework : true
    }

    function setComponentExpanded(component, value) {
        if (component === "detectionOverrides") root.detectionOverridesExpanded = value
        else if (component === "reshade") root.reshadeExpanded = value
        else if (component === "renodx") root.renodxExpanded = value
        else if (component === "rhiNote") root.rhiNoteExpanded = value
        else if (component === "reframework") root.reframeworkExpanded = value
        const key = root.selectedGameKey
        if (!key) return
        const next = Object.assign({}, root.componentExpandedByGame)
        const state = Object.assign({}, next[key] || ({}))
        state[component] = value
        next[key] = state
        root.componentExpandedByGame = next
    }

    function scrollDetailsTo(item) {
        if (!item || !detailsScrollView.contentItem) return
        const flick = detailsScrollView.contentItem
        if (flick.contentY !== undefined)
            flick.contentY = Math.max(0, item.y - 16)
    }

    function activateHealthTarget() {
        if (root.selectedRow < 0) return
        const target = root.selectedGame.healthTarget || "none"
        if (target === "diagnostics") {
            libraryDiagnosticsPopup.openFor(root.selectedRow)
            return
        }
        if (target === "updates") {
            if (root.selectedGame.reshadeUpdateAvailable === true) {
                root.setComponentExpanded("reshade", true)
                Qt.callLater(function() { root.scrollDetailsTo(reshadeCard) })
            } else if (root.selectedGame.renodxUpdateAvailable === true) {
                root.setComponentExpanded("renodx", true)
                Qt.callLater(function() { root.scrollDetailsTo(renodxCard) })
            } else if (root.selectedGame.reframeworkUpdateAvailable === true) {
                root.setComponentExpanded("reframework", true)
                Qt.callLater(function() { root.scrollDetailsTo(reframeworkCard) })
            }
            return
        }
        if (target === "components") {
            if (root.selectedGame.reshadeExternal === true || root.selectedGame.reshade64External === true) {
                root.setComponentExpanded("reshade", true)
                Qt.callLater(function() { root.scrollDetailsTo(reshadeCard) })
            } else if (root.selectedGame.renodxExternal === true) {
                root.setComponentExpanded("renodx", true)
                Qt.callLater(function() { root.scrollDetailsTo(renodxCard) })
            } else if (root.selectedGame.reframeworkExternal === true) {
                root.setComponentExpanded("reframework", true)
                Qt.callLater(function() { root.scrollDetailsTo(reframeworkCard) })
            }
        }
    }

    function ownershipLabel(installed, managed, external) {
        if (!installed) return "Not installed"
        if (managed) return "Managed"
        if (external) return "External"
        return "Detected"
    }

    function loadGameChoices() {
        if (root.selectedRow < 0)
            return
        reshadeBuildChoice.currentIndex = gameModel.reShadeChoiceIndex(root.selectedRow)
        const applied = optiIntegration.appliedChoices(root.selectedRow)
        root.appliedOptiProxyIndex = applied.proxyIndex !== undefined ? applied.proxyIndex : gameModel.optiProxyChoiceIndex(root.selectedRow)
        root.appliedOptiMethodIndex = applied.methodIndex !== undefined ? applied.methodIndex : gameModel.optiMethodChoiceIndex(root.selectedRow)
        optiProxyChoice.currentIndex = root.appliedOptiProxyIndex
        optiMethodChoice.currentIndex = root.appliedOptiMethodIndex
    }

    property var graphicsApiOverrideChoices: ["Auto", "DirectX 9", "DirectX 10", "DirectX 11", "DirectX 12", "Vulkan", "OpenGL"]

    function loadDetectionOverrides() {
        if (root.selectedRow < 0) return
        exeOverrideField.text = gameModel.executableOverride(root.selectedRow) || ""
        prefixOverrideField.text = gameModel.prefixOverride(root.selectedRow) || ""
        const api = gameModel.graphicsApiOverride(root.selectedRow) || ""
        let index = 0
        for (let i = 1; i < root.graphicsApiOverrideChoices.length; ++i) {
            if (root.graphicsApiOverrideChoices[i] === api) { index = i; break }
        }
        graphicsApiOverrideChoice.currentIndex = index
    }

    onSelectedGameKeyChanged: {
        Qt.callLater(root.restoreDetailsPosition)
        Qt.callLater(root.loadGameNotes)
        Qt.callLater(root.loadGameChoices)
        Qt.callLater(root.loadDetectionOverrides)
        Qt.callLater(root.loadOptiExpansionState)
        Qt.callLater(root.loadRestoreExpansionState)
        Qt.callLater(root.loadComponentExpansionState)
    }
    Component.onCompleted: {
        Qt.callLater(root.loadGameNotes)
        Qt.callLater(root.loadGameChoices)
        Qt.callLater(root.loadDetectionOverrides)
        Qt.callLater(root.loadOptiExpansionState)
        Qt.callLater(root.loadRestoreExpansionState)
        Qt.callLater(root.loadComponentExpansionState)
    }

    property var notesExpandedByGame: ({})
    property bool notesExpanded: notesExpandedByGame[selectedGameKey] === true
    property bool toolsExpanded: false
    property bool actionsExpanded: false
    property bool recommendedSetupActive: false
    property var recommendedSetupQueue: []
    property int recommendedSetupRow: -1
    property string recommendedSetupAppId: ""
    property string recommendedSetupCurrent: ""
    property string recommendedSetupCurrentComponent: ""
    property int recommendedSetupCompleted: 0
    property bool recommendedSetupWaitingForBusy: false
    property string recommendedSetupRunSummary: ""
    property int verificationNonce: 0
    property var setupVerification: {
        root.verificationNonce
        gameModel.revision
        return root.selectedRow < 0 ? ({}) : installer.verificationInfo(root.selectedRow)
    }
    property var recoveryCopies: {
        installer.status
        gameModel.revision
        return root.selectedRow < 0 ? [] : installer.recoveryHistory(root.selectedRow)
    }
    property var rememberedView: appSettings.viewState()
    property bool viewRestored: false
    property bool restoringScroll: false
    property string scrollGameKey: ""
    function restoreView() {
        if (viewRestored || (gameModel.scanning && !gameModel.cachedSnapshot) || gameModel.count === 0) return
        for (let row = 0; row < gameModel.count; ++row) {
            if (gameModel.gameAt(row).appId === rememberedView.gameId) {
                gameList.currentIndex = row
                break
            }
        }
        componentExpandedByGame = rememberedView.components || ({})
        notesExpandedByGame = rememberedView.notes || ({})
        restoreExpandedByGame = rememberedView.restore || ({})
        optiExpandedByGame = rememberedView.opti || ({})
        toolsExpanded = rememberedView.tools === true
        actionsExpanded = rememberedView.actions === true
        root.loadComponentExpansionState()
        root.loadRestoreExpansionState()
        root.loadOptiExpansionState()
        viewRestored = true
        restoringScroll = true
        Qt.callLater(function() {
            gameList.contentY = Math.max(gameList.originY, Math.min(Math.max(gameList.originY, gameList.originY + gameList.contentHeight - gameList.height), rememberedView.listY || 0))
            root.restoreDetailsPosition()
        })
    }
    function restoreDetailsPosition() {
        if (!viewRestored || !detailsScrollView.contentItem) return
        restoringScroll = true
        const positions = rememberedView.positions || ({})
        const view = detailsScrollView.contentItem
        const maximum = Math.max(view.originY || 0, (view.originY || 0) + view.contentHeight - view.height)
        view.contentY = Math.max(view.originY || 0, Math.min(maximum, positions[selectedGameKey] || 0))
        scrollGameKey = selectedGameKey
        restoringScroll = false
    }
    function rememberView() {
        if (!viewRestored || restoringScroll || gameModel.scanning || selectedRow < 0 || !detailsScrollView.contentItem) return
        const positions = Object.assign({}, rememberedView.positions || ({}))
        if (scrollGameKey === selectedGameKey) positions[selectedGameKey] = detailsScrollView.contentItem.contentY
        const next = { gameId: selectedGameKey, listY: gameList.contentY, positions: positions,
                       components: componentExpandedByGame, notes: notesExpandedByGame,
                       restore: restoreExpandedByGame, opti: optiExpandedByGame,
                       tools: toolsExpanded, actions: actionsExpanded }
        if (JSON.stringify(next) !== JSON.stringify(rememberedView)) {
            rememberedView = next
            appSettings.saveViewState(next)
        }
    }
    Timer {
        interval: 800
        repeat: true
        running: true
        onTriggered: { root.restoreView(); root.rememberView() }
    }
    onClosing: root.rememberView()

    property var noteDrafts: ({})
    function loadGameNotes() {
        if (!gameNotesEditor) return
        gameNotesEditor.loading = true
        gameNotesEditor.noteKey = root.selectedGameKey
        gameNotesEditor.text = root.noteDrafts[root.selectedGameKey] !== undefined
                ? root.noteDrafts[root.selectedGameKey] : appSettings.gameNotes(root.selectedGameKey)
        gameNotesEditor.loading = false
        gameNotesEditor.saveStatus = root.noteDrafts[root.selectedGameKey] !== undefined ? "Unsaved draft — retry saving" : "Saved locally"
    }
    function saveGameNotes() {
        if (!gameNotesEditor || gameNotesEditor.loading || gameNotesEditor.noteKey.length === 0) return
        const key = gameNotesEditor.noteKey
        root.noteDrafts[key] = gameNotesEditor.text
        if (appSettings.setGameNotes(key, gameNotesEditor.text)) {
            delete root.noteDrafts[key]
            gameNotesEditor.saveStatus = "Saved locally"
        } else gameNotesEditor.saveStatus = "Could not save — draft kept in this session. Retry saving."
    }
    property string selectedRecoveryFolder: {
        installer.inlineStatusRevision
        installer.status
        root.backupRefreshNonce
        gameModel.revision
        return root.selectedRow < 0 ? "" : installer.recoveryFolder(root.selectedRow)
    }
    function unavailableActions() {
        const reasons = []
        const game = root.selectedGame || ({})
        if (installer.busy) reasons.push("Component install, remove and restore: wait for the current operation to finish.")
        if (game.graphicsApi === "Vulkan") reasons.push("Install ReShade: Vulkan installation is not supported.")
        if (reshadeBuildChoice.currentIndex === 2 && !appSettings.customReShadeConfigured)
            reasons.push("Install ReShade / ReShade64: configure the Custom build in Settings first.")
        if (game.architecture !== "x64") reasons.push("Install ReShade64: requires a 64-bit executable.")
        if (game.reshadeInstalled !== true && game.reshade64Installed !== true)
            reasons.push("Install RenoDX: install ReShade or ReShade64 first.")
        if (root.selectedRenoDxTweakInfo.canApply !== true && root.selectedRenoDxTweakInfo.available === true)
            reasons.push("Apply tweaks: " + (root.selectedRenoDxTweakInfo.profilePending === true ? "catalog still loading." : game.renodxInstalled !== true ? "install RenoDX first." : root.selectedRenoDxTweakInfo.engineStatus || "profile target is unavailable."))
        if (game.reframeworkSupported !== true && game.reframeworkExternal !== true)
            reasons.push("Install REFramework: no supported title or external installation detected.")
        const removals = []
        if (game.reshadeManaged !== true) removals.push("ReShade")
        if (game.reshade64Managed !== true) removals.push("ReShade64")
        if (game.renodxManaged !== true) removals.push("RenoDX")
        if (game.reframeworkManaged !== true) removals.push("REFramework")
        if (removals.length > 0) reasons.push("Remove " + removals.join(" / ") + ": no Reno119-managed installation.")
        if (root.selectedRenoDxTweakInfo.canRestore !== true) reasons.push("Restore original tweaks: no managed tweak snapshot.")
        if ((root.selectedBackupHistory || []).length === 0) reasons.push("Restore component backup: no backup available.")
        if (!root.optiChoicesDirty && root.selectedOptiPreview.canApply !== true)
            reasons.push("Apply OptiScaler integration: " + (root.selectedOptiPreview.summary || "review the integration requirements and issues above."))
        else if (!root.optiChoicesDirty && root.selectedOptiPreview.hasFileChanges !== true && root.selectedOptiAnalysis.drift !== true)
            reasons.push("Apply OptiScaler integration: no pending file changes.")
        if (!root.optiChoicesDirty && !(root.selectedOptiAnalysis.lastTransactionDir || "").length)
            reasons.push("Revert integration: no previous transaction or pending choices.")
        if (root.selectedOptiAnalysis.detected !== true) reasons.push("Save working setup: OptiScaler is not detected.")
        if (root.selectedOptiAnalysis.workingSaved !== true) reasons.push("Restore working setup: no saved setup.")
        return reasons
    }

    function integrationHistoryText() {
        if (root.selectedRow < 0)
            return ""
        const entries = optiIntegration.history(root.selectedRow)
        if (!entries || entries.length === 0)
            return "No Reno119 integration changes recorded yet."
        let result = ""
        const count = Math.min(entries.length, 4)
        for (let i = 0; i < count; ++i) {
            if (i > 0)
                result += "\n"
            result += entries[i]
        }
        return result
    }

    function diagnosticsList(value) {
        if (!value || value.length === 0)
            return "(none)"
        let out = ""
        for (let i = 0; i < value.length; ++i)
            out += (i > 0 ? "\n" : "") + "  - " + value[i]
        return out
    }

    function diagnosticsStatus(name, status) {
        if (!status || !status.text)
            return name + ": (no recorded action)"
        return name + ": [" + (status.level || "info") + "] " + status.text
    }

    function diagnosticsMap(value) {
        if (!value)
            return "(none)"
        const keys = Object.keys(value).sort()
        if (keys.length === 0)
            return "(none)"
        let out = ""
        for (let i = 0; i < keys.length; ++i) {
            const key = keys[i]
            const item = value[key]
            out += (i > 0 ? "\n" : "") + "  - " + key + ": " + ((item === undefined || item === null || String(item).length === 0) ? "(empty)" : String(item))
        }
        return out
    }

    function libraryProxyScanText(entries) {
        if (!entries || entries.length === 0)
            return "(none)"
        let out = ""
        for (let i = 0; i < entries.length; ++i) {
            const item = entries[i] || ({})
            out += (i > 0 ? "\n" : "") + "  - " + (item.file || "?") + " — " + (item.classification || "unknown") + (item.managed === true ? " [managed]" : " [unmanaged]")
        }
        return out
    }

    function diagnosticsReport(redact = true) {
        if (root.selectedRow < 0)
            return "Reno119 diagnostics\nNo game selected.\n"

        const opti = root.selectedOptiAnalysis || ({})
        const pcgw = root.selectedPcgwInfo || ({})
        const libraryDiag = gameModel.diagnosticInfo(root.selectedRow) || ({})
        const renodx = installer.renoDxResolutionInfo(root.selectedRow) || ({})
        const updates = root.selectedUpdateInfo || ({})
        const scan = opti.proxyScan || []
        let scanText = "(none)"
        if (scan.length > 0) {
            scanText = ""
            for (let i = 0; i < scan.length; ++i) {
                const item = scan[i] || ({})
                scanText += (i > 0 ? "\n" : "") + "  - " + (item.file || "?") + " — " + (item.role || "unknown") + " [" + (item.severity || "info") + "]"
            }
        }

        let pcgwText = "Not queried"
        if (pcgw.pending === true)
            pcgwText = "Lookup pending"
        else if (pcgw.resolved === true)
            pcgwText = (pcgw.title || "Resolved") + " — " + (pcgw.url || "")
        else if (pcgw.queried === true)
            pcgwText = "Lookup failed — " + (pcgw.error || "unknown error")

        const lines = []
        lines.push(installer.debugSummary(root.selectedRow))
        lines.push("")
        lines.push("Health")
        lines.push("ReShade: " + root.reShadeHealthSummary())
        lines.push("RenoDX: " + (root.selectedGame.renodxInstalled === true ? root.ownershipLabel(true, root.selectedGame.renodxManaged === true, root.selectedGame.renodxExternal === true) : "Not installed"))
        lines.push("REFramework: " + root.reFrameworkHealthSummary())
        lines.push("OptiScaler: " + (opti.health || opti.summary || "Not detected"))
        lines.push("Integrity: " + (root.selectedGame.integrityWarning === true ? (root.selectedGame.integritySummary || "warning") : "OK"))
        lines.push("")
        lines.push("Library / launcher resolution")
        lines.push("Status: [" + (libraryDiag.level || "unknown") + "] " + (libraryDiag.summary || ""))
        lines.push("Install path: " + (libraryDiag.installPath || "(unresolved)"))
        lines.push("Executable: " + (libraryDiag.executable || "(unresolved)") + (libraryDiag.executableOverridden === true ? " [override]" : " [auto]"))
        lines.push("Auto-detected executable: " + (libraryDiag.detectedExecutable || "(unresolved)"))
        lines.push("Wine/Proton prefix: " + (libraryDiag.prefix || "(unresolved)") + (libraryDiag.prefixOverridden === true ? " [override]" : " [auto]"))
        lines.push("Auto-detected prefix: " + (libraryDiag.detectedPrefix || "(unresolved)"))
        lines.push("Graphics API: " + (libraryDiag.graphicsApi || "Unknown") + (libraryDiag.graphicsApiOverridden === true ? " [override]" : " [auto]"))
        lines.push("Auto-detected graphics API: " + (libraryDiag.detectedGraphicsApi || "Unknown"))
        lines.push("Graphics detection evidence:\n" + root.diagnosticsList(libraryDiag.graphicsApiEvidence || []))
        lines.push("Problems:\n" + root.diagnosticsList(libraryDiag.problems || []))
        lines.push("Warnings:\n" + root.diagnosticsList(libraryDiag.warnings || []))
        lines.push("Importer notes:\n" + root.diagnosticsList(libraryDiag.importNotes || []))
        lines.push("Launcher metadata:\n" + root.diagnosticsMap(libraryDiag.sourceMetadata || ({})))
        lines.push("Detection history:\n" + root.diagnosticsList(libraryDiag.detectionHistory || []))
        lines.push("Proxy / loader ownership:\n" + root.libraryProxyScanText(libraryDiag.proxyScan || []))
        lines.push("Proxy warnings:\n" + root.diagnosticsList(libraryDiag.proxyWarnings || []))
        lines.push("")
        lines.push("RenoDX resolution")
        lines.push("Source: " + (renodx.source || "unknown"))
        lines.push("File: " + (renodx.file || "(none)"))
        lines.push("URL: " + (renodx.url || "(none)"))
        lines.push("Catalog ready: " + (renodx.catalogReady === true ? "yes" : "no"))
        lines.push("Catalog finished: " + (renodx.catalogFinished === true ? "yes" : "no"))
        lines.push("")
        lines.push("OptiScaler integration")
        lines.push("Detected: " + (opti.detected === true ? "yes" : "no"))
        lines.push("INI: " + (opti.iniPath || "(none)"))
        lines.push("Proxy: " + (opti.optiProxy || "(none)"))
        lines.push("Applied method: " + (opti.appliedMethod || "(none)"))
        lines.push("LoadReshade: " + (opti.loadReshade || "(unset)"))
        lines.push("Normal ReShade path: " + (opti.normalReShadePath || "(none)"))
        lines.push("ReShade64 path: " + (opti.reshade64Path || "(none)"))
        lines.push("Drift detected: " + (opti.drift === true ? "yes" : "no"))
        lines.push("Conflict detected: " + (opti.conflict === true ? "yes" : "no"))
        lines.push("Issues:\n" + root.diagnosticsList(opti.issues || []))
        lines.push("Drift details:\n" + root.diagnosticsList(opti.driftDetails || []))
        lines.push("Proxy scan:\n" + scanText)
        lines.push("")
        lines.push("Suggested launch configuration (advisory only; not applied by Reno119)")
        lines.push(root.selectedGame.source === "Steam" ? (root.selectedGame.launchOptions || "(none)") : (root.selectedGame.wineOverrides || "(none)"))
        lines.push("")
        lines.push("Update state")
        lines.push("Checked: " + (root.selectedGame.updateChecked === true ? "yes" : "no"))
        lines.push("ReShade update available: " + (root.selectedGame.reshadeUpdateAvailable === true ? "yes" : "no"))
        lines.push("RenoDX update available: " + (root.selectedGame.renodxUpdateAvailable === true ? "yes" : "no"))
        lines.push("REFramework update available: " + (root.selectedGame.reframeworkUpdateAvailable === true ? "yes" : "no"))
        if (updates.reshadeSummary) lines.push("ReShade check: " + updates.reshadeSummary)
        if (updates.renodxSummary) lines.push("RenoDX check: " + updates.renodxSummary)
        if (updates.reframeworkSummary) lines.push("REFramework check: " + updates.reframeworkSummary)
        lines.push("")
        lines.push("PCGamingWiki")
        lines.push(pcgwText)
        lines.push("")
        lines.push("Last recorded actions")
        lines.push(root.diagnosticsStatus("ReShade", root.selectedReShadeInlineStatus))
        lines.push(root.diagnosticsStatus("ReShade64", root.selectedReShade64InlineStatus))
        lines.push(root.diagnosticsStatus("RenoDX", root.selectedRenoDxInlineStatus))
        lines.push(root.diagnosticsStatus("REFramework", root.selectedReFrameworkInlineStatus))
        lines.push(root.diagnosticsStatus("OptiScaler", root.selectedOptiInlineStatus))
        lines.push("")
        lines.push("Generated: " + Qt.formatDateTime(new Date(), "yyyy-MM-dd hh:mm:ss"))
        const tweaks = root.selectedRenoDxTweakInfo || ({})
        lines.push("", "Managed RenoDX tweaks")
        lines.push("Profile: " + (tweaks.title || "None"))
        lines.push("Applied: " + (tweaks.applied === true ? "yes" : "no"))
        lines.push("Engine.ini: " + (tweaks.engineIniPath || "(unresolved)"))
        lines.push("Profile settings: " + root.diagnosticsList(tweaks.changes || []))
        lines.push("Latest recovery folder: " + (installer.recoveryFolder(root.selectedRow) || "(none)"))
        const report = lines.join("\n") + "\n"
        return redact ? installer.redactDiagnostics(report) : report
    }

    function openFilePickerAt(dialog, path, requestId, title) {
        // Keep the Qt dialog prepared as a fallback, but prefer the XDG desktop
        // portal so the running desktop/session chooses the actual file picker.
        const folder = appSettings.filePickerFolder(path || "")
        if (folder && folder.toString().length > 0)
            dialog.currentFolder = folder
        portalFileDialog.openFile(requestId, title, path || "")
    }

    function openFolderPickerAt(dialog, path, requestId, title) {
        const folder = appSettings.filePickerFolder(path || "")
        if (folder && folder.toString().length > 0)
            dialog.currentFolder = folder
        portalFileDialog.openFolder(requestId, title, path || "")
    }

    function openSavePicker(dialog, requestId, title, suggestedName, path) {
        const folder = appSettings.filePickerFolder(path || "")
        if (folder && folder.toString().length > 0)
            dialog.currentFolder = folder
        portalFileDialog.saveFile(requestId, title, path || "", suggestedName || "")
    }

    Connections {
        target: portalFileDialog

        function onAccepted(requestId, url) {
            const path = appSettings.localPathFromUrl(url)
            switch (requestId) {
            case "customReShade":
                customReShadeFileField.text = path
                appSettings.setCustomReShadeFile(path)
                break
            case "customProgramExe":
                customProgramPopup.setExecutablePath(path)
                break
            case "customProgramPrefix":
                customProgramPopup.setPrefixPath(path)
                break
            case "customProgramArtwork":
                customProgramPopup.setArtworkPath(path)
                break
            case "customArtworkQuick":
                if (root.pendingArtworkRow >= 0)
                    gameModel.setArtworkOverride(root.pendingArtworkRow, path)
                root.pendingArtworkRow = -1
                break
            case "heroicRootOverride":
                heroicRootField.text = path
                gameModel.setHeroicRootOverride(path)
                break
            case "lutrisDataRootOverride":
                lutrisDataRootField.text = path
                gameModel.setLutrisDataRootOverride(path)
                break
            case "lutrisConfigRootOverride":
                lutrisConfigRootField.text = path
                gameModel.setLutrisConfigRootOverride(path)
                break
            case "exeOverride":
                exeOverrideField.text = path
                break
            case "prefixOverride":
                prefixOverrideField.text = path
                break
            case "settingsExport":
                appSettings.exportConfiguration(url)
                break
            case "settingsImport":
                root.pendingSettingsImportUrl = url
                settingsImportConfirm.open()
                break
            case "diagnosticsExport":
                installer.saveDiagnosticsReport(url, root.diagnosticsReport())
                break
            }
        }

        function onFailed(requestId, message) {
            console.warn("XDG file chooser portal failed for " + requestId + ": " + message)
            // A native portal should normally be available on a modern Linux
            // desktop. Keep the previous QtQuick.Dialogs path as a compatibility
            // fallback rather than making Browse unusable on unusual sessions.
            switch (requestId) {
            case "customReShade": customReShadeFileDialog.open(); break
            case "customProgramExe": customProgramExeDialog.open(); break
            case "customProgramPrefix": customProgramPrefixDialog.open(); break
            case "customProgramArtwork": customProgramArtworkDialog.open(); break
            case "customArtworkQuick": customArtworkQuickDialog.open(); break
            case "heroicRootOverride": heroicRootDialog.open(); break
            case "lutrisDataRootOverride": lutrisDataRootDialog.open(); break
            case "lutrisConfigRootOverride": lutrisConfigRootDialog.open(); break
            case "exeOverride": exeOverrideDialog.open(); break
            case "prefixOverride": prefixOverrideDialog.open(); break
            case "settingsExport": settingsExportDialog.open(); break
            case "settingsImport": settingsImportDialog.open(); break
            case "diagnosticsExport": diagnosticsSaveDialog.open(); break
            }
        }
    }

    FileDialog {
        id: diagnosticsSaveDialog
        title: "Export Reno119 diagnostics"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Text reports (*.txt)", "All files (*)"]
        onAccepted: installer.saveDiagnosticsReport(selectedFile, root.diagnosticsReport())
    }

    FileDialog {
        id: settingsExportDialog
        title: "Export Reno119 backup"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Reno119 backup (*.json)", "JSON files (*.json)"]
        onAccepted: appSettings.exportConfiguration(selectedFile)
    }

    FileDialog {
        id: settingsImportDialog
        title: "Import Reno119 backup"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Reno119 backup (*.json)", "JSON files (*.json)"]
        onAccepted: {
            root.pendingSettingsImportUrl = selectedFile
            settingsImportConfirm.open()
        }
    }

    FileDialog {
        id: customReShadeFileDialog
        title: "Choose custom ReShade installer"
        nameFilters: ["Windows executables (*.exe)", "All files (*)"]
        onAccepted: {
            const path = appSettings.localPathFromUrl(selectedFile)
            customReShadeFileField.text = path
            appSettings.setCustomReShadeFile(path)
        }
    }

    FileDialog {
        id: customProgramExeDialog
        title: "Choose Windows executable"
        nameFilters: ["Windows executables (*.exe *.EXE)", "All files (*)"]
        onAccepted: customProgramPopup.setExecutablePath(appSettings.localPathFromUrl(selectedFile))
    }

    FolderDialog {
        id: customProgramPrefixDialog
        title: "Choose Wine / Proton prefix"
        onAccepted: customProgramPopup.setPrefixPath(appSettings.localPathFromUrl(selectedFolder))
    }

    FileDialog {
        id: customProgramArtworkDialog
        title: "Choose custom artwork"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)", "All files (*)"]
        onAccepted: customProgramPopup.setArtworkPath(appSettings.localPathFromUrl(selectedFile))
    }

    FileDialog {
        id: customArtworkQuickDialog
        title: "Choose custom artwork"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)", "All files (*)"]
        onAccepted: {
            if (root.pendingArtworkRow >= 0)
                gameModel.setArtworkOverride(root.pendingArtworkRow, appSettings.localPathFromUrl(selectedFile))
            root.pendingArtworkRow = -1
        }
        onRejected: root.pendingArtworkRow = -1
    }

    FolderDialog {
        id: heroicRootDialog
        title: "Choose additional Heroic config root"
        onAccepted: {
            const path = appSettings.localPathFromUrl(selectedFolder)
            heroicRootField.text = path
            gameModel.setHeroicRootOverride(path)
        }
    }

    FolderDialog {
        id: lutrisDataRootDialog
        title: "Choose additional Lutris data root"
        onAccepted: {
            const path = appSettings.localPathFromUrl(selectedFolder)
            lutrisDataRootField.text = path
            gameModel.setLutrisDataRootOverride(path)
        }
    }

    FolderDialog {
        id: lutrisConfigRootDialog
        title: "Choose additional Lutris config root"
        onAccepted: {
            const path = appSettings.localPathFromUrl(selectedFolder)
            lutrisConfigRootField.text = path
            gameModel.setLutrisConfigRootOverride(path)
        }
    }

    FileDialog {
        id: exeOverrideDialog
        title: "Choose Windows executable"
        nameFilters: ["Windows executables (*.exe *.EXE)", "All files (*)"]
        onAccepted: exeOverrideField.text = appSettings.localPathFromUrl(selectedFile)
    }

    FolderDialog {
        id: prefixOverrideDialog
        title: "Choose Wine / Proton prefix"
        onAccepted: prefixOverrideField.text = appSettings.localPathFromUrl(selectedFolder)
    }

    header: ToolBar {
        background: Rectangle { color: root.windowBg }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            Item {
                id: renoLogo
                Layout.preferredWidth: logoLabel.implicitWidth + 18
                Layout.preferredHeight: 36

                Rectangle {
                    anchors.centerIn: parent
                    width: logoLabel.implicitWidth + 18
                    height: logoLabel.implicitHeight + 12
                    radius: 9
                    color: root.pursuitBluePhase ? root.pursuitBlue : root.pursuitRed
                    opacity: appSettings.pursuitModeEnabled ? 0.22 : 0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }

                Label {
                    id: logoLabel
                    anchors.centerIn: parent
                    text: "Reno119"
                    font.pixelSize: 20
                    font.bold: true
                    color: appSettings.pursuitModeEnabled
                           ? (root.pursuitBluePhase ? root.pursuitBlue : root.pursuitRed)
                           : root.textColor
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.logoClickCount++
                        pursuitClickResetTimer.restart()
                        if (root.logoClickCount >= 10) {
                            const enabling = !appSettings.pursuitModeEnabled
                            appSettings.setPursuitModeEnabled(enabling)
                            root.logoClickCount = 0
                            root.pursuitBluePhase = false
                            root.pursuitNotice = enabling ? "🚨 Pursuit mode activated" : "Pursuit mode disabled"
                            pursuitNoticeTimer.restart()
                        }
                    }
                }
            }
            Label {
                visible: root.pursuitNotice.length > 0
                text: root.pursuitNotice
                color: appSettings.pursuitModeEnabled ? (root.pursuitBluePhase ? root.pursuitBlue : root.pursuitRed) : root.mutedTextColor
                font.pixelSize: 11
            }
            Label { text: "ReShade + RenoDX"; opacity: 0.65 }
            Item { Layout.fillWidth: true }
            Label { text: renoDxCatalog.ready ? "RenoDX catalog: live" : (renoDxCatalog.renoDxCatalogFinished ? "RenoDX catalog: unavailable" : "RenoDX catalog: loading…"); opacity: 0.7 }
            Label { text: renoDxCatalog.rhiManifestReady ? "Tweaks: live" : (renoDxCatalog.rhiManifestFinished ? "Tweaks: offline" : "Tweaks: loading…"); opacity: 0.7 }
            RenoButton {
                text: gameModel.scanning ? "Scanning…" : "Refresh"
                enabled: !gameModel.scanning && !installer.busy
                onClicked: {
                    root.optiRefreshNonce++
                    gameModel.refresh()
                }
            }
            RenoToolButton {
                id: settingsButton
                text: "⚙"
                font.pixelSize: 20
                Accessible.name: "Settings"
                ToolTip.visible: hovered
                ToolTip.text: "Settings"
                onClicked: settingsPopup.open()
            }
        }
    }

    Popup {
        id: settingsPopup
        width: 470
        height: Math.min(root.height - 80, 680)
        x: root.width - width - 16
        y: root.header ? root.header.height + 8 : 8
        modal: false
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 10
            color: root.panelBg
            border.width: 1
            border.color: root.dividerColor
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Label { text: "Settings"; font.pixelSize: 18; font.bold: true }
                Item { Layout.fillWidth: true }
                RenoToolButton { text: "×"; Accessible.name: "Close settings"; onClicked: settingsPopup.close() }
            }

            TabBar {
                id: settingsTabs
                Layout.fillWidth: true
                leftPadding: 10
                rightPadding: 10
                TabButton { text: "Appearance" }
                TabButton { text: "Integrations" }
                TabButton { text: "Library" }
                TabButton { text: "Storage & Safety" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsTabs.currentIndex

                ScrollView {
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        x: 16
                        width: Math.max(0, parent.width - 32)
                        spacing: 14

                        Label { text: "Theme"; font.bold: true; opacity: 0.82 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: "System follows Qt. Material 3 uses role-based colors and can load a generated palette. Noctalia follows a Noctalia v5 user-template output live. Reno119 Dark and AMOLED Black remain available as built-in themes."
                            opacity: 0.58
                            font.pixelSize: 11
                        }
                        RenoComboBox {
                            Layout.fillWidth: true
                            model: ["System", "Reno119 Dark", "AMOLED Black", "Material 3", "Noctalia"]
                            currentIndex: root.themeIndex(appSettings.themeMode)
                            onActivated: appSettings.setThemeMode(root.themeModeForIndex(currentIndex))
                        }
                        Label {
                            visible: root.systemTheme
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: "System mode uses the Qt theme exposed by your desktop/session. QT_QUICK_CONTROLS_STYLE remains available for choosing an installed Qt Quick Controls style."
                            opacity: 0.5
                            font.pixelSize: 10
                        }
                        ColumnLayout {
                            visible: root.materialTheme
                            Layout.fillWidth: true
                            spacing: 5
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: themeManager.materialStatus
                                color: themeManager.materialCustomReady ? root.successColor : root.mutedTextColor
                                font.pixelSize: 10
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Optional override: " + themeManager.materialThemePath + ". The file may contain a full or partial Material role map; missing roles fall back to Reno119's built-in Material 3 palette."
                                opacity: 0.5
                                font.pixelSize: 10
                            }
                            RenoButton { text: "Reload Material palette"; onClicked: themeManager.reloadMaterial() }
                        }
                        ColumnLayout {
                            visible: root.noctaliaTheme
                            Layout.fillWidth: true
                            spacing: 5
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: themeManager.noctaliaStatus
                                color: themeManager.noctaliaReady ? root.successColor : root.warningColor
                                font.pixelSize: 10
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Expected generated file: " + themeManager.noctaliaThemePath + ". Reno119 watches this file and applies palette changes live. The source archive includes data/noctalia/reno119-theme.json.template and a ready-to-copy TOML snippet."
                                opacity: 0.5
                                font.pixelSize: 10
                            }
                            RenoButton { text: "Reload Noctalia palette"; onClicked: themeManager.reloadNoctalia() }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: "Game covers" }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Show Steam portrait artwork in the game list and selected-game header."
                                    wrapMode: Text.Wrap
                                    opacity: 0.55
                                    font.pixelSize: 11
                                }
                            }
                            Switch { checked: appSettings.gameCoversEnabled; onToggled: appSettings.setGameCoversEnabled(checked) }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                ScrollView {
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        x: 16
                        width: Math.max(0, parent.width - 32)
                        spacing: 12

                        Label { text: "Update Center"; font.bold: true; opacity: 0.82 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            opacity: 0.58
                            font.pixelSize: 11
                            text: "Reno119 can check managed ReShade, RenoDX, and REFramework components in the background. Nothing is installed automatically; updates are applied only from Update Center."
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label { text: "Check for component updates after startup" }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    opacity: 0.5
                                    font.pixelSize: 10
                                    text: "The check is read-only, waits until the library and RenoDX catalog are ready, then repeats every six hours while Reno119 remains open."
                                }
                            }
                            Switch { checked: appSettings.updateChecksOnStartup; onToggled: appSettings.setUpdateChecksOnStartup(checked) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            RenoButton {
                                text: "Open Update Center"
                                onClicked: { settingsPopup.close(); updateCenterPopup.open(); root.resetUpdateCenterSelection(); installer.checkAllUpdates() }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }

                        Label { text: "ReShade Custom Build"; font.bold: true; opacity: 0.82 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            opacity: 0.58
                            font.pixelSize: 11
                            text: "The per-game selector can use Recommended, Latest, or this Custom source."
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Source"; opacity: 0.65 }
                            RenoComboBox {
                                id: customSourceBox
                                Layout.fillWidth: true
                                model: ["Version", "Custom URL", "Local file"]
                                currentIndex: appSettings.customReShadeSource === "url" ? 1 : (appSettings.customReShadeSource === "file" ? 2 : 0)
                                onActivated: appSettings.setCustomReShadeSource(currentIndex === 0 ? "version" : currentIndex === 1 ? "url" : "file")
                            }
                        }
                        TextField {
                            visible: appSettings.customReShadeSource === "version"
                            Layout.fillWidth: true
                            placeholderText: "e.g. 6.8.0"
                            text: appSettings.customReShadeVersion
                            onEditingFinished: appSettings.setCustomReShadeVersion(text)
                        }
                        TextField {
                            visible: appSettings.customReShadeSource === "url"
                            Layout.fillWidth: true
                            placeholderText: "https://..."
                            text: appSettings.customReShadeUrl
                            onEditingFinished: appSettings.setCustomReShadeUrl(text)
                        }
                        RowLayout {
                            visible: appSettings.customReShadeSource === "file"
                            Layout.fillWidth: true
                            TextField {
                                id: customReShadeFileField
                                Layout.fillWidth: true
                                placeholderText: "/path/to/ReShade_Setup_Addon.exe"
                                text: appSettings.customReShadeFile
                                onEditingFinished: appSettings.setCustomReShadeFile(text)
                            }
                            RenoButton { text: "Browse…"; onClicked: root.openFilePickerAt(customReShadeFileDialog, customReShadeFileField.text, "customReShade", "Choose custom ReShade installer") }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: appSettings.customReShadeSummary
                            opacity: appSettings.customReShadeConfigured ? 0.85 : 0.55
                            color: appSettings.customReShadeConfigured ? root.successColor : root.mutedTextColor
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                ScrollView {
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        x: 16
                        width: Math.max(0, parent.width - 32)
                        spacing: 12

                        Label { text: "Library sources"; font.bold: true; opacity: 0.82 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            opacity: 0.58
                            font.pixelSize: 11
                            text: "Choose which launcher libraries Reno119 scans. Custom programs are always kept. Disabling a source only removes it from Reno119's current library view; it never changes the launcher."
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Scan Steam"; Layout.fillWidth: true }
                            Switch { checked: gameModel.scanSteamEnabled; onToggled: gameModel.setScanSteamEnabled(checked) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Scan Heroic"; Layout.fillWidth: true }
                            Switch { checked: gameModel.scanHeroicEnabled; onToggled: gameModel.setScanHeroicEnabled(checked) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Scan Lutris"; Layout.fillWidth: true }
                            Switch { checked: gameModel.scanLutrisEnabled; onToggled: gameModel.setScanLutrisEnabled(checked) }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                        CollapsibleHeader {
                            titleText: "Advanced launcher paths"
                            summaryText: root.advancedLibrarySettingsExpanded ? "Custom Heroic/Lutris discovery roots" : "Automatic discovery is recommended for most systems"
                            expanded: root.advancedLibrarySettingsExpanded
                            onToggleRequested: root.advancedLibrarySettingsExpanded = !root.advancedLibrarySettingsExpanded
                        }
                        ColumnLayout {
                            visible: root.advancedLibrarySettingsExpanded
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                opacity: 0.55
                                font.pixelSize: 11
                                text: "Optional extra roots are scanned in addition to Reno119's normal native and Flatpak locations. Leave them empty to use automatic discovery only."
                            }
                            Label { text: "Heroic config root"; opacity: 0.65; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: heroicRootField
                                    Layout.fillWidth: true
                                    placeholderText: "e.g. /mnt/storage/heroic-config"
                                    text: gameModel.heroicRootOverride
                                    onEditingFinished: gameModel.setHeroicRootOverride(text)
                                }
                                RenoButton { text: "Browse…"; onClicked: root.openFolderPickerAt(heroicRootDialog, heroicRootField.text, "heroicRootOverride", "Choose additional Heroic config root") }
                                RenoButton { text: "Clear"; visible: heroicRootField.text.length > 0; onClicked: { heroicRootField.clear(); gameModel.setHeroicRootOverride("") } }
                            }
                            Label { text: "Lutris data root (contains pga.db)"; opacity: 0.65; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: lutrisDataRootField
                                    Layout.fillWidth: true
                                    placeholderText: "e.g. /mnt/storage/lutris"
                                    text: gameModel.lutrisDataRootOverride
                                    onEditingFinished: gameModel.setLutrisDataRootOverride(text)
                                }
                                RenoButton { text: "Browse…"; onClicked: root.openFolderPickerAt(lutrisDataRootDialog, lutrisDataRootField.text, "lutrisDataRootOverride", "Choose additional Lutris data root") }
                                RenoButton { text: "Clear"; visible: lutrisDataRootField.text.length > 0; onClicked: { lutrisDataRootField.clear(); gameModel.setLutrisDataRootOverride("") } }
                            }
                            Label { text: "Lutris config root (contains games/)"; opacity: 0.65; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: lutrisConfigRootField
                                    Layout.fillWidth: true
                                    placeholderText: "Optional; defaults to the data root for a custom location"
                                    text: gameModel.lutrisConfigRootOverride
                                    onEditingFinished: gameModel.setLutrisConfigRootOverride(text)
                                }
                                RenoButton { text: "Browse…"; onClicked: root.openFolderPickerAt(lutrisConfigRootDialog, lutrisConfigRootField.text, "lutrisConfigRootOverride", "Choose additional Lutris config root") }
                                RenoButton { text: "Clear"; visible: lutrisConfigRootField.text.length > 0; onClicked: { lutrisConfigRootField.clear(); gameModel.setLutrisConfigRootOverride("") } }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                        Label { text: "Favorites"; font.bold: true; opacity: 0.82 }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: "Keep favorites at the top" }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    opacity: 0.55
                                    font.pixelSize: 11
                                    text: "Pinned games stay ahead of non-favorites while keeping the selected sort order within each group."
                                }
                            }
                            Switch { checked: gameModel.favoritesFirst; onToggled: gameModel.setFavoritesFirst(checked) }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                ScrollView {
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        x: 16
                        width: Math.max(0, parent.width - 32)
                        spacing: 12

                        Label { text: "Safety"; font.bold: true; opacity: 0.82 }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: "Backup before install or uninstall" }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    opacity: 0.55
                                    font.pixelSize: 11
                                    text: "Create a per-game backup of managed ReShade/RenoDX/REFramework files before changing them. OptiScaler integration transactions always create their own rollback snapshot."
                                }
                            }
                            Switch { checked: appSettings.backupBeforeChanges; onToggled: appSettings.setBackupBeforeChanges(checked) }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                        CollapsibleHeader {
                            titleText: "Advanced storage & cache"
                            summaryText: root.advancedStorageSettingsExpanded ? "Download and artwork cache maintenance" : "Normally no action is required"
                            expanded: root.advancedStorageSettingsExpanded
                            onToggleRequested: root.advancedStorageSettingsExpanded = !root.advancedStorageSettingsExpanded
                        }
                        ColumnLayout {
                            visible: root.advancedStorageSettingsExpanded
                            Layout.fillWidth: true
                            spacing: 8
                            Label { Layout.fillWidth: true; text: "Downloads: " + installer.cachePath(); wrapMode: Text.Wrap; opacity: 0.58; font.pixelSize: 11 }
                            Label {
                                text: {
                                    installer.cacheRevision
                                    return "Download cache size: " + installer.cacheSizeString()
                                }
                                opacity: 0.78
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Artwork: " + coverService.cachePath() + " · " + coverService.bannerCachePath()
                                wrapMode: Text.Wrap
                                opacity: 0.55
                                font.pixelSize: 11
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                RenoButton { text: "Clear downloads"; onClicked: installer.clearDownloadCache() }
                                RenoButton { text: "Refresh all artwork"; onClicked: coverService.refreshAll(gameModel.steamAppIds()) }
                                RenoButton { text: "Clear artwork"; onClicked: coverService.clearCache() }
                            }
                            Label {
                                visible: (coverService.lastStatus || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: coverService.lastStatus || ""
                                opacity: 0.58
                                font.pixelSize: 10
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                        Label { text: "Metadata backup"; font.bold: true; opacity: 0.82 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: "Export or import a portable Reno119 backup. This includes appearance, library preferences, favorites, nicknames, hidden state, artwork/executable overrides, notes, verification state, alternate-install links, Custom ReShade, update-check preferences/skipped targets, custom programs, and workspace/UI choices. Generated Material/Noctalia palette files, game files, launcher configuration, recovery snapshots, DLL ownership files, and caches are not included."
                            opacity: 0.55
                            font.pixelSize: 11
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            RenoButton { text: "Export backup…"; onClicked: root.openSavePicker(settingsExportDialog, "settingsExport", "Export Reno119 backup", "reno119-backup.json", "") }
                            RenoButton { text: "Import backup…"; onClicked: root.openFilePickerAt(settingsImportDialog, "", "settingsImport", "Import Reno119 backup") }
                        }
                        Label {
                            visible: (appSettings.settingsTransferStatus || "").length > 0
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: appSettings.settingsTransferStatus || ""
                            opacity: 0.65
                            font.pixelSize: 10
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    Popup {
        id: updateCenterPopup
        width: Math.min(940, root.width - 56)
        height: Math.min(760, root.height - 76)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: root.resetUpdateCenterSelection()

        background: Rectangle {
            radius: 10
            color: root.panelBg
            border.width: 1
            border.color: root.dividerColor
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Label { text: "Update Center"; font.pixelSize: 20; font.bold: true }
                StatusBadge {
                    visible: root.updateCenterActionableCount > 0
                    text: root.updateCenterCountText()
                    accent: root.badgeWarningColor
                    tooltip: "Compatible, unskipped Reno119-managed component updates in the current library view"
                }
                Item { Layout.fillWidth: true }
                RenoToolButton { text: "×"; Accessible.name: "Close Update Center"; onClicked: updateCenterPopup.close() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 10
                Layout.bottomMargin: 10
                spacing: 8
                RenoButton {
                    text: installer.bulkUpdateBusy ? "Checking…" : "Check now"
                    enabled: !installer.bulkUpdateBusy && !installer.busy && !installer.updateQueueBusy && !gameModel.scanning
                    onClicked: installer.checkAllUpdates(true)
                }
                RenoButton {
                    text: "Update selected"
                    enabled: root.selectedUpdateRows().length > 0 && !installer.busy && !installer.updateQueueBusy && !installer.bulkUpdateBusy
                    onClicked: root.previewUpdates(root.selectedUpdateRows(), false)
                }
                RenoButton {
                    text: "Update all compatible"
                    enabled: root.updateCenterActionableCount > 0 && !installer.busy && !installer.updateQueueBusy && !installer.bulkUpdateBusy
                    onClicked: root.previewUpdates((root.updateCenterItems || []).map(function(g) { return g.row }), false)
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: installer.updateQueueBusy
                          ? "Updating · " + installer.updateQueueRemaining + " queued"
                          : (installer.bulkUpdateBusy
                             ? "Checking " + installer.bulkUpdateChecked + "/" + installer.bulkUpdateTotal
                             : "Updates always create a rollback snapshot")
                    opacity: 0.65
                    font.pixelSize: 11
                }
            }

            Rectangle {
                visible: installer.bulkUpdateBusy || installer.updateQueueBusy
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 10
                implicitHeight: updateLoadingColumn.implicitHeight + 20
                radius: 8
                color: root.controlBg
                border.width: 1
                border.color: root.dividerColor
                ColumnLayout {
                    id: updateLoadingColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        BusyIndicator {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            running: updateCenterPopup.visible && (installer.bulkUpdateBusy || installer.updateQueueBusy)
                            Accessible.name: installer.bulkUpdateBusy ? "Checking for updates" : "Applying updates"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label {
                                Layout.fillWidth: true
                                font.bold: true
                                wrapMode: Text.Wrap
                                text: installer.bulkUpdateBusy ? "Checking for updates…" : "Applying updates…"
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                font.pixelSize: 11
                                opacity: 0.7
                                text: installer.bulkUpdateBusy
                                      ? (installer.bulkUpdateChecked === 0
                                         ? "Fetching release information. This may take a little while…"
                                         : "Checked " + installer.bulkUpdateChecked + " of " + installer.bulkUpdateTotal + " games / programs…")
                                      : installer.updateQueueRemaining + " components waiting · Results appear below as updates finish."
                            }
                        }
                    }
                    RenoProgressBar {
                        Layout.fillWidth: true
                        indeterminate: true
                        visible: updateCenterPopup.visible
                        trackColor: root.dividerColor
                        fillColor: root.buttonHighlight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                RenoComboBox {
                    Layout.preferredWidth: 190
                    model: ["All installations", "Updates available", "Up to date", "External installs", "Failed"]
                    currentIndex: root.updateCenterFilter
                    onActivated: root.updateCenterFilter = currentIndex
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: installer.lastUpdateCheck.length > 0
                          ? "Last check completed: " + new Date(installer.lastUpdateCheck).toLocaleString(Qt.locale(), Locale.ShortFormat)
                          : "Not checked this session"
                    opacity: 0.65
                    font.pixelSize: 11
                }
                RenoButton {
                    text: "Retry failed"
                    enabled: root.failedUpdateResults.length > 0 && !installer.busy && !installer.bulkUpdateBusy && !installer.updateQueueBusy
                             && root.updatePreviewItems(root.failedUpdateResults.map(function(r) { return r.appId }), true).length > 0
                    onClicked: root.previewUpdates((root.updateCenterItems || []).filter(function(g) {
                        return root.failedUpdateResults.some(function(r) { return r.appId === g.appId })
                    }).map(function(g) { return g.row }), true)
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                font.pixelSize: 10
                opacity: 0.65
                wrapMode: Text.Wrap
                text: "Cached information appears immediately; sources refresh after 30 minutes. Check now forces a refresh. Times below are UTC.\n"
                      + "RenoDX: " + (renoDxCatalog.renoDxCatalogReady ? (renoDxCatalog.catalogStale ? "stale — last known result" : "cached") : "unavailable")
                      + (renoDxCatalog.catalogCheckedUtc.length > 0 ? " · " + renoDxCatalog.catalogCheckedUtc : "")
                      + "\n" + installer.updateCacheStatus
            }

            Label {
                visible: (installer.bulkUpdateSummary || "").length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                wrapMode: Text.Wrap
                text: installer.bulkUpdateSummary || ""
                color: installer.bulkUpdateAvailableComponents > 0 ? root.warningColor : root.mutedTextColor
                opacity: 0.82
                font.pixelSize: 11
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Label {
                    visible: (installer.updateResults || []).length > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 8
                    font.bold: true
                    text: "Latest update batch · " + (installer.updateResults || []).filter(function(r) { return r.outcome === "Succeeded" }).length
                          + " succeeded · " + root.failedUpdateResults.length + " failed · "
                          + (installer.updateResults || []).filter(function(r) { return r.outcome === "Skipped" }).length + " skipped"
                    wrapMode: Text.Wrap
                }
                Repeater {
                    model: (installer.updateResults || []).filter(function(r) { return root.updateCenterFilter !== 4 || r.outcome === "Failed" })
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        text: modelData.name + " · " + modelData.component + " — " + modelData.outcome + "\n" + modelData.message
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: 11
                        color: modelData.outcome === "Failed" ? root.warningColor : root.mutedTextColor
                    }
                }

                Label {
                    visible: root.filteredUpdateItems.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 24
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                    wrapMode: Text.Wrap
                    text: installer.bulkUpdateBusy
                          ? "Checking installed components…"
                          : "No installations match this filter in the current library view."
                    opacity: 0.6
                }

                ListView {
                    id: updateCenterList
                    visible: root.filteredUpdateItems.length > 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    topMargin: 8
                    bottomMargin: 8
                    model: root.filteredUpdateItems
                    reuseItems: true
                    cacheBuffer: 240
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Rectangle {
                            id: updateGameCard
                            required property var modelData
                            property var gameItem: modelData
                            x: 12
                            width: Math.max(0, updateCenterList.width - 24)
                            height: updateGameColumn.implicitHeight + 20
                            radius: 8
                            color: root.controlBg
                            border.width: 1
                            border.color: root.dividerColor

                            ColumnLayout {
                                id: updateGameColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 7

                                RowLayout {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        visible: (updateGameCard.gameItem.actionableUpdates || 0) > 0
                                        checked: root.updateCenterSelection[updateGameCard.gameItem.appId] === true
                                        onToggled: root.setUpdateCenterSelected(updateGameCard.gameItem.appId, checked)
                                        Accessible.name: "Select " + (updateGameCard.gameItem.name || "game") + " for update"
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Label {
                                            Layout.fillWidth: true
                                            text: updateGameCard.gameItem.name || "Unknown game"
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            text: (updateGameCard.gameItem.source || "Unknown source") +
                                                  ((updateGameCard.gameItem.checked === true) ? " · checked" : " · not checked yet")
                                            opacity: 0.52
                                            font.pixelSize: 10
                                        }
                                    }
                                    StatusBadge {
                                        visible: (updateGameCard.gameItem.actionableUpdates || 0) > 0
                                        text: (updateGameCard.gameItem.actionableUpdates || 0) + " available"
                                        accent: root.badgeWarningColor
                                    }
                                    RenoButton {
                                        text: "Update game"
                                        enabled: (updateGameCard.gameItem.actionableUpdates || 0) > 0 && !installer.busy && !installer.updateQueueBusy && !installer.bulkUpdateBusy
                                        onClicked: root.previewUpdates([updateGameCard.gameItem.row], false)
                                    }
                                    RenoButton {
                                        text: "History"
                                        onClicked: updateHistoryDialog.openFor(updateGameCard.gameItem.row, updateGameCard.gameItem.name || "Game")
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: (updateGameCard.gameItem.supportNotice || "").length > 0
                                    text: updateGameCard.gameItem.supportNotice || ""
                                    color: root.infoColor
                                    wrapMode: Text.Wrap
                                }

                                Repeater {
                                    model: updateGameCard.gameItem.components || []
                                    delegate: Rectangle {
                                        id: updateComponentRow
                                        required property var modelData
                                        property var componentItem: modelData
                                        Layout.fillWidth: true
                                        implicitHeight: componentColumn.implicitHeight + 14
                                        radius: 6
                                        color: root.panelBg
                                        border.width: 1
                                        border.color: root.dividerColor

                                        ColumnLayout {
                                            id: componentColumn
                                            anchors.fill: parent
                                            anchors.margins: 7
                                            spacing: 4
                                            RowLayout {
                                                Layout.fillWidth: true
                                                Label { text: updateComponentRow.componentItem.name || "Component"; font.bold: true }
                                                StatusBadge {
                                                    text: updateComponentRow.componentItem.managed === true ? "Managed" : "External / advisory"
                                                    accent: updateComponentRow.componentItem.managed === true ? root.badgeSuccessColor : root.badgeMutedColor
                                                }
                                                StatusBadge {
                                                    visible: updateComponentRow.componentItem.stale === true
                                                    text: "Stale / unconfirmed"
                                                    accent: root.badgeWarningColor
                                                }
                                                StatusBadge {
                                                    visible: updateComponentRow.componentItem.skipped === true
                                                    text: "Skipped"
                                                    accent: root.badgeWarningColor
                                                }
                                                StatusBadge {
                                                    visible: updateComponentRow.componentItem.updateAvailable === true
                                                    text: "Update available"
                                                    accent: root.badgeWarningColor
                                                }
                                                Item { Layout.fillWidth: true }
                                                RenoButton {
                                                    visible: (updateComponentRow.componentItem.releaseUrl || "").length > 0
                                                    text: updateComponentRow.componentItem.linkLabel || "Release notes"
                                                    onClicked: Qt.openUrlExternally(updateComponentRow.componentItem.releaseUrl)
                                                }
                                                RenoButton {
                                                    visible: updateComponentRow.componentItem.updateDetected === true && (updateComponentRow.componentItem.available || "").length > 0
                                                    text: updateComponentRow.componentItem.skipped === true ? "Unskip" : "Skip this version"
                                                    enabled: !installer.busy && !installer.bulkUpdateBusy && !installer.updateQueueBusy
                                                    onClicked: {
                                                        if (updateComponentRow.componentItem.skipped === true)
                                                            installer.clearSkippedUpdate(updateGameCard.gameItem.row, updateComponentRow.componentItem.id)
                                                        else
                                                            installer.skipUpdate(updateGameCard.gameItem.row, updateComponentRow.componentItem.id)
                                                    }
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                wrapMode: Text.Wrap
                                                text: "Installed: " + ((updateComponentRow.componentItem.installed || "").length > 0 ? updateComponentRow.componentItem.installed : "unknown") +
                                                      "    Available: " + ((updateComponentRow.componentItem.available || "").length > 0 ? updateComponentRow.componentItem.available : "unknown")
                                                opacity: 0.78
                                                font.pixelSize: 11
                                            }
                                            Label {
                                                visible: (updateComponentRow.componentItem.publishedUtc || "").length > 0
                                                Layout.fillWidth: true
                                                text: "Published: " + updateComponentRow.componentItem.publishedUtc
                                                opacity: 0.48
                                                font.pixelSize: 10
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                wrapMode: Text.Wrap
                                                text: updateComponentRow.componentItem.summary || ""
                                                opacity: 0.58
                                                font.pixelSize: 10
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                visible: updateComponentRow.componentItem.id === "renodx" && (updateComponentRow.componentItem.matchTitle || "").length > 0
                                                wrapMode: Text.Wrap
                                                text: "Catalog match: " + (updateComponentRow.componentItem.matchTitle || "") + " · " + (updateComponentRow.componentItem.matchMethod || "")
                                                color: updateComponentRow.componentItem.matchRequiresConfirmation === true ? root.warningColor : root.infoColor
                                                opacity: 0.78
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    RenoUpdatePreviewDialog {
        id: updatePreviewDialog
        host: root
        installerBackend: installer
        gameModelBackend: gameModel
    }

    RenoUpdateHistoryDialog {
        id: updateHistoryDialog
        host: root
        installerBackend: installer
        onRollbackRequested: function(row, path, label) {
            root.pendingBackupRestoreRow = row
            root.pendingBackupRestorePath = path
            root.pendingBackupRestoreLabel = label
            root.pendingBackupRestoreKind = "update"
            restoreBackupDialog.open()
        }
    }

    Connections {
        target: installer
        function onBulkUpdateChanged() {
            if (updateCenterPopup.visible && !installer.bulkUpdateBusy) {
                root.updateCenterNonce++
                root.resetUpdateCenterSelection()
            }
        }
        function onUpdateQueueChanged() {
            if (updateCenterPopup.visible)
                root.updateCenterNonce++
        }
        function onBusyChanged() {
            if (!root.recommendedSetupActive || !root.recommendedSetupWaitingForBusy || installer.busy)
                return
            root.recommendedSetupWaitingForBusy = false
            const component = root.recommendedSetupCurrentComponent || ""
            const result = component.length > 0 ? (installer.inlineStatus(root.recommendedSetupRow, component) || ({})) : ({})
            if ((result.level || "") === "error") {
                root.stopRecommendedSetup("Recommended setup stopped during " + (root.recommendedSetupCurrent || "an install") + ": " + (result.text || installer.status || "unknown error"))
                return
            }
            root.recommendedSetupCompleted++
            root.recommendedSetupCurrentComponent = ""
            root.verificationNonce++
            root.optiRefreshNonce++
            Qt.callLater(root.runNextRecommendedSetupStep)
        }
    }

    RenoRecoveryDialog {
        id: recoveryDialog
        host: root
        installerBackend: installer
        onRestoreCompleted: root.verificationNonce++
    }

    Dialog {
        id: libraryDiagnosticsPopup
        property int gameRow: -1
        property string gameAppId: ""
        property var game: ({})
        property var info: ({})
        modal: true
        title: "Library diagnostics"
        width: Math.min(900, root.width - 40)
        height: Math.min(740, root.height - 40)
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        standardButtons: Dialog.NoButton

        function findRow(appId) {
            if (!appId) return -1
            for (let row = 0; row < gameModel.count; ++row) {
                if ((gameModel.gameAt(row).appId || "") === appId) return row
            }
            return -1
        }

        function refresh() {
            if (gameAppId) gameRow = findRow(gameAppId)
            game = gameRow >= 0 ? gameModel.gameAt(gameRow) : ({})
            info = gameRow >= 0 ? gameModel.diagnosticInfo(gameRow) : ({})
        }

        function openFor(row) {
            gameRow = row
            gameAppId = row >= 0 ? (gameModel.gameAt(row).appId || "") : ""
            refresh()
            open()
        }

        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: (libraryDiagnosticsPopup.game.name || "Unknown game") + " · " + (libraryDiagnosticsPopup.game.source || "Unknown source")
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: "[" + (libraryDiagnosticsPopup.info.level || "unknown").toUpperCase() + "] " + (libraryDiagnosticsPopup.info.summary || "No diagnostic result")
                color: (libraryDiagnosticsPopup.info.level || "") === "error" ? root.warningColor :
                       (libraryDiagnosticsPopup.info.level || "") === "warning" ? root.warningColor : root.successColor
                wrapMode: Text.Wrap
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: "This view shows what Reno119 imported from the launcher, what it resolved locally, and which proxy/loader DLLs it can or cannot identify. Launcher configuration is read-only."
                wrapMode: Text.Wrap
                opacity: 0.62
                font.pixelSize: 11
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    Label { text: "Resolved paths"; font.bold: true }
                    Label { Layout.fillWidth: true; text: "Install: " + (libraryDiagnosticsPopup.info.installPath || "(unresolved)"); wrapMode: Text.Wrap; opacity: 0.76 }
                    Label { Layout.fillWidth: true; text: "Executable: " + (libraryDiagnosticsPopup.info.executable || "(unresolved)") + (libraryDiagnosticsPopup.info.executableOverridden === true ? " [override]" : " [auto]"); wrapMode: Text.Wrap; opacity: 0.76 }
                    Label { Layout.fillWidth: true; text: "Auto EXE: " + (libraryDiagnosticsPopup.info.detectedExecutable || "(unresolved)"); wrapMode: Text.Wrap; opacity: 0.62; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; text: "Prefix: " + (libraryDiagnosticsPopup.info.prefix || "(unresolved)") + (libraryDiagnosticsPopup.info.prefixOverridden === true ? " [override]" : " [auto]"); wrapMode: Text.Wrap; opacity: 0.76 }
                    Label { Layout.fillWidth: true; text: "Auto prefix: " + (libraryDiagnosticsPopup.info.detectedPrefix || "(unresolved)"); wrapMode: Text.Wrap; opacity: 0.62; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; text: "Graphics API: " + (libraryDiagnosticsPopup.info.graphicsApi || "Unknown") + (libraryDiagnosticsPopup.info.graphicsApiOverridden === true ? " [override]" : " [auto]"); wrapMode: Text.Wrap; opacity: 0.76 }
                    Label { Layout.fillWidth: true; text: "Auto graphics API: " + (libraryDiagnosticsPopup.info.detectedGraphicsApi || "Unknown"); wrapMode: Text.Wrap; opacity: 0.62; font.pixelSize: 11 }
                    Label { visible: (libraryDiagnosticsPopup.info.graphicsApiEvidence || []).length > 0; text: "Graphics detection evidence"; font.bold: true; opacity: 0.8 }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.graphicsApiEvidence || []
                        delegate: Label { required property var modelData; Layout.fillWidth: true; text: "• " + modelData; wrapMode: Text.Wrap; opacity: 0.62; font.pixelSize: 11 }
                    }

                    Label { visible: (libraryDiagnosticsPopup.info.problems || []).length > 0; text: "Problems"; font.bold: true; color: root.warningColor }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.problems || []
                        delegate: Label { required property var modelData; Layout.fillWidth: true; text: "• " + modelData; wrapMode: Text.Wrap; color: root.warningColor; opacity: 0.92 }
                    }
                    Label { visible: (libraryDiagnosticsPopup.info.warnings || []).length > 0; text: "Warnings"; font.bold: true; color: root.warningColor }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.warnings || []
                        delegate: Label { required property var modelData; Layout.fillWidth: true; text: "• " + modelData; wrapMode: Text.Wrap; opacity: 0.78 }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                    Label { text: "Launcher/import metadata"; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        visible: (libraryDiagnosticsPopup.info.importNotes || []).length === 0
                        text: "No importer notes were recorded for this entry."
                        opacity: 0.52
                        font.pixelSize: 11
                    }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.importNotes || []
                        delegate: Label { required property var modelData; Layout.fillWidth: true; text: "• " + modelData; wrapMode: Text.Wrap; opacity: 0.72; font.pixelSize: 11 }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: metadataText.implicitHeight + 16
                        radius: 6
                        color: root.alternateBg
                        border.width: 1
                        border.color: root.dividerColor
                        TextEdit {
                            id: metadataText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            color: root.textColor
                            selectionColor: root.buttonHighlight
                            font.family: "monospace"
                            font.pixelSize: 11
                            text: root.diagnosticsMap(libraryDiagnosticsPopup.info.sourceMetadata || ({}))
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                    Label { text: "Detection history"; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        visible: (libraryDiagnosticsPopup.info.detectionHistory || []).length === 0
                        text: "No automatic executable, prefix, or graphics API changes have been recorded."
                        opacity: 0.52
                        font.pixelSize: 11
                    }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.detectionHistory || []
                        delegate: Label {
                            required property var modelData
                            Layout.fillWidth: true
                            text: "• " + modelData
                            wrapMode: Text.Wrap
                            opacity: 0.72
                            font.pixelSize: 11
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                    Label { text: "Proxy / loader ownership"; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        visible: (libraryDiagnosticsPopup.info.proxyScan || []).length === 0
                        text: "No common proxy/loader DLLs were found beside the resolved executable."
                        opacity: 0.52
                        font.pixelSize: 11
                    }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.proxyScan || []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: proxyRow.implicitHeight + 12
                            radius: 6
                            color: root.alternateBg
                            border.width: 1
                            border.color: (modelData.severity || "") === "warning" ? root.warningColor : root.dividerColor
                            RowLayout {
                                id: proxyRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 6
                                spacing: 8
                                Label { text: modelData.file || "?"; font.bold: true; Layout.preferredWidth: 120; elide: Text.ElideRight }
                                Label { Layout.fillWidth: true; text: modelData.classification || "Unknown"; elide: Text.ElideRight; opacity: 0.76 }
                                Label { text: modelData.managed === true ? "Managed" : "External"; opacity: 0.58; font.pixelSize: 10 }
                            }
                        }
                    }
                    Repeater {
                        model: libraryDiagnosticsPopup.info.proxyWarnings || []
                        delegate: Label { required property var modelData; Layout.fillWidth: true; text: "⚠ " + modelData; wrapMode: Text.Wrap; color: root.warningColor; font.pixelSize: 11 }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                RenoButton { text: "Refresh view"; onClicked: libraryDiagnosticsPopup.refresh() }
                RenoButton {
                    text: "Clear detection history"
                    visible: (libraryDiagnosticsPopup.info.detectionHistory || []).length > 0
                    enabled: libraryDiagnosticsPopup.gameRow >= 0
                    onClicked: {
                        if (gameModel.clearDetectionHistory(libraryDiagnosticsPopup.gameRow))
                            libraryDiagnosticsPopup.refresh()
                    }
                }
                RenoButton {
                    text: "Rescan this game"
                    enabled: libraryDiagnosticsPopup.gameRow >= 0
                    onClicked: {
                        if (gameModel.rescanGame(libraryDiagnosticsPopup.gameRow)) {
                            libraryDiagnosticsPopup.refresh()
                            Qt.callLater(root.loadDetectionOverrides)
                        }
                    }
                }
                RenoButton { text: "Copy troubleshooting report"; enabled: libraryDiagnosticsPopup.gameRow >= 0; onClicked: installer.copyText(root.diagnosticsReport()) }
                Item { Layout.fillWidth: true }
                RenoButton { text: "Close"; onClicked: libraryDiagnosticsPopup.close() }
            }
        }
    }

    RenoTroubleshootingDialog {
        id: troubleshootingDialog
        host: root
        installerBackend: installer
    }

    RenoTweakPreviewDialog {
        id: tweakPreviewDialog
        host: root
        installerBackend: installer
    }

    RenoConfirmDialog {
        id: settingsImportConfirm
        host: root
        title: "Import Reno119 backup?"
        acceptText: "Import"
        rejectText: "Cancel"
        dialogWidth: 560
        message: "This replaces Reno119's portable metadata/settings with the selected backup, including notes, verification records and alternate-install links. Game files, launcher configuration, installed DLL ownership files and recovery snapshots are not changed. The library will be refreshed afterward."
        onAccepted: {
            if (appSettings.importConfiguration(root.pendingSettingsImportUrl)) {
                root.optiRefreshNonce++
                root.backupRefreshNonce++
                gameModel.reloadConfiguration()
                Qt.callLater(root.loadGameNotes)
            }
            root.pendingSettingsImportUrl = ""
        }
        onRejected: root.pendingSettingsImportUrl = ""
    }

    RenoCustomProgramPopup {
        id: customProgramPopup
        host: root
        libraryModel: gameModel
        onBrowseExecutable: function(currentPath) {
            root.openFilePickerAt(customProgramExeDialog, currentPath, "customProgramExe", "Choose Windows executable")
        }
        onBrowsePrefix: function(currentPath) {
            root.openFolderPickerAt(customProgramPrefixDialog, currentPath, "customProgramPrefix", "Choose Wine / Proton prefix")
        }
        onBrowseArtwork: function(currentPath) {
            root.openFilePickerAt(customProgramArtworkDialog, currentPath, "customProgramArtwork", "Choose custom artwork")
        }
    }

    Menu {
        id: gameContextMenu
        property int gameRow: -1
        property var game: {
            gameModel.revision
            return gameRow >= 0 ? gameModel.gameAt(gameRow) : ({})
        }

        MenuItem {
            text: gameContextMenu.game.favorite === true ? "★ Remove from favorites" : "☆ Add to favorites"
            onTriggered: {
                const id = gameContextMenu.game.appId || ""
                if (gameContextMenu.gameRow >= 0 && gameModel.setGameFavorite(gameContextMenu.gameRow, gameContextMenu.game.favorite !== true))
                    nicknamePopup.reselect(id)
            }
        }
        MenuItem {
            text: (gameContextMenu.game.nickname || "").length > 0 ? "Change nickname…" : "Set nickname…"
            onTriggered: if (gameContextMenu.gameRow >= 0) nicknamePopup.openFor(gameContextMenu.gameRow)
        }
        MenuItem {
            visible: (gameContextMenu.game.nickname || "").length > 0
            text: "Clear nickname"
            onTriggered: {
                const id = gameContextMenu.game.appId || ""
                if (gameContextMenu.gameRow >= 0 && gameModel.clearGameNickname(gameContextMenu.gameRow))
                    nicknamePopup.reselect(id)
            }
        }
        MenuItem {
            text: "Manage alternate installs…"
            onTriggered: if (gameContextMenu.gameRow >= 0) duplicatePopup.openFor(gameContextMenu.gameRow)
        }
        MenuSeparator {}
        MenuItem {
            text: gameContextMenu.game.artworkOverridden === true ? "Change custom artwork…" : "Set custom artwork…"
            onTriggered: {
                if (gameContextMenu.gameRow < 0) return
                root.pendingArtworkRow = gameContextMenu.gameRow
                root.openFilePickerAt(customArtworkQuickDialog,
                                      gameContextMenu.game.artworkOverridden === true ? (gameContextMenu.game.coverArtPath || "") : "",
                                      "customArtworkQuick",
                                      "Choose custom artwork")
            }
        }
        MenuItem {
            visible: gameContextMenu.game.artworkOverridden === true
            text: "Remove custom artwork"
            onTriggered: if (gameContextMenu.gameRow >= 0) gameModel.clearArtworkOverride(gameContextMenu.gameRow)
        }
        MenuItem {
            text: gameContextMenu.game.hidden === true ? "Unhide game" : "Hide game"
            onTriggered: if (gameContextMenu.gameRow >= 0) gameModel.setGameHidden(gameContextMenu.gameRow, gameContextMenu.game.hidden !== true)
        }
        MenuSeparator {}
        MenuItem {
            text: "Open executable folder"
            enabled: (gameContextMenu.game.exePath || "").length > 0
            onTriggered: installer.openGameFolder(gameContextMenu.gameRow)
        }
        MenuItem {
            text: "Open Wine / Proton prefix"
            enabled: (gameContextMenu.game.protonPrefix || "").length > 0
            onTriggered: installer.openPrefixFolder(gameContextMenu.gameRow)
        }
        MenuItem {
            text: "Open Engine.ini folder"
            onTriggered: installer.openRenoDxEngineIniFolder(gameContextMenu.gameRow)
        }
        MenuItem {
            text: "Library diagnostics…"
            onTriggered: if (gameContextMenu.gameRow >= 0) libraryDiagnosticsPopup.openFor(gameContextMenu.gameRow)
        }
        MenuItem {
            visible: (gameContextMenu.game.detectionChangeCount || 0) > 0
            text: "Clear detection history"
            onTriggered: if (gameContextMenu.gameRow >= 0) gameModel.clearDetectionHistory(gameContextMenu.gameRow)
        }
        MenuSeparator {}
        MenuItem {
            visible: gameContextMenu.game.customProgram === true
            text: "Edit custom entry…"
            onTriggered: customProgramPopup.openEdit(gameContextMenu.gameRow)
        }
        MenuItem { text: "Refresh library"; onTriggered: gameModel.refresh() }
    }

    RenoNicknamePopup {
        id: nicknamePopup
        host: root
        libraryModel: gameModel
        listView: gameList
    }

    RenoDuplicatePopup {
        id: duplicatePopup
        host: root
        libraryModel: gameModel
        nicknameController: nicknamePopup
    }

    RenoConfirmDialog {
        id: removeCustomDialog
        host: root
        title: "Remove custom program?"
        acceptText: "Yes"
        rejectText: "No"
        dialogWidth: 460
        message: "This removes only the Reno119 entry. It does not delete the program or any ReShade/RenoDX files already installed beside it."
        onAccepted: gameModel.removeCustomProgram(root.selectedRow)
    }

    RenoPreviewConfirmDialog {
        id: externalReShadeOverwriteDialog
        host: root
        title: "Install over external ReShade?"
        acceptText: "Install anyway"
        rejectText: "Cancel"
        dialogWidth: 620
        previewHeight: 440
        previewText: installer.externalReShadeOverwritePreview(root.selectedRow)
        onAccepted: installer.installReShadeOverExternal(root.selectedRow,
                                                         reshadeBuildChoice.currentIndex === 0 ? "recommended"
                                                                                              : reshadeBuildChoice.currentIndex === 1 ? "latest"
                                                                                                                                     : "custom")
    }

    RenoPreviewConfirmDialog {
        id: externalReShade64OverwriteDialog
        host: root
        title: "Install over external ReShade64?"
        acceptText: "Install anyway"
        rejectText: "Cancel"
        dialogWidth: 620
        previewHeight: 440
        previewText: installer.externalReShade64OverwritePreview(root.selectedRow)
        onAccepted: installer.installReShade64OverExternal(root.selectedRow,
                                                           reshadeBuildChoice.currentIndex === 0 ? "recommended"
                                                                                                : reshadeBuildChoice.currentIndex === 1 ? "latest"
                                                                                                                                       : "custom")
    }

    RenoPreviewConfirmDialog {
        id: externalReFrameworkOverwriteDialog
        host: root
        title: "Install over external REFramework?"
        acceptText: "Install anyway"
        rejectText: "Cancel"
        dialogWidth: 620
        previewHeight: 420
        previewText: installer.externalReFrameworkOverwritePreview(root.selectedRow)
        onAccepted: installer.installReFrameworkOverExternal(root.selectedRow)
    }

    RenoDxMatchDialog {
        id: nonExactRenoDxMatchDialog
        host: root
        onAccepted: {
            if (externalOverwrite) installer.installRenoDxOverExternalConfirmed(gameRow, matchUrl)
            else installer.installRenoDxConfirmed(gameRow, matchUrl)
        }
    }

    RenoOverlayHotkeyDialog {
        id: overlayHotkeyDialog
        host: root
        hotkeyOptions: root.hotkeyOptions
        onAccepted: {
            const item = selectedItem
            if (!item) return
            if (target === "opti") {
                if (optiIntegration.setOverlayHotkey(root.selectedRow, item.code))
                    root.optiRefreshNonce++
            } else {
                if (installer.setReFrameworkHotkey(root.selectedRow, item.code))
                    root.reframeworkRefreshNonce++
            }
        }
    }

    RenoConfirmDialog {
        id: restoreBackupDialog
        host: root
        title: "Restore this backup?"
        acceptText: "Restore"
        rejectText: "Cancel"
        dialogWidth: 560
        message: "Restore “" + root.pendingBackupRestoreLabel + "”?\n\n" +
                 (root.pendingBackupRestoreKind === "integration"
                  ? "Reno119 will restore the selected OptiScaler/ReShade integration transaction. Independent ReShade64 installs and unrelated game/mod files are left untouched."
                  : root.pendingBackupRestoreKind === "update"
                    ? "Reno119 will roll this game back to the pre-update snapshot and then refresh its update status. Only Reno119-managed files captured by the snapshot are restored."
                    : "Reno119 will restore the files captured at that point and remove only newer files that are currently recorded as Reno119-managed but were not present in the selected backup. Unrelated game/mod files are left untouched.")
        onAccepted: {
            if (root.pendingBackupRestoreRow >= 0 && root.pendingBackupRestorePath.length > 0) {
                const restored = root.pendingBackupRestoreKind === "integration"
                    ? optiIntegration.restorePoint(root.pendingBackupRestoreRow, root.pendingBackupRestorePath)
                    : root.pendingBackupRestoreKind === "update"
                      ? installer.rollbackUpdate(root.pendingBackupRestoreRow, root.pendingBackupRestorePath)
                      : installer.restoreBackup(root.pendingBackupRestoreRow, root.pendingBackupRestorePath)
                if (restored) {
                    root.backupRefreshNonce++
                    root.optiRefreshNonce++
                    Qt.callLater(root.loadGameChoices)
                }
            }
            root.pendingBackupRestoreRow = -1
            root.pendingBackupRestorePath = ""
            root.pendingBackupRestoreLabel = ""
            root.pendingBackupRestoreKind = "component"
        }
        onRejected: {
            root.pendingBackupRestoreRow = -1
            root.pendingBackupRestorePath = ""
            root.pendingBackupRestoreLabel = ""
            root.pendingBackupRestoreKind = "component"
        }
    }

    Connections {
        target: gameModel
        function onCountChanged() {
            if (gameModel.count > 0 && gameList.currentIndex < 0)
                gameList.currentIndex = 0
            if (gameModel.count === 0)
                gameList.currentIndex = -1
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 12

        Rectangle {
            SplitView.preferredWidth: 430
            SplitView.minimumWidth: 430
            color: root.panelBg
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: gameModel.scanning
                          ? (gameModel.cachedSnapshot
                             ? gameModel.totalCount + " games / programs · Refreshing…"
                             : "Scanning Steam, Heroic, Lutris and PE imports…")
                          : (gameModel.searchText.length > 0 || gameModel.filterMode !== "all" || gameModel.sourceFilter !== "all"
                             ? gameModel.count + " of " + gameModel.totalCount + " entries"
                             : gameModel.totalCount + " games / programs")
                    opacity: 0.7
                }

                RowLayout {
                    Layout.fillWidth: true
                    RenoButton {
                        text: installer.bulkUpdateBusy ? "Checking updates…" : (root.updateCenterActionableCount > 0 ? root.updateCenterCountText() : "Update Center")
                        enabled: !installer.updateQueueBusy
                        onClicked: {
                            updateCenterPopup.open()
                            root.resetUpdateCenterSelection()
                            if (!installer.bulkUpdateBusy && !installer.busy && !gameModel.scanning)
                                installer.checkAllUpdates()
                        }
                    }
                    RenoButton {
                        text: root.bulkMode ? "Done" : "Select"
                        enabled: !installer.busy
                        onClicked: {
                            root.bulkMode = !root.bulkMode
                            if (!root.bulkMode) gameModel.clearBulkSelection()
                        }
                    }
                    RenoButton { text: "+ Add Program"; enabled: !installer.busy && !installer.bulkUpdateBusy; onClicked: customProgramPopup.openAdd() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    TextField {
                        id: gameSearch
                        Layout.fillWidth: true
                        placeholderText: "Search games and programs…"
                        enabled: !installer.busy
                        selectByMouse: true
                        onTextChanged: gameModel.searchText = text
                    }
                    RenoToolButton { text: "×"; visible: gameSearch.text.length > 0; enabled: !installer.busy; onClicked: gameSearch.clear() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    RenoComboBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 230
                        implicitHeight: 32
                        font.pixelSize: 12
                        model: ["All entries", "Favorites", "ReShade installed", "RenoDX installed", "External installs", "Updates available (checked)", "No ReShade/RenoDX", "Hidden games"]
                        onActivated: {
                            const values = ["all", "favorites", "reshade", "renodx", "external", "updates", "none", "hidden"]
                            gameModel.filterMode = values[currentIndex]
                        }
                    }
                    RenoComboBox {
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 110
                        Layout.maximumWidth: 110
                        implicitHeight: 32
                        font.pixelSize: 12
                        model: ["All sources", "Steam", "Heroic", "Lutris", "Custom"]
                        onActivated: {
                            const values = ["all", "steam", "heroic", "lutris", "custom"]
                            gameModel.sourceFilter = values[currentIndex]
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Sort:"; opacity: 0.55; font.pixelSize: 11 }
                    RenoComboBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 230
                        implicitHeight: 32
                        font.pixelSize: 12
                        model: ["Name", "Source", "Graphics API", "Engine", "ReShade status", "RenoDX status"]
                        onActivated: {
                            const values = ["name", "source", "api", "engine", "reshade", "renodx"]
                            gameModel.sortMode = values[currentIndex]
                        }
                    }
                }

                Rectangle {
                    visible: root.bulkMode
                    Layout.fillWidth: true
                    implicitHeight: bulkActions.implicitHeight + 12
                    radius: 7
                    color: root.controlBg
                    border.width: 1
                    border.color: root.dividerColor
                    ColumnLayout {
                        id: bulkActions
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 6
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: gameModel.bulkSelectionCount + " selected"; font.bold: true; Layout.fillWidth: true }
                            RenoButton { text: "Clear selection"; enabled: gameModel.bulkSelectionCount > 0; onClicked: gameModel.clearBulkSelection() }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 5
                            RenoButton { text: "Hide"; enabled: gameModel.bulkSelectionCount > 0; onClicked: gameModel.bulkSetHidden(true) }
                            RenoButton { text: "Unhide"; enabled: gameModel.bulkSelectionCount > 0; onClicked: gameModel.bulkSetHidden(false) }
                            RenoButton {
                                text: "Rescan"
                                enabled: gameModel.bulkSelectionCount > 0
                                onClicked: { const id = root.selectedGameKey; gameModel.bulkRescanSelected(); nicknamePopup.reselect(id) }
                            }
                            RenoButton {
                                text: "Clear detection overrides"
                                enabled: gameModel.bulkSelectionCount > 0
                                onClicked: { const id = root.selectedGameKey; if (gameModel.bulkClearDetectionOverrides()) nicknamePopup.reselect(id) }
                            }
                            RenoButton {
                                text: "Refresh Steam artwork"
                                enabled: gameModel.bulkSelectionCount > 0
                                onClicked: {
                                    const ids = gameModel.bulkSelectedSteamAppIds()
                                    for (let i = 0; i < ids.length; ++i) coverService.refreshArtwork(ids[i])
                                }
                            }
                        }
                    }
                }

                Label {
                    visible: (installer.bulkUpdateSummary || "").length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: installer.bulkUpdateSummary || ""
                    color: installer.bulkUpdateAvailableComponents > 0 ? root.warningColor : root.mutedTextColor
                    opacity: 0.78
                    font.pixelSize: 11
                }

                Label {
                    visible: !gameModel.scanning && gameModel.totalCount > 0 && gameModel.count === 0
                    Layout.fillWidth: true
                    text: "No entries match the current search / filters."
                    opacity: 0.55
                    horizontalAlignment: Text.AlignHCenter
                }

                Item {
                    id: gameListFrame
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: gameList
                        anchors.fill: parent
                        clip: true
                        spacing: 4
                        model: gameModel
                        currentIndex: gameModel.count > 0 ? 0 : -1
                        property real gameRowHeight: appSettings.gameCoversEnabled ? 106 : 88
                        property real estimatedContentHeight: gameModel.count > 0
                                                              ? gameModel.count * gameRowHeight + Math.max(0, gameModel.count - 1) * spacing
                                                              : 0
                        property bool needsVerticalScroll: estimatedContentHeight > height + 1

                        // Keep the list geometry fresh as the cached snapshot/background refresh changes
                        // the model, but do not use any of these measurements to decide whether the rail
                        // itself is visible. The game-list scrollbar is intentionally always rendered so
                        // SplitView/minimum-width transitions cannot hide it.
                        onWidthChanged: Qt.callLater(function() { gameList.forceLayout() })
                        onHeightChanged: Qt.callLater(function() { gameList.forceLayout() })
                        Connections {
                            target: gameModel
                            function onRevisionChanged() { Qt.callLater(function() { gameList.forceLayout() }) }
                            function onCountChanged() { Qt.callLater(function() { gameList.forceLayout() }) }
                        }
                        // Keep the scrollbar attached to the ListView for automatic thumb
                        // synchronization, but render it from the outer-left edge of the
                        // right/details panel. That panel owns the pixels, so left-pane
                        // delegates, clipping, and SplitView minimum-width transitions
                        // cannot cover it.
                        ScrollBar.vertical: ScrollBar {
                            id: gameListScrollBar
                            parent: detailsPanel
                            x: -4
                            y: gameListFrame.y + 10
                            height: gameListFrame.height
                            width: 20
                            policy: ScrollBar.AlwaysOn
                            visible: true
                            active: true
                            enabled: gameList.needsVerticalScroll
                            opacity: 1.0
                            z: 1000
                            minimumSize: height > 0 ? Math.min(1.0, 52.0 / height) : 0.0

                            background: Rectangle {
                                implicitWidth: 20
                                radius: 10
                                color: root.controlBg
                                border.width: 1
                                border.color: root.dividerColor
                            }

                            contentItem: Rectangle {
                                implicitWidth: 14
                                radius: 7
                                color: root.buttonHighlight
                                opacity: 1.0
                                border.width: 1
                                border.color: root.textColor
                            }
                        }

                        delegate: Rectangle {
                        id: gameDelegate
                        required property int index
                        required property string name
                        required property string appId
                        required property string source
                        required property bool customProgram
                        required property bool favorite
                        required property int duplicateCandidateCount
                        required property int linkedAlternateCount
                        required property bool hidden
                        required property string graphicsApi
                        required property string architecture
                        required property string engine
                        required property string coverArtSource
                        required property bool reshadeInstalled
                        required property bool reshadeManaged
                        required property bool reshadeExternal
                        required property string reshadeVersion
                        required property bool reshade64Installed
                        required property bool reshade64Managed
                        required property bool reshade64External
                        required property string reshade64Version
                        required property bool renodxInstalled
                        required property bool renodxManaged
                        required property bool renodxExternal
                        required property string renodxFile
                        required property bool reframeworkSupported
                        required property bool reframeworkInstalled
                        required property bool reframeworkManaged
                        required property bool reframeworkExternal
                        required property string reframeworkVersion
                        required property bool optiScalerInstalled
                        required property bool updateChecked
                        required property bool reshadeUpdateAvailable
                        required property bool renodxUpdateAvailable
                        required property bool reframeworkUpdateAvailable
                        required property bool integrityWarning
                        required property string integritySummary
                        required property string diagnosticLevel
                        required property string diagnosticSummary
                        required property bool exeOverridden
                        required property bool prefixOverridden
                        required property bool graphicsApiOverridden
                        required property bool artworkOverridden
                        required property int overrideCount
                        required property int detectionChangeCount
                        required property bool bulkSelected

                        property bool anyReShadeInstalled: reshadeInstalled || reshade64Installed
                        property bool anyReShadeExternal: reshadeExternal || reshade64External
                        property string listReShadeText: {
                            if (reshadeInstalled && reshade64Installed) {
                                if (anyReShadeExternal && (reshadeManaged || reshade64Managed))
                                    return "ReShade: Mixed"
                                if (anyReShadeExternal)
                                    return "ReShade: External"
                                return "ReShade: Managed (both)"
                            }
                            if (reshade64Installed)
                                return "ReShade64: " + (reshade64External ? "External" : "Managed") + (reshade64Version.length > 0 ? " · " + reshade64Version : "")
                            if (reshadeInstalled)
                                return "ReShade: " + (reshadeExternal ? "External" : "Managed") + (reshadeVersion.length > 0 ? " · " + reshadeVersion : "")
                            return "ReShade: Not installed"
                        }

                        x: 20
                        width: Math.max(0, gameList.width - 40)
                        height: gameList.gameRowHeight
                        color: "transparent"

                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            radius: 10
                            color: gameDelegate.ListView.isCurrentItem ? root.selectedBg : mouse.containsMouse ? root.hoverBg : "transparent"
                            border.width: gameDelegate.ListView.isCurrentItem ? 1 : 0
                            border.color: root.buttonHighlight
                        }

                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(event) {
                                gameList.currentIndex = index
                                if (event.button === Qt.LeftButton && root.bulkMode) {
                                    gameModel.toggleBulkSelected(index)
                                    return
                                }
                                if (event.button === Qt.RightButton) {
                                    gameContextMenu.gameRow = index
                                    gameContextMenu.popup()
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            CheckBox {
                                visible: root.bulkMode
                                checked: gameDelegate.bulkSelected
                                Layout.alignment: Qt.AlignVCenter
                                Accessible.name: "Select " + gameDelegate.name + " for bulk actions"
                                onClicked: gameModel.toggleBulkSelected(index)
                            }

                            Rectangle {
                                id: coverFrame
                                visible: appSettings.gameCoversEnabled
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 88
                                Layout.alignment: Qt.AlignVCenter
                                radius: 5
                                clip: true
                                color: root.controlBg
                                border.width: 1
                                border.color: ListView.isCurrentItem ? root.buttonHighlight : root.dividerColor

                                Image {
                                    id: gameCover
                                    anchors.fill: parent
                                    source: {
                                        coverService.revision
                                        if (!appSettings.gameCoversEnabled)
                                            return ""
                                        if ((gameDelegate.coverArtSource || "").length > 0)
                                            return gameDelegate.coverArtSource
                                        if (gameDelegate.source !== "Steam" || gameDelegate.appId.length === 0)
                                            return ""
                                        return coverService.coverSource(gameDelegate.appId)
                                    }
                                    asynchronous: true
                                    cache: false
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                }

                                Label {
                                    anchors.centerIn: parent
                                    visible: !gameCover.visible
                                    text: root.gameInitials(name)
                                    font.pixelSize: 17
                                    font.bold: true
                                    color: root.buttonHighlight
                                    opacity: 0.85
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                // Keep the details column shrinkable so long titles elide
                                // instead of forcing the delegate wider than the library pane.
                                Layout.minimumWidth: 0
                                spacing: 4

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.preferredHeight: 26
                                    text: name
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: graphicsApi + " • " + architecture + " • " + engine
                                    elide: Text.ElideRight
                                    opacity: 0.62
                                    font.pixelSize: 12
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    StatusBadge {
                                        visible: anyReShadeInstalled
                                        text: "ReShade"
                                        accent: anyReShadeExternal ? root.badgeWarningColor : root.badgeSuccessColor
                                        tooltip: listReShadeText
                                    }
                                    StatusBadge {
                                        visible: renodxInstalled
                                        text: "RenoDX"
                                        accent: renodxExternal ? root.badgeWarningColor : root.badgeSuccessColor
                                        tooltip: renodxFile.length > 0 ? renodxFile : "RenoDX installed"
                                    }
                                    StatusBadge {
                                        visible: optiScalerInstalled
                                        text: "OptiScaler"
                                        accent: root.badgeSuccessColor
                                        tooltip: "OptiScaler detected"
                                    }
                                    StatusBadge {
                                        visible: reframeworkInstalled
                                        text: "REF"
                                        accent: reframeworkExternal ? root.badgeWarningColor : root.badgeSuccessColor
                                        tooltip: reframeworkVersion.length > 0 ? root.reFrameworkVersionDisplay(reframeworkVersion) : "REFramework installed"
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }

        }

        Rectangle {
            id: detailsPanel
            SplitView.fillWidth: true
            color: root.panelBg
            radius: 8
            clip: false

            ScrollView {
                id: detailsScrollView
                anchors.fill: parent
                anchors.margins: 18
                contentWidth: availableWidth

                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.selectedRow >= 0
                        spacing: 10

                        Rectangle {
                            id: selectedGameBannerFrame
                            visible: appSettings.gameCoversEnabled
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(260, Math.min(340, width * 7 / 16))
                            radius: 10
                            clip: true
                            color: root.controlBg
                            border.width: 1
                            border.color: root.dividerColor

                            Image {
                                id: selectedGameBanner
                                anchors.fill: parent
                                source: {
                                    coverService.revision
                                    if (!appSettings.gameCoversEnabled)
                                        return ""
                                    const localBanner = root.selectedGame.bannerArtSource || root.selectedGame.coverArtSource || ""
                                    if (localBanner.length > 0)
                                        return localBanner
                                    if (root.selectedGame.source !== "Steam" || (root.selectedGame.appId || "").length === 0)
                                        return ""
                                    const banner = coverService.bannerSource(root.selectedGame.appId || "")
                                    return banner.length > 0 ? banner : coverService.coverSource(root.selectedGame.appId || "")
                                }
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: !selectedGameBanner.visible
                                text: root.gameInitials(root.selectedGame.name || "")
                                font.pixelSize: 34
                                font.bold: true
                                color: root.buttonHighlight
                                opacity: 0.78
                            }

                            // Blend the artwork into the surrounding details panel on every edge.
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: Math.min(64, parent.height * 0.28)
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: root.panelBg }
                                    GradientStop { position: 1.0; color: "#00000000" }
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                width: Math.min(64, parent.width * 0.12)
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: root.panelBg }
                                    GradientStop { position: 1.0; color: "#00000000" }
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                width: Math.min(64, parent.width * 0.12)
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "#00000000" }
                                    GradientStop { position: 1.0; color: root.panelBg }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: Math.min(90, parent.height * 0.45)
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#00000000" }
                                    GradientStop { position: 1.0; color: root.panelBg }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedGame.name || ""
                            font.pixelSize: 26
                            font.bold: true
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: (root.selectedGame.nickname || "").length > 0 &&
                                     (root.selectedGame.originalName || "").length > 0
                            text: "Original: " + (root.selectedGame.originalName || "")
                            opacity: 0.48
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: root.selectedGame.source || ""; opacity: 0.62 }
                            Label {
                                visible: root.selectedGame.source === "Steam"
                                text: "AppID " + (root.selectedGame.appId || "")
                                opacity: 0.45
                                font.pixelSize: 11
                            }
                            Label {
                                visible: root.selectedOptiAnalysis.drift === true
                                text: "• OptiScaler config changed externally"
                                color: root.warningColor
                                opacity: 0.9
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: (root.selectedGame.graphicsApi || "Unknown") + "  •  " +
                                  (root.selectedGame.architecture || "Unknown") + "  •  " +
                                  (root.selectedGame.engine || "Unknown")
                            opacity: 0.66
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedGame.exePath || "Executable not found"
                            opacity: 0.48
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: healthRow.implicitHeight + 14
                            radius: 8
                            color: root.controlBg
                            border.width: 1
                            border.color: (root.selectedGame.healthLevel || "ok") === "ok" ? root.dividerColor : root.warningColor
                            RowLayout {
                                id: healthRow
                                anchors.fill: parent
                                anchors.margins: 7
                                spacing: 8
                                Label {
                                    text: (root.selectedGame.healthLevel || "ok") === "ok" ? "✓" : "⚠"
                                    color: (root.selectedGame.healthLevel || "ok") === "ok" ? root.successColor : root.warningColor
                                    font.bold: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Label { text: root.selectedGame.healthSummary || "Ready"; font.bold: true }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (root.selectedGame.detectionChangeCount || 0) > 0
                                              ? (root.selectedGame.detectionChangeCount + " detection change" + (root.selectedGame.detectionChangeCount === 1 ? "" : "s") + " recorded")
                                              : ((root.selectedGame.overrideCount || 0) > 0
                                                 ? (root.selectedGame.overrideCount + " Reno119 override" + (root.selectedGame.overrideCount === 1 ? "" : "s") + " active")
                                                 : "Automatic detection and managed state look normal")
                                        opacity: 0.55
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }
                                Label {
                                    visible: (root.selectedGame.healthTarget || "none") !== "none"
                                    text: "Open ›"
                                    color: root.infoColor
                                    font.pixelSize: 11
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: (root.selectedGame.healthTarget || "none") !== "none"
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: root.activateHealthTarget()
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            RenoButton {
                                visible: root.selectedGame.source === "Steam"
                                text: "Refresh artwork"
                                onClicked: coverService.refreshArtwork(root.selectedGame.appId || "")
                            }
                            RenoButton {
                                visible: root.selectedGame.source === "Steam"
                                text: "Clear artwork"
                                onClicked: coverService.clearArtwork(root.selectedGame.appId || "")
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0
                                text: root.selectedGame.artworkOverridden === true ? "Change custom artwork" : "Set custom artwork"
                                onClicked: {
                                    root.pendingArtworkRow = root.selectedRow
                                    root.openFilePickerAt(customArtworkQuickDialog,
                                                          root.selectedGame.artworkOverridden === true ? (root.selectedGame.coverArtPath || "") : "",
                                                          "customArtworkQuick",
                                                          "Choose custom artwork")
                                }
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0 && root.selectedGame.artworkOverridden === true
                                text: "Remove custom artwork"
                                onClicked: gameModel.clearArtworkOverride(root.selectedRow)
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0
                                text: (root.selectedGame.nickname || "").length > 0 ? "Change nickname" : "Set nickname"
                                onClicked: nicknamePopup.openFor(root.selectedRow)
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0 && (root.selectedGame.nickname || "").length > 0
                                text: "Clear nickname"
                                onClicked: {
                                    const appId = root.selectedGame.appId || ""
                                    if (gameModel.clearGameNickname(root.selectedRow))
                                        nicknamePopup.reselect(appId)
                                }
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0 && (((root.selectedGame.duplicateCandidateCount || 0) > 0) || ((root.selectedGame.linkedAlternateCount || 0) > 0))
                                text: (root.selectedGame.linkedAlternateCount || 0) > 0 ? "Alternate installs" : "Review duplicate"
                                onClicked: duplicatePopup.openFor(root.selectedRow)
                            }
                            RenoButton {
                                visible: root.selectedRow >= 0
                                text: root.selectedGame.hidden === true ? "Unhide game" : "Hide game"
                                onClicked: gameModel.setGameHidden(root.selectedRow, root.selectedGame.hidden !== true)
                            }
                            RenoButton {
                                visible: root.selectedGame.customProgram === true
                                text: "Edit"
                                onClicked: customProgramPopup.openEdit(root.selectedRow)
                            }
                            RenoButton {
                                visible: root.selectedGame.customProgram === true
                                text: "Remove entry"
                                onClicked: removeCustomDialog.open()
                            }
                        }
                    }

                    Label {
                        visible: root.selectedRow < 0
                        Layout.fillWidth: true
                        text: "Select a game or program"
                        font.pixelSize: 26
                        font.bold: true
                    }

                    GroupBox {
                        visible: root.selectedRow >= 0
                        title: "Recommended setup"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 7

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Based on the detected executable, engine, current installs, and the live compatibility metadata available to Reno119. Use the action beside a recommendation to install, update, preview, or fix it. Actions that replace external files or use a partial RenoDX match still require confirmation."
                                color: root.mutedTextColor
                                opacity: root.roleTheme ? 1.0 : 0.72
                                font.pixelSize: 11
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: "Setup health"; opacity: 0.55; font.pixelSize: 11 }
                                HealthChip {
                                    label: "ReShade"
                                    stateText: root.reShadeHealthState().text
                                    stateColor: root.reShadeHealthState().color
                                    ToolTip.visible: reShadeHealthHover.containsMouse
                                    ToolTip.text: root.reShadeHealthSummary()
                                    MouseArea {
                                        id: reShadeHealthHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }
                                HealthChip {
                                    label: "RenoDX"
                                    stateText: root.renoDxHealthState().text
                                    stateColor: root.renoDxHealthState().color
                                }
                                HealthChip {
                                    visible: root.selectedGame.reframeworkSupported === true || root.selectedGame.reframeworkInstalled === true
                                    label: "REF"
                                    stateText: root.reFrameworkHealthState().text
                                    stateColor: root.reFrameworkHealthState().color
                                    ToolTip.visible: reFrameworkHealthHover.containsMouse
                                    ToolTip.text: root.reFrameworkHealthSummary()
                                    MouseArea {
                                        id: reFrameworkHealthHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }
                                HealthChip {
                                    label: "OptiScaler"
                                    stateText: root.optiHealthState().text
                                    stateColor: root.optiHealthState().color
                                }
                                HealthChip {
                                    visible: root.selectedGame.integrityWarning === true
                                    label: "Integrity"
                                    stateText: "⚠"
                                    stateColor: root.warningColor
                                    ToolTip.visible: integrityHealthHover.containsMouse
                                    ToolTip.text: root.selectedGame.integritySummary || "Managed setup changed outside Reno119"
                                    MouseArea {
                                        id: integrityHealthHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }
                                Label {
                                    visible: root.optiChoicesDirty
                                    text: "● OptiScaler pending"
                                    color: root.warningColor
                                    font.pixelSize: 11
                                }
                                Item { Layout.fillWidth: true }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                RenoButton {
                                    text: root.recommendedSetupActive ? "Setting up…" : "Set up recommended"
                                    enabled: !root.recommendedSetupActive && !installer.busy && !installer.updateQueueBusy && !installer.bulkUpdateBusy && root.recommendedSetupSafePlan().length > 0
                                    onClicked: root.startRecommendedSetup()
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: root.recommendedSetupStatusText()
                                    color: root.recommendedSetupActive ? root.infoColor : root.mutedTextColor
                                    font.pixelSize: 11
                                    opacity: 0.82
                                }
                            }

                            Repeater {
                                model: root.recommendedSetupItems()
                                delegate: RecommendationRow {
                                    required property var modelData
                                    itemName: modelData.name || ""
                                    detailText: modelData.detail || ""
                                    stateText: modelData.state || ""
                                    stateColor: root.recommendationColor(modelData.level || "")
                                    actionText: modelData.actionText || ""
                                    actionEnabled: root.selectedRow >= 0
                                    onActionRequested: root.executeRecommendedSetupAction(modelData.action || "")
                                }
                            }

                            Rectangle {
                                visible: root.selectedGame.source === "Steam"
                                         ? ((root.selectedGame.launchOptions || "") + "").length > 0
                                         : ((root.selectedGame.wineOverrides || "") + "").length > 0
                                Layout.fillWidth: true
                                implicitHeight: protonRecommendation.implicitHeight + 16
                                radius: 7
                                color: root.controlBg
                                border.width: 1
                                border.color: root.dividerColor

                                RowLayout {
                                    id: protonRecommendation
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label { text: root.selectedGame.source === "Steam" ? "Suggested Proton override" : "Suggested Wine override"; font.bold: true }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.selectedGame.source === "Steam" ? (root.selectedGame.launchOptions || "") : (root.selectedGame.wineOverrides || "")
                                            elide: Text.ElideMiddle
                                            color: root.mutedTextColor
                                            font.family: "monospace"
                                            font.pixelSize: 10
                                            opacity: 0.82
                                        }
                                    }
                                    RenoButton {
                                        text: "Copy"
                                        enabled: !installer.busy
                                        onClicked: installer.copyText(root.selectedGame.source === "Steam" ? (root.selectedGame.launchOptions || "") : (root.selectedGame.wineOverrides || ""))
                                    }
                                }
                            }
                        }
                    }

                    GridLayout {
                        visible: root.selectedRow >= 0
                        columns: 2
                        columnSpacing: 18
                        rowSpacing: 8
                        Layout.fillWidth: true
                        Label { text: "Graphics API"; opacity: 0.55 }
                        Label { text: root.selectedGame.graphicsApi || "Unknown" }
                        Label { text: "Architecture"; opacity: 0.55 }
                        Label { text: root.selectedGame.architecture || "Unknown" }
                        Label { text: "Engine"; opacity: 0.55 }
                        Label { text: root.selectedGame.engine || "Unknown" }
                        Label { text: "Executable"; opacity: 0.55 }
                        Label { Layout.fillWidth: true; text: root.selectedGame.exePath || "Not found"; elide: Text.ElideMiddle }
                        Label { text: "Prefix"; opacity: 0.55 }
                        Label { Layout.fillWidth: true; text: root.selectedGame.protonPrefix || "Not configured / not created"; elide: Text.ElideMiddle }
                    }

                    RowLayout {
                        visible: root.selectedRow >= 0
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "PCGamingWiki"; opacity: 0.55 }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: root.selectedPcgwInfo.pending === true
                                  ? "Looking up game page…"
                                  : root.selectedPcgwInfo.resolved === true
                                    ? (root.selectedPcgwInfo.title || root.selectedGame.name || "")
                                    : (root.selectedPcgwInfo.queried === true ? (root.selectedPcgwInfo.error || "No matching page found") : "Supplemental compatibility / game information")
                            color: root.selectedPcgwInfo.queried === true && root.selectedPcgwInfo.resolved !== true ? root.warningColor : root.mutedTextColor
                            opacity: 0.75
                            font.pixelSize: 11
                        }
                        RenoButton {
                            text: root.selectedPcgwInfo.resolved === true ? "Open page" : (root.selectedPcgwInfo.pending === true ? "Looking up…" : (root.selectedPcgwInfo.queried === true ? "Retry lookup" : "Find game"))
                            enabled: root.selectedPcgwInfo.pending !== true
                            onClicked: {
                                if (root.selectedPcgwInfo.resolved === true) {
                                    Qt.openUrlExternally(root.selectedPcgwInfo.url || "")
                                } else {
                                    if (root.selectedPcgwInfo.queried === true) pcGamingWiki.clear(root.selectedGameKey)
                                    pcGamingWiki.resolve(root.selectedGameKey, root.selectedGame.originalName || root.selectedGame.name || "")
                                }
                            }
                        }
                    }

                    CollapsibleHeader {
                        visible: root.selectedRow >= 0
                        titleText: "Detection overrides"
                        summaryText: {
                            let count = 0
                            if (root.selectedGame.exeOverridden === true) count++
                            if (root.selectedGame.prefixOverridden === true) count++
                            if (root.selectedGame.graphicsApiOverridden === true) count++
                            return count === 0 ? "Automatic detection" : (count + (count === 1 ? " override active" : " overrides active"))
                        }
                        expanded: root.detectionOverridesExpanded
                        onToggleRequested: root.setComponentExpanded("detectionOverrides", !root.detectionOverridesExpanded)
                    }
                    GroupBox {
                        visible: root.selectedRow >= 0 && root.detectionOverridesExpanded
                        title: ""
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Automatic detection remains the baseline. These overrides are stored only by Reno119 and can be reset independently. Rescan this game re-checks local executable/prefix/API data without refreshing the whole library."
                                opacity: 0.72
                            }

                            Label { text: "Executable"; font.bold: true; opacity: 0.8 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: exeOverrideField
                                    Layout.fillWidth: true
                                    placeholderText: root.selectedGame.detectedExePath || root.selectedGame.exePath || "/path/to/game.exe"
                                }
                                RenoButton { text: "Browse…"; onClicked: root.openFilePickerAt(exeOverrideDialog, exeOverrideField.text.length > 0 ? exeOverrideField.text : (root.selectedGame.exePath || root.selectedGame.detectedExePath || ""), "exeOverride", "Choose Windows executable") }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Auto: " + (root.selectedGame.detectedExePath || "Unresolved") + (root.selectedGame.exeOverridden === true ? " · using override" : " · active")
                                opacity: 0.58
                                font.pixelSize: 11
                            }
                            RowLayout {
                                RenoButton {
                                    text: "Apply EXE override"
                                    enabled: !installer.busy && exeOverrideField.text.length > 0
                                    onClicked: {
                                        if (gameModel.setExecutableOverride(root.selectedRow, exeOverrideField.text))
                                            Qt.callLater(root.loadDetectionOverrides)
                                    }
                                }
                                RenoButton {
                                    text: "Use auto EXE"
                                    enabled: !installer.busy && root.selectedGame.exeOverridden === true
                                    onClicked: {
                                        gameModel.clearExecutableOverride(root.selectedRow)
                                        exeOverrideField.text = ""
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "Wine / Proton prefix"; font.bold: true; opacity: 0.8 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: prefixOverrideField
                                    Layout.fillWidth: true
                                    placeholderText: root.selectedGame.detectedProtonPrefix || root.selectedGame.protonPrefix || "/path/to/prefix"
                                }
                                RenoButton { text: "Browse…"; onClicked: root.openFolderPickerAt(prefixOverrideDialog, prefixOverrideField.text.length > 0 ? prefixOverrideField.text : (root.selectedGame.protonPrefix || root.selectedGame.detectedProtonPrefix || ""), "prefixOverride", "Choose Wine / Proton prefix") }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Auto: " + (root.selectedGame.detectedProtonPrefix || "Unresolved") + (root.selectedGame.prefixOverridden === true ? " · using override" : " · active")
                                opacity: 0.58
                                font.pixelSize: 11
                            }
                            RowLayout {
                                RenoButton {
                                    text: "Apply prefix override"
                                    enabled: !installer.busy && prefixOverrideField.text.length > 0
                                    onClicked: {
                                        if (gameModel.setPrefixOverride(root.selectedRow, prefixOverrideField.text))
                                            Qt.callLater(root.loadDetectionOverrides)
                                    }
                                }
                                RenoButton {
                                    text: "Use auto prefix"
                                    enabled: !installer.busy && root.selectedGame.prefixOverridden === true
                                    onClicked: {
                                        gameModel.clearPrefixOverride(root.selectedRow)
                                        prefixOverrideField.text = ""
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "Graphics API"; font.bold: true; opacity: 0.8 }
                            RowLayout {
                                Layout.fillWidth: true
                                RenoComboBox {
                                    id: graphicsApiOverrideChoice
                                    Layout.preferredWidth: 190
                                    model: root.graphicsApiOverrideChoices
                                }
                                RenoButton {
                                    text: "Apply API"
                                    enabled: !installer.busy
                                    onClicked: {
                                        gameModel.setGraphicsApiOverride(root.selectedRow, graphicsApiOverrideChoice.currentText)
                                        Qt.callLater(root.loadDetectionOverrides)
                                    }
                                }
                                RenoButton {
                                    text: "Use auto API"
                                    enabled: !installer.busy && root.selectedGame.graphicsApiOverridden === true
                                    onClicked: {
                                        gameModel.clearGraphicsApiOverride(root.selectedRow)
                                        graphicsApiOverrideChoice.currentIndex = 0
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Auto: " + (root.selectedGame.detectedGraphicsApi || "Unknown") + " · Active: " + (root.selectedGame.graphicsApi || "Unknown")
                                opacity: 0.58
                                font.pixelSize: 11
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton {
                                    text: "Rescan this game"
                                    enabled: !installer.busy && root.selectedRow >= 0
                                    onClicked: {
                                        if (gameModel.rescanGame(root.selectedRow)) {
                                            Qt.callLater(root.loadDetectionOverrides)
                                            root.verificationNonce++
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Only this entry and its cached snapshot data are refreshed. Use Refresh library if launcher metadata itself changed."
                                    wrapMode: Text.Wrap
                                    opacity: 0.5
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    CollapsibleHeader {
                        visible: root.selectedRow >= 0
                        titleText: "ReShade — full add-on build"
                        summaryText: root.ownershipLabel(root.selectedGame.reshadeInstalled === true, root.selectedGame.reshadeManaged === true, root.selectedGame.reshadeExternal === true) +
                                     ((root.selectedGame.reshadeVersion || "").length > 0 ? " · " + root.selectedGame.reshadeVersion : "")
                        expanded: root.reshadeExpanded
                        onToggleRequested: root.setComponentExpanded("reshade", !root.reshadeExpanded)
                    }
                    GroupBox {
                        id: reshadeCard
                        visible: root.selectedRow >= 0 && root.reshadeExpanded
                        title: ""
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Choose the RenoDX-recommended build, the latest full add-on ReShade release, or your configured Custom source. Unmanaged ReShade is only overwritten after an explicit confirmation and takeover backup."
                                opacity: 0.72
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: !root.selectedGame.reshadeInstalled
                                      ? "Status: Not installed"
                                      : root.selectedGame.reshadeExternal
                                        ? "Status: External" + ((root.selectedGame.reshadeVersion || "").length > 0 ? " — ReShade " + root.selectedGame.reshadeVersion : " — version unknown") + ((root.selectedGame.reshadeProxy || "").length > 0 ? " via " + root.selectedGame.reshadeProxy : "")
                                        : "Status: Managed — ReShade " + ((root.selectedGame.reshadeVersion || "").length > 0 ? root.selectedGame.reshadeVersion : "unknown") + ((root.selectedGame.reshadeChannel || "").length > 0 ? " (" + root.selectedGame.reshadeChannel + ")" : "") + ((root.selectedGame.reshadeProxy || "").length > 0 ? " via " + root.selectedGame.reshadeProxy : "")
                                color: root.selectedGame.reshadeExternal ? root.warningColor : (root.selectedGame.reshadeInstalled ? root.successColor : root.mutedTextColor)
                                opacity: 0.9
                            }
                            Label {
                                visible: (root.selectedReShadeInlineStatus.text || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Last action: " + (root.selectedReShadeInlineStatus.text || "")
                                color: root.statusColor(root.selectedReShadeInlineStatus)
                                opacity: 0.92
                                font.pixelSize: 12
                            }
                            RowLayout {
                                Label { text: "Build:"; opacity: 0.65 }
                                RenoComboBox {
                                    id: reshadeBuildChoice
                                    model: root.reshadeBuildModel()
                                    currentIndex: 0
                                    enabled: !installer.busy
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                opacity: 0.58
                                font.pixelSize: 11
                                text: reshadeBuildChoice.currentIndex === 0
                                      ? "Recommended follows the current full add-on ReShade baseline published by RenoDX."
                                      : reshadeBuildChoice.currentIndex === 1
                                        ? "Latest checks reshade.me and installs the newest full add-on build."
                                        : (appSettings.customReShadeConfigured
                                           ? "Custom uses the source configured in Settings."
                                           : "Custom is not configured yet. Open Settings to choose a version, URL, or local file.")
                            }
                            Label {
                                visible: reshadeBuildChoice.currentIndex === 2
                                text: appSettings.customReShadeSummary
                                color: appSettings.customReShadeConfigured ? root.successColor : root.warningColor
                                opacity: 0.88
                            }
                            RowLayout {
                                RenoButton {
                                    text: root.selectedGame.reshadeExternal ? "Install over external ReShade" : (root.selectedGame.reshadeInstalled ? "Reinstall / Update ReShade" : "Install ReShade")
                                    enabled: !installer.busy && root.selectedGame.graphicsApi !== "Vulkan" && (reshadeBuildChoice.currentIndex !== 2 || appSettings.customReShadeConfigured)
                                    onClicked: {
                                        if (root.selectedGame.reshadeExternal === true) {
                                            externalReShadeOverwriteDialog.open()
                                        } else {
                                            installer.installReShade(root.selectedRow,
                                                                     reshadeBuildChoice.currentIndex === 0 ? "recommended"
                                                                                                          : reshadeBuildChoice.currentIndex === 1 ? "latest"
                                                                                                                                                 : "custom")
                                        }
                                    }
                                }
                                RenoButton { text: "Remove ReShade"; enabled: !installer.busy && root.selectedGame.reshadeManaged === true; onClicked: installer.uninstallReShade(root.selectedRow) }
                                RenoButton { text: "Check updates"; enabled: !installer.busy; onClicked: installer.checkUpdates(root.selectedRow) }
                                Item { Layout.fillWidth: true }
                            }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.selectedUpdateInfo.reshadeSummary || ""; opacity: 0.7 }
                        }
                    }

                    CollapsibleHeader {
                        visible: root.selectedRow >= 0
                        titleText: "RenoDX HDR"
                        summaryText: (root.selectedRenoDxResolutionInfo.dedicatedAvailable ? "Dedicated addon now available · " : "") + root.ownershipLabel(root.selectedGame.renodxInstalled === true, root.selectedGame.renodxManaged === true, root.selectedGame.renodxExternal === true) +
                                     ((root.selectedGame.renodxFile || "").length > 0 ? " · " + root.selectedGame.renodxFile : "")
                        expanded: root.renodxExpanded
                        onToggleRequested: root.setComponentExpanded("renodx", !root.renodxExpanded)
                    }
                    GroupBox {
                        id: renodxCard
                        visible: root.selectedRow >= 0 && root.renodxExpanded
                        title: ""
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: (root.selectedRenoDxResolutionInfo.supportState || "Checking support…") + "\n" +
                                      (root.selectedRenoDxResolutionInfo.supportDetail || "Waiting for the RenoDX catalog.")
                                color: (root.selectedRenoDxResolutionInfo.dedicatedMatch === true || root.selectedRenoDxResolutionInfo.dedicatedAvailable === true) ? root.infoColor : root.mutedTextColor
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                visible: (root.selectedRenoDxResolutionInfo.url || "").length > 0
                                implicitHeight: resolutionColumn.implicitHeight + 14
                                radius: 6
                                color: root.controlBg
                                border.width: 1
                                border.color: root.selectedRenoDxResolutionInfo.requiresConfirmation === true ? root.warningColor : root.dividerColor
                                ColumnLayout {
                                    id: resolutionColumn
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    spacing: 3
                                    Label { text: "Addon resolution"; font.bold: true; opacity: 0.82 }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: root.selectedRenoDxResolutionInfo.dedicatedMatch === true
                                              ? ((root.selectedRenoDxResolutionInfo.matchedCatalogTitle || "Unknown catalog title") + " · " + (root.selectedRenoDxResolutionInfo.matchMethod || "Unknown match"))
                                              : (root.selectedRenoDxResolutionInfo.matchMethod || root.selectedRenoDxResolutionInfo.source || "Resolved addon")
                                        color: root.selectedRenoDxResolutionInfo.requiresConfirmation === true ? root.warningColor : root.infoColor
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: (root.selectedRenoDxResolutionInfo.file || "").length > 0
                                        text: root.selectedRenoDxResolutionInfo.file || ""
                                        wrapMode: Text.WrapAnywhere
                                        opacity: 0.62
                                        font.pixelSize: 10
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.selectedRenoDxResolutionInfo.requiresConfirmation === true
                                        text: "Non-exact dedicated match · confirmation required before install"
                                        color: root.warningColor
                                        wrapMode: Text.Wrap
                                        font.pixelSize: 10
                                    }
                                }
                            }
                            Rectangle {
                                visible: (root.selectedRhiGameNote || "").trim().length > 0
                                Layout.fillWidth: true
                                implicitHeight: rhiNoteColumn.implicitHeight + 16
                                radius: 7
                                color: root.controlBg
                                border.width: 1
                                border.color: root.dividerColor

                                ColumnLayout {
                                    id: rhiNoteColumn
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    CollapsibleHeader {
                                        titleText: "Game note"
                                        summaryText: root.rhiNoteExpanded ? "Title-specific guidance" : "Click to view game-specific guidance"
                                        expanded: root.rhiNoteExpanded
                                        onToggleRequested: root.setComponentExpanded("rhiNote", !root.rhiNoteExpanded)
                                    }
                                    Label {
                                        visible: root.rhiNoteExpanded
                                        Layout.fillWidth: true
                                        text: root.selectedRhiGameNote
                                        textFormat: Text.PlainText
                                        wrapMode: Text.Wrap
                                        color: root.mutedTextColor
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: !root.selectedGame.renodxInstalled
                                      ? "Status: Not installed"
                                      : root.selectedGame.renodxExternal
                                        ? "Status: External" + ((root.selectedGame.renodxFile || "").length > 0 ? " — " + root.selectedGame.renodxFile : "")
                                        : "Status: Managed" + ((root.selectedGame.renodxFile || "").length > 0 ? " — " + root.selectedGame.renodxFile : "")
                                color: root.selectedGame.renodxExternal ? root.warningColor : (root.selectedGame.renodxInstalled ? root.successColor : root.mutedTextColor)
                                opacity: 0.9
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                property bool hasLiveStatus: installer.renoDxStatusAppId === (root.selectedGame.appId || "") && (installer.renoDxStatus || "").length > 0
                                visible: hasLiveStatus || (root.selectedRenoDxInlineStatus.text || "").length > 0
                                text: hasLiveStatus ? (installer.renoDxStatus || "") : ("Last action: " + (root.selectedRenoDxInlineStatus.text || ""))
                                color: {
                                    if (!hasLiveStatus)
                                        return root.statusColor(root.selectedRenoDxInlineStatus)
                                    const message = (installer.renoDxStatus || "").toLowerCase()
                                    if (message.indexOf("installed:") >= 0 || message.indexOf("matched renodx addon:") === 0 || message.indexOf("removed ") === 0)
                                        return root.successColor
                                    if (message.indexOf("downloading") === 0 || message.indexOf("resolving") === 0 || message.indexOf("matched a renodx") === 0)
                                        return root.infoColor
                                    return root.warningColor
                                }
                                opacity: 0.92
                                font.pixelSize: 12
                            }
                            RowLayout {
                                RenoButton {
                                    text: root.selectedGame.renodxExternal ? "Install over external RenoDX" : (root.selectedGame.renodxInstalled ? "Reinstall RenoDX" : "Install RenoDX")
                                    enabled: !installer.busy && root.selectedRow >= 0 && (root.selectedGame.reshadeInstalled === true || root.selectedGame.reshade64Installed === true)
                                    onClicked: {
                                        const resolution = root.selectedRenoDxResolutionInfo || ({})
                                        if (resolution.requiresConfirmation === true) {
                                            nonExactRenoDxMatchDialog.gameRow = root.selectedRow
                                            nonExactRenoDxMatchDialog.externalOverwrite = root.selectedGame.renodxExternal === true
                                            nonExactRenoDxMatchDialog.gameName = root.selectedGame.name || "Selected game"
                                            nonExactRenoDxMatchDialog.matchTitle = resolution.matchedCatalogTitle || ""
                                            nonExactRenoDxMatchDialog.matchMethod = resolution.matchMethod || ""
                                            nonExactRenoDxMatchDialog.addonFile = resolution.file || ""
                                            nonExactRenoDxMatchDialog.matchUrl = resolution.url || ""
                                            nonExactRenoDxMatchDialog.open()
                                        } else if (root.selectedGame.renodxExternal === true) {
                                            installer.installRenoDxOverExternal(root.selectedRow)
                                        } else {
                                            installer.installRenoDx(root.selectedRow)
                                        }
                                    }
                                }
                                RenoButton { text: "Remove RenoDX"; enabled: !installer.busy && root.selectedGame.renodxManaged === true; onClicked: installer.uninstallRenoDx(root.selectedRow) }
                                Item { Layout.fillWidth: true }
                            }

                            Rectangle {
                                visible: root.selectedRenoDxTweakInfo.available === true || root.selectedRenoDxTweakInfo.canRestore === true
                                Layout.fillWidth: true
                                implicitHeight: tweakColumn.implicitHeight + 20
                                radius: 8
                                color: root.alternateBg
                                border.color: root.dividerColor
                                border.width: 1

                                ColumnLayout {
                                    id: tweakColumn
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: "Managed RenoDX tweaks"
                                            font.bold: true
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: root.selectedRenoDxTweakInfo.applied === true
                                                  ? "Applied"
                                                  : root.selectedRenoDxTweakInfo.profilePending === true ? "Checking…" : "Available"
                                            color: root.selectedRenoDxTweakInfo.applied === true
                                                   ? root.successColor
                                                   : root.selectedRenoDxTweakInfo.profilePending === true ? root.infoColor : root.warningColor
                                            font.bold: true
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.selectedRenoDxTweakInfo.profileChanged === true || root.selectedRenoDxTweakInfo.profileComparisonUnknown === true
                                        wrapMode: Text.Wrap
                                        color: root.warningColor
                                        text: root.selectedRenoDxTweakInfo.profileChanged === true
                                              ? "The available profile differs from the one you applied. Preview reapply to review the changes.\n" + (root.selectedRenoDxTweakInfo.profileChangeSummary || []).join("\n")
                                              : "This profile predates update tracking. A reviewed reapply will establish its comparison baseline."
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: root.selectedRenoDxTweakInfo.title || "Managed tweak profile"
                                        font.bold: true
                                        opacity: 0.92
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: root.selectedRenoDxTweakInfo.description || ""
                                        visible: text.length > 0
                                        opacity: 0.72
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: root.selectedRenoDxTweakInfo.engineStatus || ""
                                        visible: text.length > 0
                                        color: root.selectedRenoDxTweakInfo.canApply === true ? root.mutedTextColor : root.warningColor
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: root.selectedRenoDxTweakInfo.actionStatus || ""
                                        visible: text.length > 0
                                        color: root.infoColor
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WrapAnywhere
                                        text: (root.selectedRenoDxTweakInfo.engineIniPath || "").length > 0
                                              ? "Engine.ini: " + root.selectedRenoDxTweakInfo.engineIniPath
                                              : ""
                                        visible: text.length > 0
                                        opacity: 0.62
                                        font.pixelSize: 10
                                    }
                                    Repeater {
                                        model: root.selectedRenoDxTweakInfo.changes || []
                                        delegate: Label {
                                            required property string modelData
                                            Layout.fillWidth: true
                                            wrapMode: Text.WrapAnywhere
                                            text: "• " + modelData
                                            opacity: 0.78
                                            font.pixelSize: 11
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        text: (root.selectedRenoDxTweakInfo.source || "").length > 0
                                              ? "Source: " + root.selectedRenoDxTweakInfo.source
                                              : ""
                                        visible: text.length > 0
                                        opacity: 0.62
                                        font.pixelSize: 10
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        visible: root.selectedRenoDxTweakInfo.canRestore === true
                                        text: "Restore original previews the exact pre-tweak files before replacing them. Changes made since applying are checked, and a recovery copy is kept."
                                        color: root.warningColor
                                        opacity: 0.78
                                        font.pixelSize: 10
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        RenoButton {
                                            text: root.selectedRenoDxTweakInfo.applied === true ? "Preview reapply" : "Preview tweaks"
                                            enabled: !installer.busy && root.selectedRenoDxTweakInfo.canApply === true
                                            onClicked: tweakPreviewDialog.reviewGame(root.selectedRow, false)
                                        }
                                        RenoButton {
                                            text: "Restore original"
                                            enabled: !installer.busy && root.selectedRenoDxTweakInfo.canRestore === true
                                            onClicked: tweakPreviewDialog.reviewGame(root.selectedRow, true)
                                        }
                                        RenoButton {
                                            text: "Open Engine.ini folder"
                                            visible: (root.selectedRenoDxTweakInfo.engineIniPath || "").length > 0
                                            enabled: visible
                                            onClicked: installer.openRenoDxEngineIniFolder(root.selectedRow)
                                        }
                                        RenoButton {
                                            text: "Open source"
                                            visible: (root.selectedRenoDxTweakInfo.sourceUrl || "").length > 0
                                            enabled: visible
                                            onClicked: Qt.openUrlExternally(root.selectedRenoDxTweakInfo.sourceUrl)
                                        }
                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }

                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.selectedUpdateInfo.renodxSummary || ""; opacity: 0.7 }
                        }
                    }

                    CollapsibleHeader {
                        visible: root.selectedRow >= 0 &&
                                 (root.selectedGame.reframeworkSupported === true || root.selectedGame.reframeworkInstalled === true)
                        titleText: "REFramework Nightly"
                        summaryText: root.ownershipLabel(root.selectedGame.reframeworkInstalled === true, root.selectedGame.reframeworkManaged === true, root.selectedGame.reframeworkExternal === true) +
                                     ((root.selectedGame.reframeworkVersion || "").length > 0 ? " · " + root.reFrameworkVersionDisplay(root.selectedGame.reframeworkVersion) : "")
                        expanded: root.reframeworkExpanded
                        onToggleRequested: root.setComponentExpanded("reframework", !root.reframeworkExpanded)
                    }
                    GroupBox {
                        id: reframeworkCard
                        visible: root.selectedRow >= 0 && root.reframeworkExpanded &&
                                 (root.selectedGame.reframeworkSupported === true || root.selectedGame.reframeworkInstalled === true)
                        title: ""
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "For RE Engine games and other known REFramework-supported titles, Reno119 installs only the non-VR dinput8.dll from the current monolithic REFramework Nightly. When useful, dinput8=n,b is included only in the suggested Proton override; Reno119 never edits Steam launch options automatically."
                                opacity: 0.7
                                font.pixelSize: 11
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: !root.selectedGame.reframeworkInstalled
                                      ? "Status: Not installed"
                                      : root.selectedGame.reframeworkExternal === true
                                        ? "Status: External — dinput8.dll"
                                        : "Status: Managed" + ((root.selectedGame.reframeworkVersion || "").length > 0 ? " — " + root.reFrameworkVersionDisplay(root.selectedGame.reframeworkVersion) : "")
                                color: root.selectedGame.reframeworkExternal === true ? root.warningColor : (root.selectedGame.reframeworkInstalled === true ? root.successColor : root.mutedTextColor)
                            }
                            Label {
                                visible: (root.selectedReFrameworkInlineStatus.text || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Last action: " + (root.selectedReFrameworkInlineStatus.text || "")
                                color: root.statusColor(root.selectedReFrameworkInlineStatus)
                                font.pixelSize: 11
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton {
                                    text: root.selectedGame.reframeworkExternal === true
                                          ? "Install over external REFramework"
                                          : (root.selectedGame.reframeworkInstalled === true ? "Update / Reinstall Nightly" : "Install Nightly")
                                    enabled: !installer.busy && (root.selectedGame.reframeworkSupported === true || root.selectedGame.reframeworkExternal === true)
                                    onClicked: {
                                        if (root.selectedGame.reframeworkExternal === true)
                                            externalReFrameworkOverwriteDialog.open()
                                        else
                                            installer.installReFramework(root.selectedRow)
                                    }
                                }
                                RenoButton {
                                    text: "Remove"
                                    enabled: !installer.busy && root.selectedGame.reframeworkManaged === true
                                    onClicked: installer.uninstallReFramework(root.selectedRow)
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                visible: (root.selectedUpdateInfo.reframeworkSummary || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.selectedUpdateInfo.reframeworkSummary || ""
                                opacity: 0.7
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "REFramework overlay"; opacity: 0.65 }
                                Label {
                                    text: root.selectedReFrameworkHotkey.name || "Insert"
                                    font.bold: true
                                    ToolTip.visible: reframeworkHotkeyHover.containsMouse
                                    ToolTip.text: root.selectedReFrameworkHotkey.configured === true
                                                  ? "Read from existing " + (root.selectedReFrameworkHotkey.path || "REFramework config")
                                                  : (root.selectedReFrameworkHotkey.detectedExistingConfig === true
                                                     ? "Existing REFramework config found; the menu-key setting is unset, so Insert is the default."
                                                     : "No existing REFramework config found; Insert is the default until REFramework writes its config.")
                                    MouseArea { id: reframeworkHotkeyHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                }
                                RenoButton {
                                    text: "Change hotkey"
                                    enabled: root.selectedGame.reframeworkInstalled === true
                                    onClicked: root.openHotkeyDialog("reframework")
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                visible: root.overlayHotkeyConflict()
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "⚠ Overlay hotkey conflict: OptiScaler and REFramework are both set to " + (root.selectedReFrameworkHotkey.name || "Insert") + ". Change either shortcut to avoid both overlays opening together."
                                color: root.warningColor
                                font.pixelSize: 11
                            }
                        }
                    }

                    Rectangle {
                        id: optiCard
                        visible: root.selectedRow >= 0
                        Layout.fillWidth: true
                        radius: 8
                        color: root.controlBg
                        border.width: 1
                        border.color: root.dividerColor
                        implicitHeight: optiCardLayout.implicitHeight + 2

                        ColumnLayout {
                            id: optiCardLayout
                            x: 1
                            y: 1
                            width: Math.max(0, parent.width - 2)
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 58
                                radius: 7
                                color: optiHeaderMouse.containsMouse ? root.hoverBg : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label {
                                            Layout.fillWidth: true
                                            text: "OptiScaler integration — ReShade / RenoDX only" +
                                                  (root.optiChoicesDirty ? "  ●" :
                                                   (root.hasInlineError(root.selectedOptiInlineStatus) || root.hasInlineError(root.selectedReShade64InlineStatus) || root.selectedOptiAnalysis.conflict === true || root.selectedOptiAnalysis.drift === true || (root.selectedOptiAnalysis.issues || []).length > 0) ? "  ⚠" : "")
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.optiCollapsedSummary()
                                            opacity: 0.62
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Label {
                                        text: root.optiExpanded ? "▴" : "▾"
                                        font.pixelSize: 18
                                        opacity: 0.72
                                    }
                                }

                                MouseArea {
                                    id: optiHeaderMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.setOptiExpanded(!root.optiExpanded)
                                }
                            }

                            Item {
                                id: optiBodyClip
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                implicitHeight: root.optiExpanded ? optiBody.implicitHeight + 20 : 0
                                clip: true
                                enabled: root.optiExpanded
                                opacity: root.optiExpanded ? 1 : 0
                                Behavior on implicitHeight { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 120 } }

                                ColumnLayout {
                                    id: optiBody
                                    x: 10
                                    y: 10
                                    width: Math.max(0, parent.width - 20)
                                    spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.selectedOptiAnalysis.summary || ""
                                opacity: 0.76
                            }
                            Label {
                                visible: root.selectedOptiAnalysis.detected === true
                                text: "Health: " + (root.selectedOptiAnalysis.health || "Unknown")
                                color: (root.selectedOptiAnalysis.issues || []).length > 0 ? root.warningColor : root.successColor
                                opacity: 0.85
                                font.pixelSize: 12
                            }
                            Repeater {
                                model: root.selectedOptiAnalysis.issues || []
                                delegate: Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "• " + modelData; opacity: 0.62; font.pixelSize: 11 }
                            }
                            Label {
                                visible: root.selectedOptiAnalysis.conflict === true
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: root.warningColor
                                text: "⚠ " + (root.selectedOptiAnalysis.conflictText || "Proxy conflict detected.")
                            }
                            Label {
                                visible: root.selectedOptiAnalysis.drift === true
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: root.warningColor
                                opacity: 0.85
                                text: "Relevant integration settings changed externally. Reno119 will not overwrite them automatically."
                            }
                            Repeater {
                                model: root.selectedOptiAnalysis.driftDetails || []
                                delegate: Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "• " + modelData; opacity: 0.62; font.pixelSize: 11 }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "Proxy / conflict scan"; font.bold: true; opacity: 0.82 }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.selectedOptiAnalysis.proxyScanSummary || ""
                                opacity: 0.66
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: root.selectedOptiAnalysis.proxyScan || []
                                delegate: Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: "• " + (modelData.file || "") + " — " + (modelData.role || "")
                                    color: (modelData.severity || "") === "warning" ? root.warningColor : root.textColor
                                    opacity: 0.78
                                    font.pixelSize: 11
                                }
                            }
                            Repeater {
                                model: root.selectedOptiAnalysis.proxyWarnings || []
                                delegate: Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: "⚠ " + modelData
                                    color: root.warningColor
                                    opacity: 0.9
                                    font.pixelSize: 11
                                }
                            }
                            Repeater {
                                model: root.selectedOptiAnalysis.proxyConflicts || []
                                delegate: Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: "⚠ Conflict: " + modelData
                                    color: "#ffb4a9"
                                    opacity: 0.95
                                    font.pixelSize: 11
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton { text: "Re-scan"; onClicked: root.optiRefreshNonce++ }
                                Label {
                                    visible: root.selectedOptiAnalysis.fixAvailable === true && !root.optiChoicesDirty
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: "Repair is available from the OptiScaler row in Recommended setup."
                                    color: root.infoColor
                                    opacity: 0.72
                                    font.pixelSize: 10
                                }
                                Label {
                                    visible: root.selectedOptiAnalysis.fixAvailable !== true && (root.selectedOptiAnalysis.fixSummary || "").length > 0
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: root.selectedOptiAnalysis.fixSummary || ""
                                    opacity: 0.52
                                    font.pixelSize: 10
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "ReShade64 chain-loader"; font.bold: true; opacity: 0.82 }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Uses the same Recommended / Latest / Custom ReShade build selected above, but installs it explicitly as ReShade64.dll for OptiScaler LoadReshade. This does not replace or rename the normal ReShade proxy."
                                opacity: 0.66
                                font.pixelSize: 11
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: !root.selectedGame.reshade64Installed
                                      ? "ReShade64 status: Not installed"
                                      : root.selectedGame.reshade64External
                                        ? "ReShade64 status: External" + ((root.selectedGame.reshade64Version || "").length > 0 ? " — ReShade " + root.selectedGame.reshade64Version : " — version unknown")
                                        : "ReShade64 status: Managed — ReShade " + ((root.selectedGame.reshade64Version || "").length > 0 ? root.selectedGame.reshade64Version : "unknown") + ((root.selectedGame.reshade64Channel || "").length > 0 ? " (" + root.selectedGame.reshade64Channel + ")" : "")
                                color: root.selectedGame.reshade64External ? root.warningColor : (root.selectedGame.reshade64Installed ? root.successColor : root.mutedTextColor)
                                opacity: 0.9
                            }
                            Label {
                                visible: (root.selectedReShade64InlineStatus.text || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Last action: " + (root.selectedReShade64InlineStatus.text || "")
                                color: root.statusColor(root.selectedReShade64InlineStatus)
                                opacity: 0.92
                                font.pixelSize: 11
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton {
                                    text: root.selectedGame.reshade64External
                                          ? "Install over external ReShade64"
                                          : (root.selectedGame.reshade64Installed ? "Reinstall / Update ReShade64" : "Install ReShade64")
                                    enabled: !installer.busy &&
                                             root.selectedGame.architecture === "x64" &&
                                             (reshadeBuildChoice.currentIndex !== 2 || appSettings.customReShadeConfigured)
                                    onClicked: {
                                        if (root.selectedGame.reshade64External === true) {
                                            externalReShade64OverwriteDialog.open()
                                        } else {
                                            installer.installReShade64(root.selectedRow,
                                                                       reshadeBuildChoice.currentIndex === 0 ? "recommended"
                                                                                                            : reshadeBuildChoice.currentIndex === 1 ? "latest"
                                                                                                                                                   : "custom")
                                        }
                                    }
                                }
                                RenoButton {
                                    text: "Remove ReShade64"
                                    enabled: !installer.busy && root.selectedGame.reshade64Managed === true
                                    onClicked: installer.uninstallReShade64(root.selectedRow)
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                visible: root.selectedGame.architecture !== "x64"
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "ReShade64 chain-loading is only available for x64 games."
                                color: root.warningColor
                                opacity: 0.78
                                font.pixelSize: 11
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "OptiScaler overlay"; opacity: 0.65 }
                                Label {
                                    text: root.selectedOptiHotkey.name || "Insert"
                                    font.bold: true
                                    ToolTip.visible: optiHotkeyHover.containsMouse
                                    ToolTip.text: root.selectedOptiHotkey.available === true
                                                  ? ((root.selectedOptiHotkey.raw || "").toLowerCase() === "auto"
                                                     ? "OptiScaler.ini uses auto; current default resolves to Insert."
                                                     : "Read from existing OptiScaler.ini ShortcutKey=" + (root.selectedOptiHotkey.raw || ""))
                                                  : "OptiScaler.ini not found; Insert is only the default."
                                    MouseArea { id: optiHotkeyHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                }
                                RenoButton {
                                    text: "Change hotkey"
                                    enabled: root.selectedOptiHotkey.available === true
                                    onClicked: root.openHotkeyDialog("opti")
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                visible: root.overlayHotkeyConflict()
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "⚠ Overlay hotkey conflict with REFramework: both are set to " + (root.selectedOptiHotkey.name || "Insert") + "."
                                color: root.warningColor
                                font.pixelSize: 11
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "OptiScaler proxy"; opacity: 0.65 }
                                RenoComboBox {
                                    id: optiProxyChoice
                                    Layout.fillWidth: true
                                    model: ["Recommended", "Auto", "dxgi.dll", "winmm.dll", "d3d12.dll", "dbghelp.dll", "version.dll", "wininet.dll", "winhttp.dll", "OptiScaler.asi (advanced)"]
                                    currentIndex: 0
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Integration method"; opacity: 0.65 }
                                RenoComboBox {
                                    id: optiMethodChoice
                                    Layout.fillWidth: true
                                    model: ["Recommended", "Separate proxies", "Load ReShade via OptiScaler", "OptiScaler plugins folder"]
                                    currentIndex: 0
                                }
                            }

                            Label {
                                visible: root.optiChoicesDirty
                                text: "● Pending changes — press Apply to save and apply this integration setup."
                                color: root.warningColor
                                opacity: 0.9
                            }
                            Label {
                                visible: (root.selectedOptiInlineStatus.text || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Last action: " + (root.selectedOptiInlineStatus.text || "")
                                color: root.statusColor(root.selectedOptiInlineStatus)
                                opacity: 0.9
                                font.pixelSize: 11
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "Preview"; font.bold: true; opacity: 0.8 }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.selectedOptiPreview.summary || ""; opacity: 0.65 }
                            Repeater {
                                model: root.selectedOptiPreview.changes || []
                                delegate: Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "• " + modelData; opacity: 0.72 }
                            }
                            Label {
                                visible: (root.selectedOptiPreview.wineOverrides || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Suggested Wine/Proton override (not applied by Reno119)"
                                font.bold: true
                                opacity: 0.66
                                font.pixelSize: 11
                            }
                            Label {
                                visible: (root.selectedOptiPreview.wineOverrides || "").length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.selectedOptiPreview.wineOverrides || ""
                                font.family: "monospace"
                                opacity: 0.85
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton {
                                    text: "Apply"
                                    // Pending proxy/method choices must always be actionable.  The
                                    // preview can still explain why a particular choice cannot be
                                    // completed, but it must not silently disable Apply.
                                    enabled: root.selectedRow >= 0 &&
                                             (root.optiChoicesDirty ||
                                              (root.selectedOptiPreview.canApply === true &&
                                               (root.selectedOptiPreview.hasFileChanges === true ||
                                                root.selectedOptiAnalysis.drift === true)))
                                    onClicked: {
                                        if (optiIntegration.apply(root.selectedRow,
                                                                  root.optiProxyValue(optiProxyChoice.currentIndex),
                                                                  root.optiMethodValue(optiMethodChoice.currentIndex))) {
                                            root.optiRefreshNonce++
                                            Qt.callLater(root.loadGameChoices)
                                        }
                                    }
                                }
                                RenoButton {
                                    text: root.optiChoicesDirty ? "Discard pending changes" : "Revert last integration change"
                                    enabled: root.selectedRow >= 0 &&
                                             (root.optiChoicesDirty ||
                                              (root.selectedOptiAnalysis.lastTransactionDir || "").length > 0)
                                    onClicked: {
                                        // If the user has only changed the dropdowns, Revert means
                                        // discard those pending choices.  No filesystem transaction
                                        // has happened yet, so just reload the last applied state.
                                        if (root.optiChoicesDirty) {
                                            root.loadGameChoices()
                                            return
                                        }
                                        if (optiIntegration.revert(root.selectedRow)) {
                                            root.optiRefreshNonce++
                                            Qt.callLater(root.loadGameChoices)
                                        }
                                    }
                                }
                                RenoButton {
                                    text: "Save working setup"
                                    enabled: root.selectedOptiAnalysis.detected === true
                                    onClicked: {
                                        if (optiIntegration.saveWorkingConfiguration(root.selectedRow))
                                            root.optiRefreshNonce++
                                    }
                                }
                                RenoButton {
                                    text: "Restore working setup"
                                    enabled: root.selectedOptiAnalysis.workingSaved === true
                                    onClicked: {
                                        if (optiIntegration.restoreWorkingConfiguration(root.selectedRow))
                                            root.optiRefreshNonce++
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton { text: "Re-analyze"; onClicked: root.optiRefreshNonce++ }
                                RenoButton {
                                    visible: root.selectedGame.source !== "Steam" && (root.selectedOptiPreview.wineOverrides || "").length > 0
                                    text: "Copy suggested Wine override"
                                    onClicked: installer.copyText(root.selectedOptiPreview.wineOverrides || "")
                                }
                                RenoButton {
                                    visible: root.selectedGame.source === "Steam" && (root.selectedOptiPreview.steamLaunchOptions || "").length > 0
                                    text: "Copy suggested Steam launch option"
                                    onClicked: installer.copyText(root.selectedOptiPreview.steamLaunchOptions || "")
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label { text: "Recent integration history"; font.bold: true; opacity: 0.7 }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.integrationHistoryText(); opacity: 0.52; font.pixelSize: 11 }
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: ""
                        visible: root.selectedRow >= 0
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            CollapsibleHeader {
                                titleText: "Game notes"
                                summaryText: gameNotesEditor.saveStatus.indexOf("Could not save") === 0
                                             || gameNotesEditor.saveStatus.indexOf("Unsaved") === 0
                                             ? gameNotesEditor.saveStatus
                                             : gameNotesEditor.text.length > 0 ? "Notes saved · click to expand or collapse" : "Add HDR settings, launch options or known issues"
                                expanded: root.notesExpanded
                                onToggleRequested: {
                                    const next = Object.assign({}, root.notesExpandedByGame)
                                    next[root.selectedGameKey] = !root.notesExpanded
                                    root.notesExpandedByGame = next
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.notesExpanded
                                enabled: visible
                                ScrollView {
                                    id: gameNotesScroll
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 145
                                    clip: true
                                    RenoTextArea {
                                        id: gameNotesEditor
                                        property bool loading: true
                                        property string noteKey: ""
                                        property string saveStatus: ""
                                        width: gameNotesScroll.availableWidth
                                        height: Math.max(gameNotesScroll.availableHeight, implicitHeight)
                                        enabled: noteKey.length > 0
                                        placeholderText: "Working HDR settings, launch options, known issues…"
                                        textFormat: TextEdit.PlainText
                                        wrapMode: TextEdit.Wrap
                                        selectByMouse: true
                                        backgroundColor: root.controlBg
                                        foregroundColor: root.textColor
                                        borderColor: root.dividerColor
                                        placeholderColor: root.mutedTextColor
                                        selectionColor: root.selectedBg
                                        onTextChanged: root.saveGameNotes()
                                    }
                                }
                                RowLayout {
                                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: gameNotesEditor.saveStatus; opacity: 0.7 }
                                    RenoButton { text: "Save notes"; onClicked: root.saveGameNotes() }
                                }
                            }
                        }
                    }
                    GroupBox {
                        title: "Working setup"
                        visible: root.selectedRow >= 0
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.setupVerification.text || "Not marked as tested"; color: root.setupVerification.verified === true ? root.successColor : root.mutedTextColor }
                            RowLayout {
                                RenoButton { text: "Mark verified working"; enabled: !installer.busy; onClicked: { installer.markVerified(root.selectedRow); root.verificationNonce++ } }
                                RenoButton { text: "Recheck setup"; enabled: !installer.busy; onClicked: root.verificationNonce++ }
                            }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "Mark after testing in-game. Recheck compares component and configuration files; it does not test the game."; opacity: 0.6 }
                        }
                    }

                    GroupBox {
                        visible: root.selectedRow >= 0
                        title: ""
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            CollapsibleHeader {
                                titleText: "Recovery and troubleshooting"
                                summaryText: "Recovery copies, reports and unavailable-action explanations"
                                expanded: root.toolsExpanded
                                onToggleRequested: root.toolsExpanded = !root.toolsExpanded
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.toolsExpanded
                            RowLayout {
                                Layout.fillWidth: true
                                RenoButton {
                                    text: "Open recovery folder"
                                    enabled: root.selectedRecoveryFolder.length > 0
                                    onClicked: installer.openRecoveryFolder(root.selectedRow)
                                }
                                RenoButton {
                                    text: "Troubleshooting report…"
                                    onClicked: {
                                        troubleshootingDialog.rawReport = root.diagnosticsReport(false)
                                        redactReport.checked = true
                                        troubleshootingDialog.open()
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.selectedRecoveryFolder.length > 0 ? "Latest retained copy: " + root.selectedRecoveryFolder : "No recovery copy exists for this game yet."
                                textFormat: Text.PlainText
                                wrapMode: Text.WrapAnywhere
                                opacity: 0.7
                            }
                            Label { text: "Recover a saved copy"; font.bold: true }
                            RenoComboBox {
                                id: recoveryChoice
                                Layout.fillWidth: true
                                model: root.recoveryCopies
                                textRole: "label"
                            }
                            RenoButton {
                                text: "Review recovery…"
                                enabled: !installer.busy && recoveryChoice.currentIndex >= 0 && recoveryChoice.currentIndex < root.recoveryCopies.length
                                onClicked: {
                                    recoveryDialog.gameRow = root.selectedRow
                                    recoveryDialog.recoveryPath = root.recoveryCopies[recoveryChoice.currentIndex].path
                                    recoveryDialog.review = installer.recoveryPreview(root.selectedRow, recoveryDialog.recoveryPath)
                                    recoveryDialog.open()
                                }
                            }
                            Label { visible: root.recoveryCopies.length === 0; text: "No addon or INI recovery copies yet."; opacity: 0.7 }
                            CollapsibleHeader {
                                titleText: "Unavailable actions"
                                summaryText: "Why an action is disabled"
                                expanded: root.actionsExpanded
                                onToggleRequested: root.actionsExpanded = !root.actionsExpanded
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.actionsExpanded
                            Repeater {
                                id: blockedActions
                                model: root.unavailableActions()
                                Label {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    text: "• " + modelData
                                    color: root.mutedTextColor
                                    font.pixelSize: 11
                                }
                            }
                            }
                            }
                        }
                    }

                    Rectangle {
                        id: restoreCard
                        visible: root.selectedRow >= 0
                        Layout.fillWidth: true
                        radius: 8
                        color: root.controlBg
                        border.width: 1
                        border.color: root.dividerColor
                        implicitHeight: restoreCardLayout.implicitHeight + 2

                        ColumnLayout {
                            id: restoreCardLayout
                            x: 1
                            y: 1
                            width: Math.max(0, parent.width - 2)
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 58
                                radius: 7
                                color: restoreHeaderMouse.containsMouse ? root.hoverBg : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Backups, history and diagnostics"
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.restoreCollapsedSummary()
                                            opacity: 0.62
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Label {
                                        text: root.restoreExpanded ? "▴" : "▾"
                                        font.pixelSize: 18
                                        opacity: 0.72
                                    }
                                }

                                MouseArea {
                                    id: restoreHeaderMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.setRestoreExpanded(!root.restoreExpanded)
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                implicitHeight: root.restoreExpanded ? restoreBody.implicitHeight + 20 : 0
                                clip: true
                                enabled: root.restoreExpanded
                                opacity: root.restoreExpanded ? 1 : 0
                                Behavior on implicitHeight { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 120 } }

                                ColumnLayout {
                                    id: restoreBody
                                    x: 10
                                    y: 10
                                    width: Math.max(0, parent.width - 20)
                                    spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                opacity: 0.68
                                text: "General ReShade/RenoDX/REFramework backups remain separate from OptiScaler integration transactions. Choose a specific restore point below; only Reno119-managed files are cleaned up when restoring an older snapshot."
                            }
                            RowLayout {
                                RenoButton {
                                    text: "Restore last component backup"
                                    enabled: !installer.busy && (root.selectedBackupHistory || []).length > 0
                                    onClicked: installer.restoreLastBackup(root.selectedRow)
                                }
                                RenoButton { text: "Library diagnostics…"; enabled: root.selectedRow >= 0; onClicked: libraryDiagnosticsPopup.openFor(root.selectedRow) }
                                RenoButton { text: "Copy troubleshooting report"; enabled: !installer.busy; onClicked: installer.copyText(root.diagnosticsReport()) }
                                RenoButton { text: "Export diagnostics…"; enabled: !installer.busy; onClicked: root.openSavePicker(diagnosticsSaveDialog, "diagnosticsExport", "Export Reno119 diagnostics", "reno119-diagnostics.txt", "") }
                                RenoButton { text: "Open executable folder"; enabled: (root.selectedGame.exePath || "").length > 0; onClicked: installer.openGameFolder(root.selectedRow) }
                                Item { Layout.fillWidth: true }
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "ReShade / RenoDX restore points"; font.bold: true; opacity: 0.78 }
                            Label {
                                visible: (root.selectedOtherBackupHistory || []).length === 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "No ReShade/RenoDX restore points are available for this game yet."
                                opacity: 0.52
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: Math.min((root.selectedOtherBackupHistory || []).length, 8)
                                delegate: Rectangle {
                                    property var backupEntry: root.selectedOtherBackupHistory[index] || ({})
                                    Layout.fillWidth: true
                                    implicitHeight: backupHistoryRow.implicitHeight + 12
                                    radius: 6
                                    color: root.controlBg
                                    border.width: 1
                                    border.color: root.dividerColor
                                    RowLayout {
                                        id: backupHistoryRow
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 10
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                Layout.fillWidth: true
                                                text: backupEntry.label || "Backup"
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: (backupEntry.createdDisplay || "") + " · " + (backupEntry.fileCount || 0) + " file(s)" + (backupEntry.externalTakeover === true ? " · takeover backup" : "")
                                                opacity: 0.52
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                        RenoButton {
                                            text: "Restore"
                                            enabled: !installer.busy
                                            onClicked: {
                                                root.pendingBackupRestoreRow = root.selectedRow
                                                root.pendingBackupRestorePath = backupEntry.path || ""
                                                root.pendingBackupRestoreLabel = (backupEntry.label || "Backup") + " — " + (backupEntry.createdDisplay || "")
                                                root.pendingBackupRestoreKind = "component"
                                                restoreBackupDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                            Label {
                                visible: (root.selectedOtherBackupHistory || []).length > 8
                                text: "Showing the 8 most recent of " + root.selectedOtherBackupHistory.length + " ReShade/RenoDX restore points."
                                opacity: 0.48
                                font.pixelSize: 10
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "REFramework restore points"; font.bold: true; opacity: 0.76 }
                            Label {
                                visible: (root.selectedReFrameworkBackupHistory || []).length === 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "No REFramework restore points are available for this game yet."
                                opacity: 0.52
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: Math.min((root.selectedReFrameworkBackupHistory || []).length, 6)
                                delegate: Rectangle {
                                    property var backupEntry: root.selectedReFrameworkBackupHistory[index] || ({})
                                    Layout.fillWidth: true
                                    implicitHeight: reframeworkBackupRow.implicitHeight + 12
                                    radius: 6
                                    color: root.controlBg
                                    border.width: 1
                                    border.color: root.dividerColor
                                    RowLayout {
                                        id: reframeworkBackupRow
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 10
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                Layout.fillWidth: true
                                                text: backupEntry.label || "REFramework backup"
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: (backupEntry.createdDisplay || "") + " · " + (backupEntry.fileCount || 0) + " file(s)" + (backupEntry.externalTakeover === true ? " · takeover backup" : "")
                                                opacity: 0.52
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                        RenoButton {
                                            text: "Restore"
                                            enabled: !installer.busy
                                            onClicked: {
                                                root.pendingBackupRestoreRow = root.selectedRow
                                                root.pendingBackupRestorePath = backupEntry.path || ""
                                                root.pendingBackupRestoreLabel = (backupEntry.label || "REFramework backup") + " — " + (backupEntry.createdDisplay || "")
                                                root.pendingBackupRestoreKind = "component"
                                                restoreBackupDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                            Label {
                                visible: (root.selectedReFrameworkBackupHistory || []).length > 6
                                text: "Showing the 6 most recent of " + root.selectedReFrameworkBackupHistory.length + " REFramework restore points."
                                opacity: 0.48
                                font.pixelSize: 10
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: root.dividerColor }
                            Label { text: "OptiScaler integration restore points"; font.bold: true; opacity: 0.72 }
                            Label {
                                visible: (root.selectedOptiRestorePoints || []).length === 0
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "No OptiScaler integration transactions are available to restore yet."
                                opacity: 0.52
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: Math.min((root.selectedOptiRestorePoints || []).length, 6)
                                delegate: Rectangle {
                                    property var restoreEntry: root.selectedOptiRestorePoints[index] || ({})
                                    Layout.fillWidth: true
                                    implicitHeight: optiRestoreRow.implicitHeight + 12
                                    radius: 6
                                    color: root.controlBg
                                    border.width: 1
                                    border.color: root.dividerColor
                                    RowLayout {
                                        id: optiRestoreRow
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 10
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                Layout.fillWidth: true
                                                text: restoreEntry.label || "Integration restore point"
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: restoreEntry.createdDisplay || ""
                                                opacity: 0.52
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                        RenoButton {
                                            text: "Restore"
                                            enabled: !installer.busy
                                            onClicked: {
                                                root.pendingBackupRestoreRow = root.selectedRow
                                                root.pendingBackupRestorePath = restoreEntry.snapshotDir || ""
                                                root.pendingBackupRestoreLabel = (restoreEntry.label || "Integration restore point") + " — " + (restoreEntry.createdDisplay || "")
                                                root.pendingBackupRestoreKind = "integration"
                                                restoreBackupDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                            Label {
                                visible: (root.selectedOptiRestorePoints || []).length > 6
                                text: "Showing the 6 most recent of " + root.selectedOptiRestorePoints.length + " integration restore points."
                                opacity: 0.48
                                font.pixelSize: 10
                            }
                            Label { text: "Recent integration actions"; font.bold: true; opacity: 0.68 }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.integrationHistoryText()
                                opacity: 0.48
                                font.pixelSize: 10
                            }
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: installer.busy || installer.status !== "Ready" || optiIntegration.status !== "Ready"
                        Layout.fillWidth: true
                        implicitHeight: statusColumn.implicitHeight + 20
                        radius: 6
                        color: root.statusBg
                        ColumnLayout {
                            id: statusColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: installer.status !== "Ready" ? installer.status : optiIntegration.status }
                            RenoProgressBar {
                                Layout.fillWidth: true
                                visible: installer.busy
                                value: installer.progress
                                trackColor: root.dividerColor
                                fillColor: root.buttonHighlight
                            }
                        }
                    }

                }
            }
        }
    }
}
