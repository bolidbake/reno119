#include <QtTest>

#include "core/GameModel.h"
#include "integrations/OptiScalerIntegration.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

class OptiScalerIntegrationTests : public QObject {
    Q_OBJECT

private:
    static GameInfo makeGame(const QTemporaryDir &dir) {
        GameInfo game;
        game.name = QStringLiteral("OptiScaler Test Game");
        game.appId = QStringLiteral("test-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        game.source = QStringLiteral("Custom");
        game.exePath = dir.filePath(QStringLiteral("game.exe"));
        QFile exe(game.exePath);
        exe.open(QIODevice::WriteOnly);
        exe.write("test");
        exe.close();
        return game;
    }

    static void writeText(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        file.close();
    }

private slots:
    void readsCurrentMenuHotkey() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const GameInfo game = makeGame(dir);
        writeText(dir.filePath(QStringLiteral("OptiScaler.ini")),
                  "[Menu]\nShortcutKey=0x79\n");

        GameModel model;
        model.setGamesForTesting({game});
        OptiScalerIntegration opti(&model);

        const QVariantMap hotkey = opti.overlayHotkey(0);
        QCOMPARE(hotkey.value(QStringLiteral("available")).toBool(), true);
        QCOMPARE(hotkey.value(QStringLiteral("code")).toInt(), 0x79);
        QCOMPARE(hotkey.value(QStringLiteral("name")).toString(), QStringLiteral("F10"));
    }

    void writesCurrentMenuHotkey() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const GameInfo game = makeGame(dir);
        const QString iniPath = dir.filePath(QStringLiteral("OptiScaler.ini"));
        writeText(iniPath, "[Menu]\nShortcutKey=0x2D\n");

        GameModel model;
        model.setGamesForTesting({game});
        OptiScalerIntegration opti(&model);

        QVERIFY(opti.setOverlayHotkey(0, 0x78));
        QFile ini(iniPath);
        QVERIFY(ini.open(QIODevice::ReadOnly));
        const QByteArray text = ini.readAll();
        QVERIFY(text.contains("[Menu]"));
        QVERIFY(text.contains("ShortcutKey=0x78"));
        QCOMPARE(opti.overlayHotkey(0).value(QStringLiteral("code")).toInt(), 0x78);
    }

    void preservesLegacyRootHotkeyLayout() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const GameInfo game = makeGame(dir);
        const QString iniPath = dir.filePath(QStringLiteral("OptiScaler.ini"));
        writeText(iniPath, "ShortcutKey=0x2D\n[Upscalers]\nEnabled=true\n");

        GameModel model;
        model.setGamesForTesting({game});
        OptiScalerIntegration opti(&model);

        QVERIFY(opti.setOverlayHotkey(0, 0x77));
        QFile ini(iniPath);
        QVERIFY(ini.open(QIODevice::ReadOnly));
        const QByteArray text = ini.readAll();
        QVERIFY(text.startsWith("ShortcutKey=0x77"));
        QVERIFY(!text.contains("[Menu]"));
        QCOMPARE(opti.overlayHotkey(0).value(QStringLiteral("code")).toInt(), 0x77);
    }

    void exposesRestorePointMetadata() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const GameInfo game = makeGame(dir);

        const QJsonObject restorePoint{
            {QStringLiteral("snapshotDir"), QStringLiteral("/tmp/reno119-test-snapshot")},
            {QStringLiteral("createdAt"), QStringLiteral("2026-09-15T07:00:00Z")},
            {QStringLiteral("label"), QStringLiteral("Before test integration")}
        };
        const QJsonObject state{
            {QStringLiteral("restorePoints"), QJsonArray{restorePoint}},
            {QStringLiteral("history"), QJsonArray{QStringLiteral("test history")}}
        };
        writeText(dir.filePath(QStringLiteral(".reno119-optiscaler.json")),
                  QJsonDocument(state).toJson(QJsonDocument::Indented));

        GameModel model;
        model.setGamesForTesting({game});
        OptiScalerIntegration opti(&model);

        const QVariantList points = opti.restorePoints(0);
        QCOMPARE(points.size(), 1);
        const QVariantMap point = points.first().toMap();
        QCOMPARE(point.value(QStringLiteral("snapshotDir")).toString(), QStringLiteral("/tmp/reno119-test-snapshot"));
        QVERIFY(!point.value(QStringLiteral("createdDisplay")).toString().isEmpty());
        QCOMPARE(opti.history(0), QStringList{QStringLiteral("test history")});
    }

    void rejectsRestorePointOutsideGameHistory() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const GameInfo game = makeGame(dir);

        GameModel model;
        model.setGamesForTesting({game});
        OptiScalerIntegration opti(&model);

        QVERIFY(!opti.restorePoint(0, dir.path()));
        QVERIFY(opti.status().contains(QStringLiteral("outside this game's Reno119 history"), Qt::CaseInsensitive));
    }
};

QTEST_MAIN(OptiScalerIntegrationTests)
#include "OptiScalerIntegrationTests.moc"
