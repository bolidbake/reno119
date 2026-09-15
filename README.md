# Reno119 v0.23.4


## v0.23.4 — Preserve original status-badge colors

- Compact `StatusBadge` indicators now keep Reno119's original semantic palette under Material and Noctalia themes instead of inheriting the generated accent roles.
- Dark/Pure Black themes use the familiar green `#9be9a8`, yellow `#e7d57f`, blue `#b9c6ff`, and muted `#a8adb7` badge colors.
- Light role themes use the original darker semantic variants for readable contrast.
- Health chips, recommendation states, diagnostics, buttons, surfaces, and the rest of the Material/Noctalia UI remain theme-driven.

## v0.23.3 — Material/Noctalia contrast and Pure Black mapping

- Detects Material palettes whose `background` and `surface` roles are true black and exposes that as a Pure Black role-theme state. This matches Noctalia's Pure Black output without adding a Noctalia-specific config dependency.
- Pure Black Material/Noctalia themes now keep the window and major panels on the supplied black `background`/`surface` roles. `surface_container_*` roles are reserved for controls, hover/elevation, and other layered elements instead of tinting the entire UI.
- Fills out the Qt Quick Controls palette (`light`, `midlight`, `mid`, `dark`, `shadow`, links, tooltips, and placeholder text) from Material roles so Breeze/system palette colors cannot bleed into role-based themes.
- Removes extra opacity from the main Recommended Setup explanatory/detail text when a role-based theme is active, because `on_surface_variant` already encodes the intended Material contrast. Reusable health/collapsible labels also receive slightly stronger role-theme contrast.
- Adds a regression test using a Noctalia-style `#000000` surface/background palette to verify Pure Black detection while preserving the supplied container and accent roles.

## v0.23.2 — readable workspace state

- Moves the large Qt-serialized `workspace/viewState=@Variant(...)` value out of `reno119.conf` into a dedicated, human-readable `~/.config/reno119/workspace-state.json` (or the equivalent XDG config path).
- Existing v0.23.1-and-older workspace state is migrated automatically on first launch. The legacy QSettings key is removed only after the JSON file is present or has been written successfully.
- Workspace writes use `QSaveFile` so scroll/selection/expansion state is updated atomically. The JSON has an explicit `reno119-workspace-state` format marker and schema version. Default/false values and zero scroll positions are pruned so the file stays compact and readable.
- Portable configuration backups move to schema v3 and carry workspace state as a separate `workspaceState` object instead of embedding a QVariant inside the settings map. Imports remain compatible with schema v1/v2 backups and migrate their old `workspace/viewState` value directly into the JSON file.
- The regular config remains responsible for durable application/game settings; workspace UI state no longer recreates a `[workspace]` section.

## v0.23.1

- Fix `build.sh` so source/build paths are anchored to the script directory. Running `/path/to/reno119/build.sh` from another working directory now always configures the Reno119 source tree and writes only to `reno119/build/`.
- Prevent accidental Qt/QML scanning of unrelated sibling directories caused by an incorrect current working directory.
- Update the bundled Noctalia template guidance to use `~/.config/noctalia/templates.toml`.


## v0.23.0 — Material 3 and Noctalia theming

Reno119 now has a role-based Material 3 theme layer in addition to the existing System, Reno119 Dark, and AMOLED Black themes. The built-in Material 3 mode works without external files and can be overridden by a full or partial Material role map at `~/.config/reno119/material-theme.json` (or the equivalent XDG config path). Missing roles fall back to Reno119's built-in Material 3 palette, so generators only need to emit the roles they want to change. CamelCase, snake_case, and legacy Noctalia-style `mPrimary` role names are accepted.

A dedicated **Noctalia** theme mode watches `~/.config/reno119/noctalia-theme.json` and applies changes live. Reno119 does not edit Noctalia configuration or make network requests for theming; Noctalia remains responsible for rendering the palette. The source archive includes a Noctalia v5 user template at `data/noctalia/reno119-theme.json.template` plus `data/noctalia/reno119-template.toml`, which renders Noctalia's current Material roles into Reno119's theme file.

Example Noctalia setup:

```bash
config_root="${XDG_CONFIG_HOME:-$HOME/.config}"
mkdir -p "$config_root/noctalia/templates" "$config_root/reno119"
cp data/noctalia/reno119-theme.json.template "$config_root/noctalia/templates/"
```

Then add the contents of `data/noctalia/reno119-template.toml` to the active Noctalia configuration. Re-render Noctalia templates once, then select **Noctalia** in Reno119 → Settings → Appearance. Subsequent palette changes are picked up from the generated file automatically.

For other Material generators, `data/material/reno119-theme.example.json` documents the generic JSON schema. `mode` may be `dark` or `light`, and the `colors` object uses Material role names such as `primary`, `surface`, `on_surface`, `surface_container`, `outline`, and `error`.

A new `ThemeManagerTests` QtTest target covers built-in fallback, partial Material overrides, light/dark mode handling, Noctalia-generated palette loading, legacy role aliases, and invalid-file fallback. Source-level validation only; compilation is left to the user.

**v0.22.11 — OptiScaler planning/analysis implementation split:** Completes the planned 0.22 structural pass with one more justified large-file boundary. Moves OptiScaler detection analysis, integration-plan construction, preview generation, and applied-choice reporting into `OptiScalerIntegrationPlanning.cpp`. The public API and all moved method bodies are unchanged; `apply()`/`fixSetup()`/`revert()` remain in the main implementation and consume the same `Plan` results. Snapshot/restore, config/hotkey, status, installer, QML, and network behavior are untouched. The v0.22.10 OptiScaler regression target includes the new planning implementation. Source-only update; compilation left to the user.

**v0.22.10 — regression and dead-code audit:** Pauses structural movement for a focused verification pass across the recent QML, GameModel, and OptiScaler splits. Adds `OptiScalerIntegrationTests.cpp` covering current `[Menu]` hotkey parsing/writes, legacy root-level hotkey compatibility, restore-point/history metadata exposure, and rejection of restore paths outside Reno119's per-game history. The audit found no dead `OptiScalerIntegration` methods; it removes only the now-unused `QJsonArray` and `QStandardPaths` includes left in the main implementation after v0.22.9. Existing GameModel and RenoDX regression tests remain unchanged. No runtime behavior changes are intended. Source-only update; compilation left to the user.

**v0.22.9 — OptiScaler snapshot/restore implementation split:** Continues the behavior-preserving OptiScaler cleanup by moving transaction snapshot creation/restoration, saved-working-configuration handling, restore-point listing/restoration, and integration-history reading into `OptiScalerIntegrationSnapshots.cpp`. The public `OptiScalerIntegration` API is unchanged; `apply()`/`revert()` still own transaction decisions and call the same snapshot helpers. No integration decisions, file formats, restore-path validation, UI, installer, or network behavior changes are intended. Source-only update; compilation left to the user.

**v0.22.8 — OptiScaler config/hotkey implementation split:** Begins the OptiScaler structural cleanup without changing the public `OptiScalerIntegration` API or runtime behavior. Generic OptiScaler INI read/write handling and overlay-hotkey parsing/application now live in `OptiScalerIntegrationConfig.cpp`; analysis, planning, apply/revert transactions, snapshots, restore points, status handling, and integration state remain in `OptiScalerIntegration.cpp`. Existing method bodies and hotkey compatibility behavior are preserved. Source-only update; compilation left to the user.

**v0.22.7 — GameModel library-state split:** Continues the behavior-preserving backend cleanup by moving favorites, hidden-state handling, bulk selection, and bulk library actions into `GameModelLibraryState.cpp`. The public `GameModel` API, settings keys, sorting/filtering behavior, bulk rescan/override clearing, and visible library behavior are unchanged. Scanning, cache refresh, executable/prefix/API overrides, custom-program/duplicate management, update-status handling, installer behavior, and network behavior are untouched. The main application and existing GameModel regression target both include the new implementation file. Source-only update; compilation left to the user.

**v0.22.6 — GameModel library-management split:** Continues the behavior-preserving backend cleanup by moving custom-program creation/edit/removal, nickname/artwork overrides, and duplicate/alternate-install management into `GameModelLibraryManagement.cpp`. The public `GameModel` API, settings keys, duplicate matching/linking behavior, artwork handling, and custom-program workflows are unchanged. The main application and existing GameModel regression target both include the new implementation file. No scanning, cache-refresh, filtering, update-status, installer, or network behavior changes are intended. Source-only update; compilation left to the user.

