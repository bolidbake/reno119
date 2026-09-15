cmake_minimum_required(VERSION 3.24)
project(reno119 VERSION 0.23.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Concurrent DBus Sql)
qt_standard_project_setup(REQUIRES 6.6)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(reno119
    src/main.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/ThemeManager.h
    src/core/ThemeManager.cpp
    src/core/PortalFileDialog.h
    src/core/PortalFileDialog.cpp
    src/core/GameInfo.h
    src/core/ModDetectionService.h
    src/core/ModDetectionService.cpp
    src/core/GameModel.h
    src/core/GameModel.cpp
    src/core/GameModelFiltering.cpp
    src/core/GameModelLibraryManagement.cpp
    src/core/GameModelLibraryState.cpp
    src/core/GameModelUpdateStatus.cpp
    src/steam/SteamScanner.h
    src/steam/SteamScanner.cpp
    src/library/HeroicScanner.h
    src/library/HeroicScanner.cpp
    src/library/LutrisScanner.h
    src/library/LutrisScanner.cpp
    src/graphics/PeParser.h
    src/graphics/PeParser.cpp
    src/installers/InstallerManager.h
    src/installers/InstallerManager.cpp
    src/installers/InstallerManagerInternal.h
    src/installers/InstallerManagerInternal.cpp
    src/installers/InstallerManagerReShade.cpp
    src/installers/InstallerManagerRenoDx.cpp
    src/installers/InstallerManagerReFramework.cpp
    src/installers/InstallerManagerUpdates.cpp
    src/installers/InstallerManagerDiagnostics.cpp
    src/integrations/OptiScalerIntegration.h
    src/integrations/OptiScalerIntegration.cpp
    src/integrations/OptiScalerIntegrationConfig.cpp
    src/integrations/OptiScalerIntegrationSnapshots.cpp
    src/integrations/OptiScalerIntegrationPlanning.cpp
    src/network/RenoDxCatalogService.h
    src/network/RenoDxCatalogService.cpp
    src/network/RenoDxTitleMatcher.h
    src/network/RenoDxTitleMatcher.cpp
    src/network/CoverService.h
    src/network/CoverService.cpp
    src/network/PCGamingWikiService.h
    src/network/PCGamingWikiService.cpp
    src/network/ReFrameworkSupportService.h
    src/network/ReFrameworkSupportService.cpp
)

target_compile_definitions(reno119 PRIVATE
    RENO119_VERSION="${PROJECT_VERSION}"
)

qt_add_qml_module(reno119
    URI Reno119
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
        qml/RenoProgressBar.qml
        qml/RenoTextArea.qml
        qml/RenoDialogButton.qml
        qml/RenoConfirmDialog.qml
        qml/RenoPreviewConfirmDialog.qml
        qml/RenoComboBox.qml
        qml/RenoDxMatchDialog.qml
        qml/RenoOverlayHotkeyDialog.qml
        qml/RenoNicknamePopup.qml
        qml/RenoDuplicatePopup.qml
        qml/RenoCustomProgramPopup.qml
        qml/RenoTroubleshootingDialog.qml
        qml/RenoUpdatePreviewDialog.qml
        qml/RenoUpdateHistoryDialog.qml
        qml/RenoRecoveryDialog.qml
        qml/RenoTweakPreviewDialog.qml
)

target_link_libraries(reno119 PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Sql
)

