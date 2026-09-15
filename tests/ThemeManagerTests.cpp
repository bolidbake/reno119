#include <QtTest>

#include "core/ThemeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

class ThemeManagerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void materialUsesBuiltInFallbackWhenMissing();
    void partialMaterialPaletteOverridesRolesAndMode();
    void noctaliaPaletteLoadsGeneratedRoleMap();
    void noctaliaPureBlackPaletteIsDetected();
    void invalidNoctaliaPaletteFallsBackSafely();
};

void ThemeManagerTests::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    const QString root = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/reno119");
    QDir(root).removeRecursively();
    QVERIFY(QDir().mkpath(root));
}

void ThemeManagerTests::materialUsesBuiltInFallbackWhenMissing() {
    ThemeManager manager;
    QFile::remove(manager.materialThemePath());
    manager.reloadMaterial();

    QVERIFY(!manager.materialCustomReady());
    QVERIFY(manager.materialDark());
    QCOMPARE(manager.materialPalette().value(QStringLiteral("primary")).toString(), QStringLiteral("#d0bcff"));
    QVERIFY(manager.materialStatus().contains(QStringLiteral("built-in"), Qt::CaseInsensitive));
}

void ThemeManagerTests::partialMaterialPaletteOverridesRolesAndMode() {
    ThemeManager manager;
    QFile file(manager.materialThemePath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({
        "name": "Test Light",
        "mode": "light",
        "colors": {
            "primary": "#123456",
            "surface": "#fefefe",
            "onSurface": "#101010"
        }
    })json");
    file.close();

    manager.reloadMaterial();
    QVERIFY(manager.materialCustomReady());
    QVERIFY(!manager.materialDark());
    QCOMPARE(manager.materialPalette().value(QStringLiteral("primary")).toString(), QStringLiteral("#123456"));
    QCOMPARE(manager.materialPalette().value(QStringLiteral("on_surface")).toString(), QStringLiteral("#101010"));
    // Missing roles inherit the built-in palette.
    QCOMPARE(manager.materialPalette().value(QStringLiteral("outline")).toString(), QStringLiteral("#79747e"));
}

void ThemeManagerTests::noctaliaPaletteLoadsGeneratedRoleMap() {
    ThemeManager manager;
    QFile file(manager.noctaliaThemePath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({
        "format": "reno119-material-theme",
        "name": "Noctalia Test",
        "mode": "dark",
        "colors": {
            "mPrimary": "#abcdef",
            "surface": "#121212",
            "on_surface": "#eeeeee",
            "surface_container_high": "#303030"
        }
    })json");
    file.close();

    manager.reloadNoctalia();
    QVERIFY(manager.noctaliaReady());
    QVERIFY(manager.noctaliaDark());
    QVERIFY(!manager.noctaliaPureBlack());
    QCOMPARE(manager.noctaliaPalette().value(QStringLiteral("primary")).toString(), QStringLiteral("#abcdef"));
    QVERIFY(manager.noctaliaStatus().contains(QStringLiteral("Noctalia Test")));
}

void ThemeManagerTests::noctaliaPureBlackPaletteIsDetected() {
    ThemeManager manager;
    QFile file(manager.noctaliaThemePath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({
        "format": "reno119-material-theme",
        "name": "Noctalia Pure Black",
        "mode": "dark",
        "colors": {
            "background": "#000000",
            "surface": "#000000",
            "surface_container_low": "#08060d",
            "surface_container": "#110d1a",
            "on_surface": "#e8d8ff",
            "primary": "#b58fff"
        }
    })json");
    file.close();

    manager.reloadNoctalia();
    QVERIFY(manager.noctaliaReady());
    QVERIFY(manager.noctaliaDark());
    QVERIFY(manager.noctaliaPureBlack());
    QVERIFY(manager.noctaliaStatus().contains(QStringLiteral("Pure Black"), Qt::CaseInsensitive));
    QCOMPARE(manager.noctaliaPalette().value(QStringLiteral("surface")).toString(), QStringLiteral("#000000"));
    QCOMPARE(manager.noctaliaPalette().value(QStringLiteral("surface_container_low")).toString(), QStringLiteral("#08060d"));
}

void ThemeManagerTests::invalidNoctaliaPaletteFallsBackSafely() {
    ThemeManager manager;
    QFile file(manager.noctaliaThemePath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("{ this is not json");
    file.close();

    manager.reloadNoctalia();
    QVERIFY(!manager.noctaliaReady());
    QVERIFY(manager.noctaliaDark());
    QCOMPARE(manager.noctaliaPalette().value(QStringLiteral("surface")).toString(), QStringLiteral("#1c1b1f"));
    QVERIFY(manager.noctaliaStatus().contains(QStringLiteral("ignored"), Qt::CaseInsensitive));
}

QTEST_MAIN(ThemeManagerTests)
#include "ThemeManagerTests.moc"