**v0.22.5 — GameModel implementation split:** Begins the backend structural cleanup without changing the public `GameModel` API or runtime behavior. Search/filter/sort handling now lives in `GameModelFiltering.cpp`, while incremental Update Center status handling lives in `GameModelUpdateStatus.cpp`. `GameModel.cpp` retains scanning, cache, diagnostics, overrides, custom-program, duplicate, bulk-selection, and configuration logic. The existing GameModel update-status regression target builds all three implementation files so the row-level update versus Updates-filter reset behavior remains covered. Source-only update; compilation left to the user.

**v0.22.3 — recovery/tweak preview extraction:** Continues the behavior-preserving `Main.qml` cleanup by moving the recovery-review dialog into `RenoRecoveryDialog.qml` and the RenoDX tweak/original-file preview dialog into `RenoTweakPreviewDialog.qml`. Existing `recoveryDialog` and `tweakPreviewDialog.reviewGame()` call sites remain intact. Recovery still uses the same preview token and restore method, with verification refresh routed back to Main.qml through a completion signal. Tweak preview still clears acknowledgement on refresh and calls the same apply/restore backend methods with the same token and acknowledgement state. No installer, recovery implementation, matching, Update Center, network, or backend behavior changes are intended. Source-only update; compilation left to the user.




## v0.22.14

- Consolidates setup health into the Recommended Setup card, making it the primary place to see ReShade, RenoDX, REFramework, OptiScaler, and integrity state.
- Removes the duplicate standalone **Fix setup** controls. OptiScaler repair now lives on the OptiScaler Recommended Setup row; the detailed integration section remains for diagnostics, re-scan, and advanced controls.
- Keeps the detailed component sections intact for manual/advanced management while reducing duplicated top-level health and repair UI.

## v0.22.13

- Adds a guided **Set up recommended** action to Recommended Setup. It serializes only unambiguous managed work instead of launching installers concurrently.
- Automatic work is intentionally conservative: managed/recommended ReShade, exact dedicated RenoDX matches, supported REFramework, and deterministic OptiScaler repair can be queued. External takeovers, partial RenoDX matches, generic fallbacks, and managed tweak previews remain explicit review actions.
- The guided run stays tied to the original game row/app ID and stops instead of continuing if that library row changes during the operation.

## v0.22.12

- Makes Recommended Setup directly actionable: supported rows can now expose Install, Update, Take over, Review match, Preview, or Fix actions without scrolling to the advanced component sections.
- Reuses the existing external-install and RenoDX partial-match confirmation dialogs; this release does not weaken any takeover or matching safeguards.
- Recommended ReShade actions use the recommended channel, while managed tweaks still open their existing preview/review flow before any file changes.

## v0.22.4

- Removes the duplicate OptiScaler clipboard action shown for Steam games.
- Steam titles now show only **Copy suggested Steam launch option** (including `%command%`).
- Non-Steam/Wine titles continue to show **Copy suggested Wine override**.
- No installer, integration, or backend behavior changes.


**v0.22.2 — Update Preview and Update History extraction:** Continues the behavior-preserving `Main.qml` cleanup by moving the Update Center review dialog into `RenoUpdatePreviewDialog.qml` and the per-game rollback-history dialog into `RenoUpdateHistoryDialog.qml`. The existing `updatePreviewDialog`/`updateHistoryDialog` IDs and `openFor()` call sites remain intact. Preview still re-evaluates targets immediately before queueing updates and refuses to start when targets changed; retry filtering, RenoDX partial-match confirmation text, queue calls, and busy/scanning guards are unchanged. Update History still reads the same installer snapshots and routes Rollback back through Main.qml's existing Recovery state and restore confirmation. The main Update Center popup, installer backend, recovery implementation, matching, and network behavior are unchanged. Source-only update; compilation left to the user.

**v0.22.1 — custom-program and troubleshooting UI extraction:** Continues the behavior-preserving `Main.qml` cleanup by moving the custom-program add/edit popup into `RenoCustomProgramPopup.qml` and the selectable/redactable troubleshooting-report preview into `RenoTroubleshootingDialog.qml`. The existing `customProgramPopup.openAdd()`/`openEdit()` and `troubleshootingDialog.rawReport`/`open()` call sites remain intact. Custom-program file-picker results now pass through explicit popup setter methods instead of reaching into child TextField IDs from `Main.qml`; add/edit validation, GameModel calls, report redaction/copy behavior, and fallback Qt file dialogs are unchanged. No backend, installer, Update Center, matching, or network behavior changes are intended. Source-only update; compilation left to the user.

**v0.22.0 — duplicate-installs popup extraction:** Starts the 0.22 series with another behavior-preserving `Main.qml` cleanup. Moves the self-contained duplicate/alternate-install management popup into `RenoDuplicatePopup.qml` while preserving the existing `duplicatePopup` ID and `openFor()`/reload behavior used by the game context menu. Linking, unlinking, duplicate suggestions, manual selection, nickname reselection, and all `GameModel` duplicate-detection logic remain unchanged. No installer, Update Center, matching, network, or backend behavior changes are intended. Source-only update; compilation left to the user.

**v0.21.33 — nickname popup extraction:** Moves the self-contained game-nickname popup out of `Main.qml` into `RenoNicknamePopup.qml` while preserving the existing `nicknamePopup` ID, `openFor()`, `reselect()`, and rename behavior. Existing context-menu, duplicate-link, bulk-rescan, and details-panel call sites remain unchanged. The extracted popup reuses the same Reno119 button visuals through `RenoDialogButton`; no installer, model, matching, Update Center, network, or duplicate-detection behavior changes are intended. Source-only update; compilation left to the user.

**v0.21.32 — small QML structure cleanup:** Moves the wheel-safe `RenoComboBox` out of `Main.qml` into a reusable QML component without changing any existing ComboBox call sites or scroll behavior. Extracts the RenoDX non-exact/partial-match confirmation and overlay-hotkey dialogs into focused components while keeping their existing root IDs, installer actions, selection logic, confirmation behavior, and visual treatment. No installer, matching, Update Center, recovery, or network behavior changes are intended. Source-only update; compilation left to the user.

**v0.21.31 — clean network/QML shutdown:** Adds an explicit shutdown phase before the QML engine is destroyed. Reno119-owned network managers now disconnect and abort outstanding replies before teardown, preventing late reply callbacks from reading closed SSL devices. The QML root object is deferred-deleted and flushed while deferred-delete processing is still available, cancelling outstanding ListView/Repeater incubation before `QQmlApplicationEngine` destruction. This targets shutdown-only `QIODevice::read (QSslSocket): device not open` and `items in the process of being created at engine destruction` warnings without changing normal runtime/network behavior. Source-only update; compilation left to the user.

**v0.21.30 — minimal-network cache audit:** Reduces Reno119's routine network footprint without changing installer decisions. Normal ReShade/REFramework/OptiScaler release metadata and the RenoDX catalog now reuse fresh results for 6 hours; Update Center's explicit Refresh still forces live metadata. RHI manifest data is cached for 24 hours together with downloaded Engine.ini profiles, so normal launches no longer refetch the manifest and every referenced engine file. REFramework's supplemental supported-title README is cached for 7 days. Successful PCGamingWiki page resolutions persist for 30 days. Steam cover/banner misses are negatively cached for 7 days so unavailable artwork does not retry up to four CDN URLs every launch; explicit artwork refresh bypasses that negative cache. Update Center now requests only the metadata sources needed by the installed managed components in the current batch. Direct Latest ReShade installs reuse the shared release metadata cache and write successful direct lookups back into it. Installer payload caches remain unchanged and continue to be shared across games. Source-only update; compilation left to the user.

**v0.21.29 — shared REFramework release metadata:** Update Center and direct REFramework installs now share the cached latest-nightly metadata, including the actual `REFramework.zip` asset URL. A batch update performs the GitHub latest-release lookup once, then every REFramework game reuses that version/asset metadata; the first game downloads/extracts the payload and later games continue reusing the existing versioned `dinput8.dll` cache. Fresh metadata is also reused by later direct installs, while stale or legacy cache entries without the asset URL are refreshed automatically. Source-only update; compilation left to the user.

**v0.21.28 — simpler partial-match wording:** Renames the user-facing RenoDX `Matched without subtitle` label to `Partial match`. Matching logic, installer confirmation rules, and the underlying resolver method remain unchanged. Source-only update; compilation left to the user.

**v0.21.27 — simpler RenoDX match wording:** Simplifies user-facing RenoDX resolver labels without changing any matching or confirmation behavior. `Exact title` is now shown as `Exact match`, `Subtitle-stripped exact` as `Matched without subtitle`, and `Prefix` as `Close title match`. Internal matcher methods and safety rules remain unchanged. Source-only update; compilation left to the user.