install(TARGETS reno119 RUNTIME DESTINATION bin)
install(FILES
    data/noctalia/reno119-theme.json.template
    data/noctalia/reno119-template.toml
    DESTINATION share/reno119/noctalia
)
install(FILES
    data/material/reno119-theme.example.json
    DESTINATION share/reno119/themes
)
install(FILES
    data/applications/reno119.desktop
    DESTINATION share/applications
)
option(RENO119_BUILD_TESTS "Build Reno119 regression tests" OFF)
if(RENO119_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    enable_testing()
    qt_add_executable(reno119_detection_tests
        tests/ModDetectionServiceTests.cpp
        src/core/ModDetectionService.h
        src/core/ModDetectionService.cpp
    )
    target_include_directories(reno119_detection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_detection_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME reno119_detection_tests COMMAND reno119_detection_tests)

    qt_add_executable(reno119_resolution_tests
        tests/ResolutionRegressionTests.cpp
        src/steam/SteamScanner.h
        src/steam/SteamScanner.cpp
        src/graphics/PeParser.h
        src/graphics/PeParser.cpp
        src/core/GameInfo.h
    )
    target_include_directories(reno119_resolution_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_resolution_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME reno119_resolution_tests COMMAND reno119_resolution_tests)

    qt_add_executable(reno119_renodx_matcher_tests
        tests/RenoDxTitleMatcherTests.cpp
        src/network/RenoDxTitleMatcher.h
        src/network/RenoDxTitleMatcher.cpp
    )
    target_include_directories(reno119_renodx_matcher_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_renodx_matcher_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME reno119_renodx_matcher_tests COMMAND reno119_renodx_matcher_tests)

    qt_add_executable(reno119_game_model_update_tests
        tests/GameModelUpdateStatusTests.cpp
        src/core/GameModel.h
        src/core/GameModel.cpp
        src/core/GameModelFiltering.cpp
        src/core/GameModelLibraryManagement.cpp
        src/core/GameModelLibraryState.cpp
        src/core/GameModelUpdateStatus.cpp
        src/core/GameInfo.h
        src/core/ModDetectionService.h
        src/core/ModDetectionService.cpp
        src/graphics/PeParser.h
        src/graphics/PeParser.cpp
        src/steam/SteamScanner.h
        src/steam/SteamScanner.cpp
        src/library/HeroicScanner.h
        src/library/HeroicScanner.cpp
        src/library/LutrisScanner.h
        src/library/LutrisScanner.cpp
    )
    target_compile_definitions(reno119_game_model_update_tests PRIVATE RENO119_TESTING=1)
    target_include_directories(reno119_game_model_update_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_game_model_update_tests PRIVATE Qt6::Core Qt6::Concurrent Qt6::Sql Qt6::Test)
    add_test(NAME reno119_game_model_update_tests COMMAND reno119_game_model_update_tests)

    qt_add_executable(reno119_optiscaler_tests
        tests/OptiScalerIntegrationTests.cpp
        src/integrations/OptiScalerIntegration.h
        src/integrations/OptiScalerIntegration.cpp
        src/integrations/OptiScalerIntegrationConfig.cpp
        src/integrations/OptiScalerIntegrationSnapshots.cpp
        src/integrations/OptiScalerIntegrationPlanning.cpp
        src/core/GameModel.h
        src/core/GameModel.cpp
        src/core/GameModelFiltering.cpp
        src/core/GameModelLibraryManagement.cpp
        src/core/GameModelLibraryState.cpp
        src/core/GameModelUpdateStatus.cpp
        src/core/GameInfo.h
        src/core/ModDetectionService.h
        src/core/ModDetectionService.cpp
        src/graphics/PeParser.h
        src/graphics/PeParser.cpp
        src/steam/SteamScanner.h
        src/steam/SteamScanner.cpp
        src/library/HeroicScanner.h
        src/library/HeroicScanner.cpp
        src/library/LutrisScanner.h
        src/library/LutrisScanner.cpp
    )
    target_compile_definitions(reno119_optiscaler_tests PRIVATE RENO119_TESTING=1)
    target_include_directories(reno119_optiscaler_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_optiscaler_tests PRIVATE Qt6::Core Qt6::Concurrent Qt6::Sql Qt6::Test)
    add_test(NAME reno119_optiscaler_tests COMMAND reno119_optiscaler_tests)

    qt_add_executable(reno119_theme_tests
        tests/ThemeManagerTests.cpp
        src/core/ThemeManager.h
        src/core/ThemeManager.cpp
    )
    target_include_directories(reno119_theme_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_theme_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME reno119_theme_tests COMMAND reno119_theme_tests)

    qt_add_executable(reno119_app_settings_tests
        tests/AppSettingsTests.cpp
        src/core/AppSettings.h
        src/core/AppSettings.cpp
    )
    target_include_directories(reno119_app_settings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reno119_app_settings_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME reno119_app_settings_tests COMMAND reno119_app_settings_tests)
endif()
