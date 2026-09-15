#include "steam/SteamScanner.h"
#include "graphics/PeParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class ResolutionRegressionTests final : public QObject {
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &data) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(data) == data.size();
    }

private slots:
    void movedSteamCompatdataIsFoundAcrossLibraries() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString installLibrary = dir.filePath(QStringLiteral("LibraryA"));
        const QString prefixLibrary = dir.filePath(QStringLiteral("LibraryB"));
        QDir().mkpath(QDir(installLibrary).filePath(QStringLiteral("steamapps")));
        const QString expected = QDir(prefixLibrary).filePath(QStringLiteral("steamapps/compatdata/1097840/pfx"));
        QDir().mkpath(QDir(expected).filePath(QStringLiteral("drive_c")));

        QStringList candidates;
        QString resolvedLibrary;
        const QString resolved = SteamScanner::findProtonPrefix(
            QStringLiteral("1097840"), {installLibrary, prefixLibrary}, installLibrary,
            &candidates, &resolvedLibrary);

        QCOMPARE(QDir::cleanPath(resolved), QDir::cleanPath(expected));
        QCOMPARE(QDir::cleanPath(resolvedLibrary), QDir::cleanPath(QDir(prefixLibrary).absolutePath()));
        QCOMPARE(candidates.size(), 2);
        QVERIFY(candidates.first().contains(QStringLiteral("LibraryA")));
        QVERIFY(candidates.last().contains(QStringLiteral("LibraryB")));
    }

    void ubisoftTitleInitialismsCoverRecentEdgeCases() {
        QCOMPARE(SteamScanner::titleInitialism(QStringLiteral("Avatar: Frontiers of Pandora")), QStringLiteral("afop"));
        QCOMPARE(SteamScanner::titleInitialism(QStringLiteral("Watch Dogs 2")), QStringLiteral("wd2"));
    }

    void graphicsFallbackFindsDynamicD3D11Reference() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString exe = dir.filePath(QStringLiteral("WatchDogs2.exe"));
        QVERIFY(writeFile(exe, QByteArray("runtime loads D3D11.DLL through LoadLibrary")));
        QCOMPARE(PeParser::detectGraphicsApiFallback(exe), QStringLiteral("DirectX 11"));
    }

    void graphicsFallbackUsesAdjacentDx11ModuleName() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString exe = dir.filePath(QStringLiteral("WatchDogs2.exe"));
        QVERIFY(writeFile(exe, QByteArray("no static graphics import here")));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("GFSDK_SSAO_D3D11.win64.dll")), QByteArray("module")));
        QCOMPARE(PeParser::detectGraphicsApiFallback(exe), QStringLiteral("DirectX 11"));
    }

    void graphicsFallbackIgnoresProxyDllFilenames() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString exe = dir.filePath(QStringLiteral("Game.exe"));
        QVERIFY(writeFile(exe, QByteArray("no graphics runtime strings")));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("dxgi.dll")), QByteArray("proxy")));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("d3d11.dll")), QByteArray("proxy")));
        QCOMPARE(PeParser::detectGraphicsApiFallback(exe), QStringLiteral("Unknown"));
    }

    void graphicsFallbackRejectsAmbiguousAdjacentRendererNames() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString exe = dir.filePath(QStringLiteral("Game.exe"));
        QVERIFY(writeFile(exe, QByteArray("no graphics runtime strings")));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("renderer_dx11_module.dll")), QByteArray("module")));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("renderer_dx12_module.dll")), QByteArray("module")));
        QCOMPARE(PeParser::detectGraphicsApiFallback(exe), QStringLiteral("Unknown"));
    }

    void ubisoftLauncherExecutablesAreRejectedAsGames() {
        QVERIFY(SteamScanner::isLikelyLauncherExecutable(QStringLiteral("/prefix/UbisoftConnect.exe")));
        QVERIFY(SteamScanner::isLikelyLauncherExecutable(QStringLiteral("/prefix/upc.exe")));
        QVERIFY(SteamScanner::isLikelyLauncherExecutable(QStringLiteral("/prefix/UplayWebCore.exe")));
        QVERIFY(!SteamScanner::isLikelyLauncherExecutable(QStringLiteral("/prefix/games/AFOP/afop.exe")));
        QVERIFY(!SteamScanner::isLikelyLauncherExecutable(QStringLiteral("/prefix/games/Watch_Dogs2/bin/WatchDogs2.exe")));
    }
};

QTEST_MAIN(ResolutionRegressionTests)
#include "ResolutionRegressionTests.moc"