**v0.21.26 — Update Center regression coverage and first Main.qml extraction:** Adds optional QtTest coverage that locks in the v0.21.25 incremental update-status behavior: normal library views update one row without resetting the model, while the explicit Updates filter still rebuilds membership as update availability changes. Extracts six low-risk confirmation/preview dialogs from `Main.qml` into reusable QML components while keeping their existing IDs, accepted/rejected handlers, installer actions, and visual button treatment. State-heavy Update Center, diagnostics, recovery-review, tweak-preview, and hotkey UI remain in `Main.qml` for a later cleanup pass. No intended runtime/UI behavior changes. Source-only update; compilation left to the user.

**v0.21.25 — stable Update Center covers and Breeze TextArea cleanup:** Update-status changes no longer reset the entire visible game model unless the Library is explicitly using the Updates filter. Normal All/Favorites/component views now receive row-level update-role changes, preventing unchanged game delegates and cover art from being destroyed/recreated during Update Center checks. Replaces the four Qt Quick Controls `TextArea` usages with a local `RenoTextArea` built on QtQuick `TextEdit`, avoiding KDE Breeze `TextArea.qml` type-assignment warnings while preserving read-only reports/previews, selection/wrapping, monospace diagnostics, and editable game notes. Source-only update; compilation left to the user.

**v0.21.24 — style-independent progress bars:** Replaces Reno119's two Qt Quick Controls `ProgressBar` instances with a small built-in `RenoProgressBar` QML component. This avoids repeated KDE Breeze `ProgressBar.qml` null-inset warnings (`Cannot read property top/bottom of null`) during Update Center and installer activity while preserving both indeterminate update-check animation and determinate installer progress. No update logic or workflow behavior is changed. Source-only update; compilation left to the user.

**v0.21.23 — single-pass Update Center checks:** Removes the redundant preliminary cached per-game scan from `checkAllUpdates()`. Reno119 still reuses fresh cached release/catalog metadata through the existing fetch/cache layer, but each logical Update Center check now evaluates the game library only once. This prevents the progress counter from restarting and making manual checks or the automatic post-update refresh appear to run twice. The automatic post-update refresh remains in place as one single-pass verification. Source-only update; compilation left to the user.

**v0.21.22 — behavior-preserving code cleanup:** Splits the oversized `InstallerManager.cpp` implementation into focused ReShade, RenoDX, REFramework, Update Center, diagnostics/recovery, and shared/common implementation files while keeping the same `InstallerManager` QObject API and state ownership. Moves file-local INI/hotkey/tweak helpers into a private internal utility module. CMake's project version is now the C++ version source used by application metadata and network user agents, and the QML title reads Qt's application version instead of duplicating it. No intended runtime/UI behavior changes. Source-only update; compilation left to the user.

**v0.21.21 — RenoDX match transparency and safeguards:** Reno119 now exposes the catalog title, addon file, and resolution method used for dedicated RenoDX matches. Exact full-title matches continue normally; subtitle-stripped exact and unique-prefix matches require explicit confirmation before a direct install, and Update Center calls them out in its existing review screen before starting. Adds dedicated RenoDX matcher regression tests covering the Peace Walker/MGS Delta collision, exact matches, subtitle-stripped exact matches, safe prefix matches, and ambiguous prefix rejection. Recommended Setup now refers generically to live compatibility metadata instead of exposing RHI branding. Source-only update; compilation left to the user.

**v0.21.20 — build workflow/QML scanner cleanup:** Restores release archives to a single stable top-level `reno119/` source directory instead of a versioned work folder or completely flat archive. Normal CMake artifacts remain in the existing `build/` subdirectory. Disables Qt's automatic QML import scan for Reno119's system-Qt build so unrelated installed QML modules such as Quickshell are not discovered and spuriously reported as missing link targets. Source-only update; compilation left to the user.

**v0.21.19 — safer RenoDX title matching:** Prevents subtitle-stripped franchise names from participating in fuzzy/prefix RenoDX snapshot matching. Subtitle-stripped names are now accepted only for an exact catalog match, while safe prefix matching uses the full launcher title. This prevents titles such as `Metal Gear Solid: Peace Walker HD` from incorrectly inheriting the dedicated `METAL GEAR SOLID Δ: SNAKE EATER` addon merely because both share the `Metal Gear Solid` prefix. Source-only update; compilation left to the user.

**v0.21.18 — generic game-note wording:** Renames the user-facing RHI note labels to simply **Game note** and removes RHI-specific wording from the collapsible note header/summary. The underlying RHI note source and sanitization remain unchanged. Source-only update; compilation left to the user.

**v0.21.17 — RHI row-reference cleanup fix:** Broadens imported RHI game-note sanitization so current wording such as “Install it from the RE Framework row above.” is removed regardless of REFramework spacing/capitalization, while keeping the useful title-specific requirement or warning. Source-only update; compilation left to the user.

**v0.21.16 — RE Engine REFramework recommendation and RHI-note cleanup:** Treats every locally detected RE Engine title as REFramework-supported without waiting for an external title list. The existing upstream title list remains a fallback when engine detection is unknown. Imported RHI game notes now remove RHI-table navigation text such as instructions to install REFramework from a “row above,” while preserving the useful game-specific guidance. Recommended Setup wording now reflects local RE Engine detection. Source-only update; compilation left to the user.

**v0.21.15 — REFramework hotkey Apply fix:** Fixes the REFramework overlay hotkey editor for current nightlies by using `REFrameworkConfig_MenuKey_V2`, the key generated by current upstream REFramework. Existing legacy configs using bare `MenuKey_V2` remain synchronized for compatibility. The hotkey display now refreshes immediately after a successful Apply. Source-only update; compilation left to the user.

**v0.21.14 — OptiScaler hotkey Apply fix:** Fixes the OptiScaler overlay hotkey editor to read and write `ShortcutKey` from the current upstream `[Menu]` section instead of incorrectly editing a root-level key that modern OptiScaler ignores. Legacy root-level `ShortcutKey` configs remain supported when no `[Menu]` section exists. Written virtual-key values keep the canonical `0xNN` form used by OptiScaler. Source-only update; compilation left to the user.

**v0.21.13 — responsive Update Center scanning:** Update Center now evaluates installed games in small event-loop slices instead of scanning the entire candidate library synchronously on the Qt GUI thread. The cached pass and fresh pass both yield between slices, so the window can repaint and remain interactive while local state, RenoDX targets, cached update metadata, and model status are processed. The v0.21.11 ListView virtualization remains in place. Source-only update; compilation left to the user.

**v0.21.10 — Update Center and backend cleanup:** Removes the redundant lower Suggested Proton/Wine override panel so Recommended setup is the single compact copy surface. Update Center counts now report both actionable component updates and affected games (for example, `4 updates · 3 games`) in the library button and popup badge. Removes unused backend entry points with no remaining QML/test/C++ callers: the old single-game/all-compatible direct update helpers, dedicated launch-option/debug-summary copy helpers, and obsolete REFramework support accessor. Existing preview/review update flow, generic text copy path, debug-summary generation, and model-backed REFramework support state remain unchanged. Source-only update; compilation left to the user.

**v0.21.9 — QML RHI-note compatibility fix:** Removes the invalid `selectByMouse` assignment from the read-only RHI note `Label`, which prevented `Main.qml` from loading on the user's Qt/QML version. The v0.21.8 RHI game-note UI, scope-description removal, and README cleanup remain unchanged. Source-only update; compilation left to the user.

**v0.21.8 — RHI game notes and scope cleanup:** Surfaces RHI `gameNotes` that Reno119 already downloads but previously did not display. Recommended setup shows a concise title-specific preview when one exists, and the RenoDX HDR section exposes the full note in a read-only collapsible panel. Matching uses the original launcher title so per-game nicknames do not break RHI lookup. Removes the obsolete bottom-of-details scope description. Corrects the limitations text to reflect the existing explicit external RenoDX takeover workflow. Source-only update; compilation left to the user.

**v0.21.7 — Recommended setup RenoDX cleanup:** Removes the redundant conditional `RenoDX support` row from Recommended setup. The existing `RenoDX` recommendation/status row remains and continues to show installed state or support resolution as appropriate. No install behavior or RenoDX detection logic changed. Source-only update; compilation left to the user.



**v0.21.6 — Cached update checks:** Persists validated RenoDX catalog and ReShade/REFramework/OptiScaler release metadata in the application cache. Re-evaluates cached targets against current local installation state immediately; sources older than 30 minutes refresh asynchronously. Check now bypasses the freshness interval. Parallel source requests have a 15-second inactivity timeout and a 20-second absolute deadline. Concurrent release lookups share in-flight requests. Failed/invalid responses preserve prior successful data and timestamps, with failure state retained across launches. Update Center shows per-source UTC timestamps and stale component badges; stale entries are excluded from Up to date. RenoDX support details also identify stale catalog information. Cache files contain upstream metadata, not installation ownership. Source review and JavaScript filter/preview checks performed; compilation is left to the user and GUI/network runtime behavior is untested.

