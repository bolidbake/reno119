#include "core/AppSettings.h"
#include "core/GameModel.h"
#include "core/PortalFileDialog.h"
#include "core/ThemeManager.h"
#include "installers/InstallerManager.h"
#include "integrations/OptiScalerIntegration.h"
#include "network/RenoDxCatalogService.h"
#include "network/CoverService.h"
#include "network/PCGamingWikiService.h"
#include "network/ReFrameworkSupportService.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    // Keep the internal application id lowercase so Qt's own cache paths
    // converge with Reno119's explicit ~/.cache/reno119 storage.
    app.setApplicationName("reno119");
    app.setApplicationDisplayName("Reno119");
    app.setApplicationVersion(QStringLiteral(RENO119_VERSION));
    app.setOrganizationName("reno119");

    AppSettings appSettings;
    ThemeManager themeManager;
    PortalFileDialog portalFileDialog;
    GameModel games;
    ReFrameworkSupportService reFrameworkSupport;
    games.setReFrameworkSupportedTitles(reFrameworkSupport.supportedTitles());
    QObject::connect(&reFrameworkSupport, &ReFrameworkSupportService::supportChanged,
                     &games, [&games, &reFrameworkSupport] {
                         games.setReFrameworkSupportedTitles(reFrameworkSupport.supportedTitles());
                     });
    RenoDxCatalogService catalog;
    CoverService coverService;
    PCGamingWikiService pcGamingWiki;
    InstallerManager installer(&games, &catalog, &appSettings);
    OptiScalerIntegration optiIntegration(&games);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appSettings", &appSettings);
    engine.rootContext()->setContextProperty("themeManager", &themeManager);
    engine.rootContext()->setContextProperty("portalFileDialog", &portalFileDialog);
    engine.rootContext()->setContextProperty("gameModel", &games);
    engine.rootContext()->setContextProperty("installer", &installer);
    engine.rootContext()->setContextProperty("renoDxCatalog", &catalog);
    engine.rootContext()->setContextProperty("coverService", &coverService);
    engine.rootContext()->setContextProperty("pcGamingWiki", &pcGamingWiki);
    engine.rootContext()->setContextProperty("optiIntegration", &optiIntegration);
    engine.loadFromModule("Reno119", "Main");
    if (engine.rootObjects().isEmpty()) return -1;

    bool shutdownStarted = false;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
        if (shutdownStarted)
            return;
        shutdownStarted = true;

        // Stop Reno119-owned network work before QML begins tearing down.
        // This prevents reply callbacks from touching closed QSslSockets or
        // repopulating models while the engine is being destroyed.
        installer.shutdown();
        catalog.shutdown();
        coverService.shutdown();
        pcGamingWiki.shutdown();
        reFrameworkSupport.shutdown();

        // Destroy the QML root while deferred-delete events can still be
        // processed. This cancels outstanding ListView/Repeater incubation
        // before QQmlApplicationEngine itself is destructed.
        const auto roots = engine.rootObjects();
        for (QObject *rootObject : roots) {
            if (rootObject)
                rootObject->deleteLater();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        engine.collectGarbage();
    });

    const int exitCode = app.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    engine.collectGarbage();
    return exitCode;
}
