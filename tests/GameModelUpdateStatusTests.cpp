#include <QtTest>
#include <QSignalSpy>

#include "core/GameModel.h"

class GameModelUpdateStatusTests : public QObject {
    Q_OBJECT

private:
    static GameInfo game(const QString &name, const QString &appId) {
        GameInfo info;
        info.name = name;
        info.appId = appId;
        info.source = QStringLiteral("Steam");
        return info;
    }

private slots:
    void normalViewUpdatesRowWithoutReset() {
        GameModel model;
        model.setGamesForTesting({game(QStringLiteral("Alpha"), QStringLiteral("1")),
                                  game(QStringLiteral("Beta"), QStringLiteral("2"))});

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelAboutToBeReset);
        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy revisionSpy(&model, &GameModel::revisionChanged);

        model.setUpdateStatus(QStringLiteral("1"), true, true, false, false);

        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(revisionSpy.count(), 1);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), GameModel::UpdateCheckedRole).toBool(), true);
        QCOMPARE(model.data(model.index(0, 0), GameModel::ReShadeUpdateAvailableRole).toBool(), true);
        QCOMPARE(model.data(model.index(1, 0), GameModel::UpdateCheckedRole).toBool(), false);
    }

    void updatesFilterRebuildsMembership() {
        GameModel model;
        model.setGamesForTesting({game(QStringLiteral("Alpha"), QStringLiteral("1")),
                                  game(QStringLiteral("Beta"), QStringLiteral("2"))});
        model.setFilterMode(QStringLiteral("updates"));
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelAboutToBeReset);

        model.setUpdateStatus(QStringLiteral("1"), true, false, true, false);
        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), GameModel::AppIdRole).toString(), QStringLiteral("1"));

        model.setUpdateStatus(QStringLiteral("1"), true, false, false, false);
        QCOMPARE(resetSpy.count(), 2);
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_MAIN(GameModelUpdateStatusTests)
#include "GameModelUpdateStatusTests.moc"