**v0.21.5 — RenoDX support clarity:** Distinguishes catalog loading/failure, dedicated support, generic fallback, and no resolved addon. A managed installation whose recorded URL is one of the known generic addons receives a dedicated-support notice when the loaded catalog resolves a dedicated addon. The notice appears in Recommended setup, the RenoDX section/header, and Update Center; it clears naturally after replacing the generic addon. External installations are not guessed from filenames. No automatic installation or additional backup system is introduced. Compilation could not start: CMake/Ninja are absent and dependency installation failed due to environment permissions. Source review only; no GUI runtime validation.

**v0.21.4 — Header cleanup:** Removes the redundant top-right Updates button. The left-panel Update Center button retains its update count and loading text. Source changes only; no build or GUI runtime validation.

**v0.21.3 — Update loading feedback:** Adds a prominent loading card with an animated spinner, indeterminate progress bar, and status text while checking or applying updates. The library Update Center button shows Checking updates during a check. Existing results remain accessible while fetching release information. Source changes only; no build or GUI runtime validation.

**v0.21.2 — Update Center polish:** Adds All installations, Updates available, Up to date, External installs, and Failed filters; a local-time last-check completion timestamp; and a review dialog showing game, component, and installed → available target before starting selected, all-compatible, per-game, or failed-only retry updates. The preview refreshes instead of starting if its targets changed. Unknown targets are excluded from Up to date.

Update batches retain per-component success, failure, and skip results with installer messages, plus totals. Retry queues only failed components and rechecks ownership/update eligibility before each operation. Results and check timestamps last for the current session. Required pre-update snapshot creation or state-recording failures now stop that component before installation. Source and JavaScript logic checks only; no compilation or GUI runtime validation.

**v0.21.1 — Library layout:** Moves the games/programs count above the Update Center, makes the left-panel dropdowns compact and shrinkable, and insets game-row highlights by 20 px per side and 4 px vertically. Source changes only; no build or GUI runtime validation.

**v0.21.0 — Update Center:** Reno119 now has a dedicated Update Center for ReShade, RenoDX, REFramework, and OptiScaler status. Managed ReShade/RenoDX/REFramework installs show installed versus available targets, can be updated per game, as a selected set, or with **Update all compatible**, and are processed sequentially so installer operations never overlap. External installs are clearly informational and are never silently taken over. OptiScaler remains advisory/manual because Reno119 manages only its ReShade/RenoDX coexistence integration, not OptiScaler installation itself.

Every Update Center install creates a forced pre-update recovery snapshot even when the general backup preference is disabled. Update history exposes those snapshots with one-click rollback through the existing Recovery system. Individual targets can be **Skip this version** / **Unskip**, with skip state included in Reno119 metadata backup. Release/source links and available publication timestamps are shown when upstream metadata provides them. An optional read-only startup update check is available in Settings → Integrations; nothing is ever installed automatically.

The header and library controls now expose a compact update count and open the Update Center. The v0.20.11 game-list layout is intentionally frozen: the right-panel-owned widened scrollbar, ReShade/RenoDX/OptiScaler/REF presence badges, and right-click-only favorites are unchanged.

**v0.20.11:** Widens the right-panel-owned game-list scrollbar only toward the left/divider side. Its right edge remains fixed at the same position inside the details-panel margin, while the bar expands from 14 px to 20 px (x = -4 through x = 16) with a wider thumb, making it easier to grab without moving it farther into the details content. The four lightweight ReShade, RenoDX, OptiScaler, and REFramework presence badges remain unchanged.

Favorites remain right-click-only. Health/override/change/update/duplicate badges remain out of the game list, with those details available in the details panel, context menu, and diagnostics.

v0.19.0 makes game detection explicitly repairable without teaching Reno119 a new heuristic for every edge case. Every Steam, Heroic, Lutris, and Custom entry can now keep a Reno119-only **Wine/Proton prefix override** and **graphics API override** alongside the existing executable override. Graphics API choices are Auto, DirectX 9/10/11/12, Vulkan, and OpenGL. The details panel and Library diagnostics show the automatic value and active value side-by-side, and every override can be reset independently without modifying launcher configuration.

A new **Rescan this game** action re-checks only the selected entry's local prefix/executable/API state, updates that game's cached snapshot data, and avoids a full Steam/Heroic/Lutris library refresh. Steam rescans repeat the cross-library AppID compatdata search; local executable rescans reuse the Ubisoft-prefix fallback and graphics evidence logic. Detailed diagnostics now include graphics-detection evidence so it is visible whether the result came from the PE import table, runtime strings, or adjacent renderer modules. The fast v0.18.8 last-known-library snapshot remains the startup path.

The optional QtTest suite is expanded with regression coverage for moved Steam compatdata, Ubisoft title initialisms such as AFOP/WD2, Watch Dogs 2-style dynamic/adjacent DX11 evidence, plus the existing Granblue/OptiScaler false-positive cases. Normal builds remain unchanged unless `RENO119_BUILD_TESTS=ON` is enabled.

Heroic and Lutris entries with a valid installed-game record are no longer silently discarded just because executable discovery fails. They remain visible with a **Needs setup** diagnostic so the bad/missing configured path can be inspected and corrected with Reno119's existing executable override. Steam entries similarly report when no Windows executable is resolved or when compatdata has not been created yet. The library keeps the normal row path checks lightweight; binary signature inspection is performed only when the detailed diagnostics view/report is requested.

No launcher providers were added. Reno119 remains focused on Steam, Heroic, Lutris, and Custom entries, and launcher databases/configuration remain read-only.

## Changes in v0.21.26

- Adds optional `GameModelUpdateStatusTests` coverage for incremental row updates versus Updates-filter model rebuilds.
- Adds a test-only GameModel fixture path behind `RENO119_TESTING`; normal application initialization is unchanged.
- Extracts simple settings-import, custom-program removal, external ReShade/ReShade64/REFramework takeover, and backup-restore confirmations from `Main.qml`.
- Adds reusable `RenoConfirmDialog`, `RenoPreviewConfirmDialog`, and matching dialog-button components while preserving existing actions and visual treatment.
- Leaves the state-heavy Update Center, diagnostics, recovery review, tweak preview, and hotkey chooser in `Main.qml` for later cleanup.
- No intended feature or UI behavior changes.

## Changes in v0.21.22

- No intended feature, installer, matching, or UI behavior changes.
- Splits `InstallerManager.cpp` into focused implementation files while preserving the existing class, signals, invokables, members, and QML API.
- Moves private INI/hotkey/tweak helper functions into `InstallerManagerInternal`.
- Uses CMake `PROJECT_VERSION` as the compile-time C++ application/network version instead of repeating the version literal across source files.
- The main QML window title now reads `Qt.application.version`.

## Changes in v0.21.21

- Dedicated RenoDX resolution now records and displays the matched catalog title, addon filename, and match method.
- Exact full-title matches install normally. Subtitle-stripped exact and unique-prefix dedicated matches require explicit confirmation.
- Update Center surfaces non-exact RenoDX matches in its review dialog; choosing **Start updates** confirms the displayed match.
- Adds optional QtTest coverage for the Peace Walker/MGS Delta regression, exact matching, subtitle-stripped exact matching, safe prefix matching, and ambiguous-prefix rejection.
- Recommended Setup now says **live compatibility metadata** instead of exposing the RHI source name.

## Changes in v0.21.20

- Release archives again contain one stable top-level `reno119/` source directory.
- Keeps CMake's existing `build/` directory for generated build artifacts.
- Adds `NO_IMPORT_SCAN` to the Reno119 QML module so Qt does not scan/link unrelated system QML plugins such as Quickshell.
- Reno119 itself still imports only QtQuick, QtQuick.Controls, QtQuick.Layouts, QtQuick.Window, and QtQuick.Dialogs.

## Changes in v0.21.19

- Keeps exact full-title RenoDX snapshot matching unchanged.
- Keeps exact subtitle-stripped matching for cases where the catalog intentionally uses a shorter title.
- Removes subtitle-stripped names from fuzzy/prefix matching, so a broad franchise base cannot resolve to an unrelated dedicated addon.
- Full launcher titles can still use the existing unique prefix fallback for legitimate edition/suffix differences.

## Changes in v0.21.18

- Renames the user-facing note to **Game note**.
- Removes RHI-specific wording from the note header and collapsed/expanded summary text.
- Keeps the existing imported-note source and sanitization behavior unchanged.

## Changes in v0.21.17

- Fixes RHI note cleanup for the current “Install it from the RE Framework row above.” wording.
- Keeps useful game-specific RHI guidance while removing table-layout navigation that does not exist in Reno119.

- Every locally detected RE Engine title is treated as REFramework-supported without requiring a title-list/RHI match.
- The upstream REFramework title list remains a fallback when engine detection is inconclusive.
- Imported RHI game notes strip RHI-specific “row above” REFramework installation directions while retaining title-specific guidance.
- The v0.21.15 REFramework hotkey fix remains intact.

## v0.17.0 Metadata backup and alternate installs

v0.17.0 adds **portable metadata backup/restore** and conservative **duplicate-install linking**. The existing JSON configuration export is schema v2 and includes per-game notes, verification fingerprints/dates, workspace view state, favorites, nicknames, hidden state, artwork/executable overrides, custom programs, library preferences, and linked alternate-install groups. Import remains backward-compatible with schema-v1 Reno119 exports. Game files, launcher databases/configuration, component ownership files beside games, download caches, and recovery snapshots are still excluded.

Probable duplicate installs are suggested only when normalized display titles match across different sources; short/ambiguous initialisms are intentionally ignored. Suggestions never merge automatically. A **Duplicate?** row badge and selected-game/context-menu actions open a review popup where matching installs can be linked as alternates; a manual link selector is also available when launcher titles differ too much for automatic matching. Linked installs receive an **Alt ×N** badge, while each install keeps its own executable, Wine/Proton prefix, ReShade/RenoDX/REFramework state, notes, verification state, and recovery history. Links are Reno119 metadata only and can be removed at any time.

v0.16.0 is a library quality-of-life release. Games from every source can now be marked as **Favorites**, filtered through a dedicated Favorites view, and optionally pinned ahead of non-favorites while preserving the selected sort mode. Game rows use compact state badges for managed/external ReShade, RenoDX, REFramework, update and integrity state, and expose a one-click star without increasing the established list height.

The game list also has a **right-click context menu** for favorite state, nickname, artwork override, hide/unhide, opening the executable folder, opening the Wine/Proton prefix, opening the resolved Engine.ini folder, editing custom entries, and refreshing the library. A new **Library** settings tab can independently enable or disable Steam, Heroic, and Lutris scanning and can add optional nonstandard Heroic/Lutris roots without changing launcher configuration. These library preferences are included in Reno119 settings export/import.

This release also keeps generic artwork overrides separate from a custom entry's base artwork, so removing an override correctly falls back to the underlying launcher/custom artwork.

v0.15.3 adds persistent **per-game nicknames** for Steam, Heroic, Lutris, and custom entries. Nicknames affect Reno119's displayed title, alphabetical sorting, and search while preserving the original launcher title for RenoDX/REFramework/PCGamingWiki compatibility matching. The selected-game header provides **Set nickname / Change nickname / Clear nickname**, and a small **Original:** line appears beneath the title while a nickname is active. Nickname state is stored only in Reno119 settings and never modifies launcher metadata.

v0.15.2 expands **custom artwork overrides to every game source**, not just manually added custom entries. Steam, Heroic, Lutris, and custom games all offer **Set custom artwork / Change custom artwork / Remove custom artwork** in the selected-game header, and the chosen image is stored as a Reno119 per-game override without modifying launcher metadata. Removing the override restores the launcher/default artwork.

v0.15.1 adds persistent per-game hiding across Steam, Heroic, Lutris, and custom entries. Hidden games are excluded from every normal library view and can be reviewed through the **Hidden games** status filter, where the selected-game action becomes **Unhide game**. Hidden state is stored in Reno119 settings and never modifies launcher metadata. This release also handles the recovery manifest `QFile::open()` result explicitly, removing the Qt 6 `nodiscard` compiler warning seen in `restoreRecovery`.

v0.15.0 expands Reno119 beyond Steam/custom entries with read-only **Heroic** and **Lutris** library discovery and adds user-selected artwork for manually added programs. Heroic discovery reads native/Flatpak launcher metadata for installed Epic games plus compatible GOG/Amazon install caches, resolves each game executable and Wine/Proton prefix when available, and reuses Heroic-maintained local artwork where possible. Lutris discovery opens `pga.db` read-only, follows installed Wine-game `configpath` entries into per-game YAML, resolves `game.exe`/`game.prefix` including `$GAMEDIR`, and reuses Lutris cover/banner artwork. Imported launcher configuration is never rewritten. Custom programs can now choose/change/clear a local image from both the entry editor and selected-game controls; the same image is used for the list cover and details banner. Source filtering now includes Steam, Heroic, Lutris, and Custom entries.


v0.14.0 centralizes binary/proxy detection in a dedicated `ModDetectionService` shared by the game-state scanner and OptiScaler integration. ReShade, REFramework, OptiScaler signature checks, ReShade proxy discovery, and OptiScaler proxy resolution now use one implementation instead of duplicated heuristics. The stricter v0.13.3 OptiScaler rule remains an explicit invariant: a lone `OptiScaler` string inside a generic proxy/ASI loader is not enough to claim the file. An optional QtTest regression suite covers ReShade/REFramework signatures, real and false-positive OptiScaler cases (including the Granblue-style `winmm.dll` ASI-loader case), ambiguous proxy fallback behavior, and ReShade/OptiScaler coexistence. Normal builds are unchanged; tests are built only with `-DRENO119_BUILD_TESTS=ON`.


v0.13.3 tightens OptiScaler proxy detection to avoid false positives from unrelated proxy/ASI loaders that share filenames such as `winmm.dll`, `dxgi.dll`, or `version.dll`. Binary inspection now requires the `OptiScaler` name plus an independent Opti-specific marker (`OptiScaler.ini`, `OptiFG`, or `OptiDllPath`) before identifying a DLL as OptiScaler. The existing conservative fallback that pairs an actual `OptiScaler.ini` with one unambiguous supported proxy remains unchanged. This fixes cases such as Granblue Fantasy: Relink ultrawide fixes that use Ultimate ASI Loader as `winmm.dll`.


v0.13.2 routes Reno119 file and folder selection through the XDG Desktop Portal FileChooser on Linux so the active desktop/session chooses the system file picker. Existing paths still seed the picker to the original file's containing folder (or the prefix directory itself). The prior QtQuick.Dialogs chooser remains only as a compatibility fallback if the portal request fails.


v0.13.1 improves Browse dialogs without replacing the desktop/session file chooser. Reno119 continues to use QtQuick.Dialogs so Qt can hand the picker to the platform-native implementation when available, but Browse now seeds that chooser from the path already shown in the relevant field: existing files open in their containing directory, existing Wine/Proton prefixes open at that directory, and an EXE override with an empty field starts beside the currently selected/detected executable. Empty/new paths are left unset so the platform chooser can use its own default location.

## v0.13.0 Recommended setup summary

v0.13.0 adds a compact **Recommended setup** panel near the top of each game detail view. It combines Reno119's existing executable/engine detection, current ReShade/RenoDX/REFramework state, live RenoDX addon resolution, managed tweak profiles, and OptiScaler analysis into one advisory overview. Dedicated RenoDX wiki matches are distinguished from generic Unreal/Unity fallbacks, managed tweak profile updates are surfaced, supported RE Engine titles get REFramework guidance, and detected OptiScaler conflicts/drift are called out. When Reno119 has a suggested Proton `WINEDLLOVERRIDES` launch option, the panel shows it with a one-click **Copy** action. The summary never installs or applies anything automatically.


## v0.12.0 Recovery, profile updates and working setups

- Recovery and troubleshooting now lists retained addon/INI recovery copies with a review-and-restore action. Existing general component backups remain in their history panel. Recovery checks the selected game's copy and current file fingerprints, saves the current files first, and rolls back failures. RenoDX recovery retains unrelated component ownership and handles a newer managed addon filename. Additional unmanaged RenoDX addons must be resolved first.
- Profile notices compare the current catalog's settings with the last applied profile and list additions, changed values and removed settings. Reapply still requires the existing preview. Removed profile keys are retained in the user's INI; no change is applied automatically. Older installations establish their profile baseline on a reviewed reapply.
- The selected game, list/detail scroll positions and panel expansion choices are remembered across launches. Saved positions are clamped to the current layout; a game missing from the visible model falls back to the current selection.
- Mark verified working records the current component/configuration fingerprint and UTC date after you test the game. Component/tweak changes refresh the indicator. Recheck setup detects external edits. DLL/addon bytes, INI files, known Engine.ini targets, prefix and ownership settings are compared; this is not an in-game test or a monitor of system drivers/Steam launch options.
- Notes have their own collapsible card. Recovery, diagnostics and unavailable-action explanations are collapsed by default, while the working-setup marker remains visible.
- Source and archive reviewed; no compilation or GUI runtime validation (build tools unavailable in this environment).

## v0.11.1 Collapsible game notes

- Game notes now use an expandable header and start collapsed. Expansion is remembered per game for the current session.
- Collapsing hides the editor and Save notes button while keeping automatic saving and saved text intact. Save errors remain visible in the collapsed header.
- Recovery and troubleshooting controls remain accessible independently of the notes section.
- Source and archive checked; not compiled or runtime-tested.

## v0.11.0 Recovery access, action explanations, reports and notes

- Added a Game notes and troubleshooting panel for the selected game.
- Open recovery folder opens the newest retained component/tweak recovery directory, including copies made by earlier versions. If no copy exists, the panel explains why the button is unavailable.
- Unavailable actions lists current reasons for the main install, removal, tweak, component restore and OptiScaler actions: busy operations, missing prerequisites, unsupported configurations, unmanaged components, absent snapshots and unchanged integrations.
- Troubleshooting report opens a selectable preview with one-click copying. The existing diagnostics report now includes managed tweak settings and the latest recovery path. Home-directory paths are hidden by default; the preview can show the original paths. Copy/export diagnostics use redaction by default. Personal game notes are not included in reports.
- Notes save automatically by stable game ID, independently of managed installation state and file restores. Notes are included in configuration export/import. A failed save keeps the draft in the current session and displays a retry action; unsaved drafts do not survive closing the app.
- Source review and archive comparison performed. Compilation could not be performed here: CMake and Ninja are unavailable. No GUI runtime validation was performed.

## v0.10.2 Dropdown wheel guard

- Closed dropdowns explicitly consume wheel events before they reach the dropdown, in addition to disabling wheel selection. This applies to every shared RenoComboBox, including focused controls.
- Wheel and touchpad deltas scroll the nearest surrounding scrollable view, with bounds clamping and ancestor fallback at an edge. Controls outside a scrollable view consume the wheel without changing the selection.
- Click and keyboard selection remain available; opening the dropdown disables the guard so its popup list can scroll normally.
- Source and archive checks only; not compiled or tested in a running GUI.

## v0.10.1 External RenoDX takeover

- Replaced the disabled External RenoDX detected button with an explicit Install over external RenoDX action. ReShade or ReShade64 remains required.
- Added a backend takeover path that can replace unmanaged RenoDX addons after a required backup, including when the downloaded addon has a different filename.
- Existing recognized RenoDX addons are backed up and replaced together to avoid loading duplicate versions. Unrelated addons are left alone.
- Downloads and backup creation must finish successfully before changing game files. Changes during download abort the operation. File/configuration/state-write failures roll back from the immediate safety snapshot.
- Recovery copies contain the original addons, ReShade.ini, and ownership state; the recovery directory is reported on completion.
- Source reviewed and archive checked; no compile or runtime validation.

## v0.10.0 Tweak previews and external-change protection

- Applying/reapplying managed RenoDX tweaks and restoring originals now opens a review dialog with exact target paths, current contents, proposed contents, file creation/removal, and permission actions.
- SHA-256 and permission baselines detect changes to managed Engine.ini/ReShade.ini files, including deleted files. Review and explicit acknowledgement are required before proceeding over changes; Cancel leaves files untouched.
- Profiles applied by older versions have no baseline and are treated as unverified until a reviewed reapply establishes one.
- Confirmation rechecks the reviewed files, profile, game identity, and backup contents. If these changed, reopen or refresh the preview.
- Reapply failures roll back to the immediate pre-operation files instead of the first-ever snapshot. The original snapshot remains the Restore original target.
- A recovery snapshot is kept for changed/unverified reapplications and every original-file restore. Its directory is shown in the operation status. Each numbered `.bin` file maps to its original path and permissions in `tweak-manifest.json`; these copies are not automatically pruned.
- If a profile resolves to different target paths, reapply is blocked until the original profile is restored.
- This release tracks managed tweak INI files. DLL tracking and automatic background watching are outside this change; files are checked when opening/refreshing the preview and again on confirmation.
- Source review only; no compilation or GUI runtime validation was performed for this release.

## v0.9.2 Engine.ini folder shortcut

- Added an **Open Engine.ini folder** action to managed RenoDX tweak profiles.
- The action opens the directory containing the exact Engine.ini path Reno119 resolved for the selected game.
- The existing **Open source** action remains separate.


## v0.9.1 Qt 6 snapshot compile fix

- Fixed the managed-tweak snapshot serializer failing to compile with Qt 6 because `QFile::Permissions::toInt()` yields an unsigned value that is ambiguous between `QJsonValue` numeric constructors.
- The permissions bitmask is now explicitly converted to `int` before insertion into the snapshot JSON; restore semantics are unchanged.


## v0.9.0 managed RenoDX tweak profiles

- Added a managed RenoDX tweak layer for Proton/Wine games. Reno119 can now merge narrowly scoped `Engine.ini` and `ReShade.ini` settings without replacing unrelated user configuration.
- Added live RHI manifest ingestion for per-game `renodxIniOverrides`, `ueExtendedCompatibility`, `nativeHdrGames`, `engineIniFiles`, and `engineIniPathOverrides`. Remote per-game Engine.ini files are fetched from RHI's `engine-files/` directory when the manifest references one. Current manifest/profile data is cached locally so normal launches reuse it instead of refetching the manifest and every referenced profile.
- Generic Unreal Engine tweaks are offered after the official RenoDX Mods catalog has finished checking. A dedicated game snapshot normally suppresses the generic profile, except where RHI explicitly forces UE-Extended/native-HDR handling; curated Reno119 profiles such as Silent Hill f remain intentionally narrower.
- Added a curated Silent Hill f profile using only `[/Script/Engine.RendererSettings] r.LUT.UpdateEveryFrame=1`, matching the bespoke RenoDX mod instructions instead of applying the generic Unreal HDR CVar set.
- Proton config discovery searches the selected game's own prefix and understands `Windows`, `WindowsNoEditor`, `WindowsClient`, `WinGDK`, `Saved_Global`, and RHI `engineIniPathOverrides`. Windows `%LOCALAPPDATA%` / `%USERPROFILE%` overrides are translated into the Proton prefix. Reno119 refuses to guess when multiple plausible Engine.ini files are present.
- The RenoDX panel now shows the matched tweak profile, exact keys, detected Engine.ini path, source, and Apply/Restore controls. Tweaks are never silently applied just because RenoDX was installed.
- Before the first tweak application, Reno119 stores an exact snapshot of every target file plus its original permissions. **Restore original** restores those bytes/permissions or removes a file that Reno119 created. Read-only Engine.ini profiles are made read-only after the merge.
- Removing the RenoDX addon does not automatically destroy managed tweak state; Reno119 leaves the files alone and lets the user explicitly restore the original snapshot.


## v0.8.9 initial game-list layout fix

- Increased the left game panel to 430 px and made that its minimum width so the filter/sort controls are not squeezed during the initial SplitView layout.
- Kept the Qt attached scrollbar behavior from v0.8.7, but made its visibility follow actual list overflow directly.
- Forced the scrollbar above game delegates and active while visible so it renders immediately after scanning instead of only appearing after a window resize.

## v0.8.8 game-panel spacing adjustment

- Increased the left game panel preferred width from 414 px to 418 px so the sorting controls have a little more horizontal room and are no longer clipped.
- Kept the v0.8.7 Qt native attached game-list scrollbar unchanged.

## Previous v0.8.7 native game-list scrollbar fix

- Replaced the hand-written game-list scrollbar drag handler with Qt Quick Controls' attached `ScrollBar`, while retaining Reno119's explicit themed track and accent thumb so desktop styles cannot make the handle disappear.
- Scrollbar dragging is now normalized to the `ListView`'s complete scroll range. Moving the thumb from the top to the bottom of its track reaches the beginning/end of the game library regardless of library length or monitor height, with the native scroll direction.
- Kept the v0.8.6 left game panel width increase at 414 px.

## Previous v0.8.6 game-list scrollbar and panel sizing

- Reworked the custom game-list scrollbar drag mapping so the grabbed point on the thumb tracks the pointer directly instead of using accumulated content deltas. This fixes the reversed/unstable drag behavior while keeping track-click jumping intact.
- Increased the left game panel preferred width slightly from 410 px to 414 px.

## Previous v0.8.5 banner edge blending

- The selected-game Steam banner now fades into the active details-panel theme on all four edges. The existing deeper bottom fade is retained, with matching softer top, left, and right gradients so artwork no longer ends abruptly against the panel.

## Previous v0.8.4 scrolling and banner polish

- Replaced the left game-list Qt Quick Controls scrollbar with a Reno119-owned track and thumb whose geometry is derived from the model count, fixed row height, spacing, and viewport height. It no longer depends on `ListView.visibleArea` during cached startup, supports dragging, and lets you click the track to jump through the library.
- Removed the visible scrollbar from the right/info pane. Mouse-wheel and touchpad scrolling continue to work normally through the `ScrollView`.
- Increased the selected-game Steam hero area to a responsive ~16:7 layout (bounded to 260–340 px high), showing more of the artwork vertically while retaining `PreserveAspectCrop`.

## Previous v0.8.3 artwork and scrollbar fixes

- Steam games now use Steam's wide Library Hero/header artwork as a banner above the selected game name. Reno119 checks Steam's local `appcache/librarycache` first, then falls back to Steam CDN hero/header paths, and finally crops the existing portrait cover when no wide asset is available.
- Wide artwork is cached independently under `~/.cache/reno119/banners/`; portrait covers remain under `~/.cache/reno119/covers/`. The selected-game controls now refresh or clear both artwork types together, and the cache settings expose the combined artwork paths.
- Both vertical scrollbar thumbs now have explicit implicit height and a minimum thumb size, preventing custom Qt styles from collapsing the draggable handle to zero height while leaving only the track visible.

## Previous v0.8.2 interaction fixes

- The game-list scrollbar now lives in a dedicated non-layout wrapper, eliminating the Qt warning about anchoring a layout-managed scrollbar.
- Game-list scrollbar visibility follows the attached scrollbar's actual `size`, so it appears immediately whenever the list is scrollable instead of waiting for a SplitView resize.
- All Reno119 ComboBoxes use a shared wheel-safe control with wheel selection explicitly disabled while closed. Scrolling over a closed dropdown continues scrolling the surrounding page/list instead of changing the selected value; the opened popup remains normally scrollable.
- The existing resizable game-list SplitView behavior and explicit details-pane scrollbar are retained.

## Previous v0.8.1 reliability notes

- REFramework support data is refreshed from the maintained upstream REFramework README at runtime, with a local fallback list that includes Dead Rising Deluxe Remaster and Onimusha: Way of the Sword.
- A positively detected external REFramework install can be explicitly taken over even if title matching is incomplete; x64 and mandatory-backup protections still apply.
- REFramework hotkey detection reads the existing game-specific `*_fw_config.txt` before falling back to Insert. Reno119 will not invent a config filename when changing the key; launch the game once if no REFramework config exists yet.
- OptiScaler hotkey detection continues to read the existing `ShortcutKey` from `OptiScaler.ini`, preserving pre-existing custom bindings.
- Startup library refresh performs a quiet integrity check for missing Reno119-managed files and basic saved OptiScaler integration drift.
- Library rows show more useful RenoDX/REFramework version or filename detail when known.


Reno119 is a native Qt 6/QML Linux manager for ReShade and RenoDX on Windows games running through Steam/Proton or user-added custom program entries. It also includes narrowly scoped integration helpers for OptiScaler and REFramework where those tools affect ReShade/RenoDX injection.

## Previous v0.8.0 additions

### Library-wide update checking

The game-library toolbar now includes **Check all updates**. Reno119 checks every game with at least one Reno119-managed ReShade/ReShade64, RenoDX, or REFramework component. Latest ReShade and REFramework metadata are fetched once per library-wide pass, then the already-loaded RenoDX catalog is used to evaluate each managed game locally. The result reports both the number of games with updates and the total number of component updates; nothing is installed automatically.

Per-game update checks now include REFramework as well as ReShade and RenoDX. The **Updates available (checked)** library filter and row update badge also include REFramework Nightly updates.

### Dedicated REFramework restore history

Backups whose transaction reason belongs to REFramework are now classified and shown in a dedicated **REFramework restore points** group in the collapsible backup/history panel. This includes backups created before managed Nightly install/update, remove, overlay-hotkey edits, and external-REFramework takeover. Restore behavior remains conservative and only restores the captured Reno119 transaction state/files.

### Settings export / import

Settings → Storage & Safety now provides **Export settings…** and **Import settings…** using a portable JSON format. The export includes appearance preferences, Custom ReShade configuration, custom-program entries, executable overrides, and per-game UI/applied-choice settings. It intentionally excludes game-side `.reno119-state.json` ownership files, DLLs, backups, downloads, and cover caches so importing a profile cannot claim ownership of files that are not actually present.

Import replaces only Reno119's portable configuration keys and refreshes the game library afterward.

## Previous v0.7.2 additions

### RenoDX-only compatibility catalog

Reno119 no longer downloads or depends on an external compatibility manifest. Dedicated RenoDX addon matching and the Recommended ReShade baseline now come directly from the live RenoDX `Mods.md` wiki. Game-specific title matching keeps the existing punctuation normalization, subtitle stripping, and unique safe-prefix fallback. Generic Unreal/Unity addons remain local engine-based fallbacks when the RenoDX catalog has no dedicated snapshot.

ReShade proxy selection now uses Reno119's local graphics-API detection (`d3d9.dll`, `opengl32.dll`, or `dxgi.dll`) instead of an external DLL-name override table.

### Pursuit-mode easter egg

Click the **Reno119** header logo ten times within four seconds to toggle Pursuit Mode. When enabled, the logo alternates red/blue with a soft glow. The setting is persisted in QSettings (`appearance/pursuitModeEnabled`) and survives application restarts. Ten clicks again disables it.

### Explicit interactive scrollbars

The game library and selected-game details pane now use explicitly styled, interactive vertical scrollbars whenever their content overflows. The scrollbars stay visible instead of relying on transient system-style behavior, use the current Reno119/system accent, and brighten on hover/drag. The resizable SplitView game-list width is unchanged.

## Previous v0.7.1 additions

### Diagnostics export

The backups/history panel includes **Export diagnostics…** for the selected game. The exported text report is designed to be pasted into an issue or troubleshooting chat and includes game/executable detection, component ownership and versions, ReShade/ReShade64 layout health, RenoDX resolution source/URL, OptiScaler proxy and integration state, proxy/conflict scan results, drift/issues, advisory launch-option text, update state, PCGamingWiki resolution state, and the last recorded per-component operation messages. Export is read-only and never modifies the game.

The existing **Copy debug summary** remains available for a shorter clipboard-only snapshot.

## Previous v0.7.0 additions

### System Qt theming

Appearance now offers **System**, **Reno119 Dark**, and **AMOLED Black**. System mode derives Reno119's surfaces, text, selection accent, and highlighted-button glow from Qt's `SystemPalette`. Reno119 also no longer forces the Fusion Quick Controls style at startup, allowing the desktop/session or `QT_QUICK_CONTROLS_STYLE` to select an installed Qt Quick Controls style. The built-in Dark and AMOLED palettes remain available when a consistent Reno119 appearance is preferred.

### Game library polish

The game list now has a visible accent-aware vertical scrollbar whenever the filtered library is taller than the available space. The selected-game header also shows a larger Steam portrait, game/source metadata, API/architecture/engine information, executable path, and the existing cover refresh/clear controls.

### Consistent collapsible component sections

ReShade, RenoDX, and REFramework now have collapsible headers, joining the existing OptiScaler and backups/history panels. Expansion state is remembered per game for the current Reno119 session. Collapsed headers retain a compact ownership/version summary.

### Settings categories and status language

Settings are split into **Appearance**, **Integrations**, and **Storage & Safety** categories. Component state wording is standardized around **Managed**, **External**, **Mixed**, and **Not installed**, including the compact game-list status labels.

## Previous v0.6.3 additions

### Advisory-only Wine / Proton overrides

Reno119 never writes or modifies Steam launch options and never automatically applies `WINEDLLOVERRIDES`. It may calculate a combined **suggested** override for ReShade, OptiScaler, and REFramework and provide a copy button, but applying that suggestion is always left to the user. This avoids unnecessary Steam restarts and respects Proton builds that already load commonly used native DLLs automatically.

OptiScaler Apply/Fix transactions now treat the suggestion as informational only; it is not listed as a filesystem/configuration change performed by Apply.

### Installed-only overlay conflict warning

The OptiScaler/REFramework overlay-hotkey conflict warning now appears only when **both OptiScaler and REFramework are actually installed/detected** and their resolved hotkeys match. Matching defaults no longer produce a warning when one component is absent.

## Previous v0.6.2 additions

### Better Steam cover artwork

Reno119 now prefers Steam's own local library-art cache before falling back to predictable Steam CDN portrait URLs. It understands both flat and nested/hash-style `appcache/librarycache` layouts, copies the best portrait into Reno119's own cover cache, and continues to use an initials placeholder when no suitable artwork is available.

Cover controls now include:

- **Refresh cover** for the selected Steam game.
- **Clear cover** for the selected Steam game.
- **Refresh all covers** in Settings → Cache.
- **Clear game covers** in Settings → Cache.
- Manual refresh bypasses the session's failed-cover suppression and actively retries local/remote artwork discovery.

Reno119 cover cache: `~/.cache/reno119/covers/` (or the equivalent XDG cache root).

### REFramework Nightly for supported RE Engine games

For supported 64-bit RE Engine titles, Reno119 can install the current monolithic REFramework Nightly. The non-VR workflow extracts and installs **only `dinput8.dll`** from `REFramework.zip`.

Reno119 currently gates this feature to the title families named by the current monolithic nightly, including Devil May Cry 5, Resident Evil 2/3/4/7/8/9, Monster Hunter Rise/Wilds/Stories 3, Street Fighter 6, Dragon's Dogma 2, Pragmata, and Star Force Legacy Collection.

Features include:

- **Install Nightly** / **Update / Reinstall Nightly**.
- Managed-vs-external `dinput8.dll` detection.
- **Install over external REFramework** with an explicit warning and mandatory takeover backup.
- Managed uninstall removes only Reno119's `dinput8.dll`; REFramework configs/scripts/plugins are preserved.
- REFramework is included in the game health strip and left-list status.
- RE Engine games with ReShade/RenoDX present but no REFramework are surfaced as a health warning.
- When useful, `dinput8=n,b` is included in Reno119's **suggested** combined `WINEDLLOVERRIDES` expression. Reno119 never writes that suggestion into Steam automatically.

### Separate overlay hotkeys

Reno119 now exposes separate hotkey controls for the two overlays:

- **OptiScaler overlay** edits `ShortcutKey` in `OptiScaler.ini`.
- **REFramework overlay** reads the existing game-side `*_fw_config.txt` and edits only its `MenuKey_V2` value.

The chooser includes Insert, Delete, Home, End, Page Up/Down, and F1–F12. Reno119 reports a warning only when both OptiScaler and REFramework are installed/detected and resolve to the same hotkey.

### PCGamingWiki supplemental game info

The selected-game details include an on-demand **PCGamingWiki** lookup. Reno119 uses the normal MediaWiki API to resolve the best matching game page, persists successful resolutions for 30 days, and lets the user open that page externally. Clearing/retrying the lookup bypasses the saved result.

This is deliberately supplemental: Reno119 does **not** make installs depend on PCGamingWiki and does not use authenticated Cargo data. Local executable/engine detection and authoritative mod-project sources remain responsible for installation decisions.

### Qt policy cleanup

On Qt 6.8+ Reno119 explicitly enables `QTP0004 NEW`, removing the harmless extra-QML-directory policy warning while retaining Qt 6.6 as the project's minimum supported Qt version.

## RenoDX live game catalog

Reno119 parses the live RenoDX `Mods.md` catalog directly. RenoDX resolution first looks for a dedicated per-game Snapshot URL from the RenoDX wiki, then uses the generic Unreal/Unity fallback when applicable. Game titles are normalized for punctuation, curly apostrophes, trademark symbols, subtitles, and safe unique-prefix matching.

This covers cases such as:

- `Assassin's Creed Valhalla` → `Assassin’s Creed® Valhalla`
- `DEATH STRANDING 2: ON THE BEACH` → `Death Stranding 2`

Nexus/Discord-only entries are not auto-installed because Reno119 requires a direct addon download URL.

## ReShade layouts

Reno119 treats normal proxy ReShade and OptiScaler-chain-loaded ReShade64 as separate layouts:

- Normal proxy: `dxgi.dll`, `d3d9.dll`, etc.
- OptiScaler chain loader: `ReShade64.dll`.

Both managed and verified external installs are detected independently. The health strip reports direct, ReShade64, mixed, external, and chain-load mismatch states. External ReShade/ReShade64 can be replaced only after explicit confirmation and a mandatory backup.

## OptiScaler integration scope

Reno119 intentionally does **not** install or tune OptiScaler. Its OptiScaler support is restricted to ReShade/RenoDX coexistence:

- Detect existing OptiScaler.
- Proxy selection.
- Separate-proxy / explicit `ReShade64.dll` chain-loading integration.
- `LoadReshade` integration changes.
- Advisory Wine/Proton DLL override composer (manual copy only; never auto-applied).
- Preview + explicit Apply.
- Discard pending changes / transaction Revert.
- Save/restore working integration setup.
- Quiet relevant-config drift detection.
- One-click Fix setup for repairable drift.
- Read-only proxy/conflict scanning.
- Transaction restore points.
- Overlay hotkey editing only where it affects overlay usability.

## Backups and history

Backups are per game and remain separate from OptiScaler integration transactions. The collapsible history panel provides selectable restore points for ReShade/RenoDX/REFramework changes and independent OptiScaler integration restore points. Restoring an older snapshot only cleans up newer files that Reno119 currently records as managed.

## Current features

- Native and Flatpak Steam library discovery.
- Read-only Heroic library discovery for installed Windows games, including native/Flatpak layouts.
- Read-only Lutris Wine-game discovery from `pga.db` and per-game YAML, including native/Flatpak layouts.
- Filters Steam-owned Proton/runtime/tool entries.
- Custom program entries with optional Wine/Proton prefix and user-selected local artwork.
- Search, source/status filtering, and sorting.
- XDG Desktop Portal file/folder pickers with Qt fallback for executable, prefix, artwork, settings, and custom ReShade paths.
- PE-based x86/x64 and DirectX/OpenGL/Vulkan detection.
- Unreal Engine / Unity / RE Engine heuristic detection.
- Per-game executable override.
- Optional Steam portrait artwork with local-cache-first lookup and manual refresh.
- Heroic/Lutris local artwork reuse plus custom cover/banner selection for manual entries.
- System / Reno119 Dark / AMOLED Black appearance modes.
- ReShade `Recommended / Latest / Custom` selection.
- Custom ReShade source by exact version, URL, or local installer.
- Managed-vs-external ReShade/ReShade64/RenoDX/REFramework detection.
- Separate normal proxy ReShade and OptiScaler `ReShade64.dll` installs.
- RenoDX Wiki live-catalog matching and generic engine fallbacks.
- REFramework Nightly install/update path for currently mapped supported RE Engine titles.
- Suggested Proton/Wine DLL override composer including ReShade proxy, OptiScaler proxy integration, and REFramework `dinput8` where applicable; Reno119 never writes launch options automatically.
- ReShade/RenoDX update checks, backup/restore, cache management, debug summary, and full selected-game diagnostics export.
- Compact per-game setup health strip for ReShade, RenoDX, REFramework, and OptiScaler.
- Persistent per-game inline operation status/errors during the Reno119 session.
- Collapsible ReShade, RenoDX, REFramework, OptiScaler, and backup/history panels.
- PCGamingWiki on-demand page resolution as a supplemental game-information source.

## Storage identity

Reno119 uses:

- executable / CMake target: `reno119`
- QML module: `Reno119`
- Qt settings identity: `Reno119`
- cache root: `~/.cache/reno119` (or `$XDG_CACHE_HOME/reno119`)
- data/log root: `~/.local/share/reno119` (or `$XDG_DATA_HOME/reno119`)
- log file: `reno119.log`
- per-game ownership state: `.reno119-state.json`
- OptiScaler integration state: `.reno119-optiscaler.json`
- network user agent: `Reno119/0.21.22`

The pre-Reno119 prototype identity is intentionally not migrated.

## Arch / CachyOS dependencies

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative 7zip
```

## Build

```bash
rm -rf build
./build.sh
```

Or manually:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/reno119
```

### Optional regression tests

The regression tests are intentionally opt-in so normal Reno119 builds do not require Qt Test. They cover component detection plus launcher/path/API resolution edge cases. To build and run them:

```bash
rm -rf build-tests
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DRENO119_BUILD_TESTS=ON
cmake --build build-tests --target reno119_detection_tests reno119_resolution_tests
ctest --test-dir build-tests --output-on-failure
```

## Current limitations

- Heroic and Lutris import is read-only and intentionally conservative. Entries with unresolved executable/prefix data remain visible for diagnostics and manual Reno119 overrides; launcher configuration is never rewritten. Reno119 currently focuses on Steam, Heroic, Lutris, and Custom entries rather than adding more launcher providers.
- Vulkan ReShade implicit-layer installation is not implemented.
- Managed RenoDX tweaks currently cover RHI-driven UE-Extended Engine.ini/ReShade.ini values and curated bespoke profiles; arbitrary tweak instructions embedded only in free-form wiki/discussion text are not auto-imported yet.
- External ReShade version detection depends on a useful `ReShade.log`; it may show an unknown version.
- Reno119 never silently takes ownership of externally detected ReShade/ReShade64/REFramework files.
- Externally detected RenoDX installs are not silently claimed; Reno119 can explicitly install over them only after the existing confirmation/takeover backup flow.
- PCGamingWiki integration currently resolves/opens game pages on demand; it does not import Cargo metadata or make install decisions.
- OptiScaler support remains intentionally limited to ReShade/RenoDX coexistence and overlay-hotkey editing; Reno119 does not install or tune OptiScaler itself.
