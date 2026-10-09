#include "network/RenoDxUpdatePolicy.h"

#include <QtTest>

class RenoDxUpdatePolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void exactRevisionChangeIsActionable() {
        RenoDxUpdateTarget current;
        current.url = QStringLiteral("https://example.invalid/game.addon64");
        current.sourceKey = QStringLiteral("owner/repo:src/games/game");
        current.sourceRevision = QStringLiteral("new-revision");
        current.catalogMatched = true;
        current.exact = true;

        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            current.url,
            current.sourceKey,
            QStringLiteral("old-revision"),
            current);

        QVERIFY(decision.updateDetected);
        QVERIFY(decision.updateAvailable);
        QVERIFY(!decision.reviewRequired);
        QCOMPARE(decision.targetIdentity,
                 QStringLiteral("owner/repo:src/games/game@new-revision"));
    }

    void unchangedExactRevisionIsCurrent() {
        RenoDxUpdateTarget current;
        current.url = QStringLiteral("https://example.invalid/game.addon64");
        current.sourceKey = QStringLiteral("owner/repo:src/games/game");
        current.sourceRevision = QStringLiteral("same-revision");
        current.catalogMatched = true;
        current.exact = true;

        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            current.url,
            current.sourceKey,
            current.sourceRevision,
            current);

        QVERIFY(!decision.updateDetected);
        QVERIFY(!decision.updateAvailable);
        QVERIFY(!decision.comparisonUnknown);
    }

    void partialMatchNeverAutoUpdates() {
        RenoDxUpdateTarget current;
        current.url = QStringLiteral("https://example.invalid/control.addon64");
        current.sourceKey = QStringLiteral("owner/repo:src/games/control");
        current.sourceRevision = QStringLiteral("new-revision");
        current.catalogMatched = true;
        current.exact = false;
        current.requiresConfirmation = true;

        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            current.url,
            current.sourceKey,
            QStringLiteral("old-revision"),
            current);

        QVERIFY(decision.updateDetected);
        QVERIFY(!decision.updateAvailable);
        QVERIFY(decision.reviewRequired);
    }

    void externalInstallSuggestsTakeoverButDoesNotUpdate() {
        RenoDxUpdateTarget current;
        current.url = QStringLiteral("https://example.invalid/game.addon64");
        current.sourceKey = QStringLiteral("owner/repo:src/games/game");
        current.sourceRevision = QStringLiteral("current-revision");
        current.catalogMatched = true;
        current.exact = true;

        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, false, true,
            QString(),
            QString(),
            QString(),
            current);

        QVERIFY(!decision.updateDetected);
        QVERIFY(!decision.updateAvailable);
        QVERIFY(decision.takeoverSuggested);
    }

    void missingCatalogEntryLeavesManagedInstallUntouched() {
        RenoDxUpdateTarget current;
        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            QStringLiteral("https://example.invalid/old.addon64"),
            QStringLiteral("owner/repo:src/games/old"),
            QStringLiteral("old-revision"),
            current,
            QStringLiteral("https://example.invalid/generic-ue.addon64"));

        QVERIFY(!decision.updateDetected);
        QVERIFY(!decision.updateAvailable);
        QVERIFY(decision.catalogMissing);
    }

    void legacyDedicatedInstallDoesNotDowngradeToGeneric() {
        RenoDxUpdateTarget current;
        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            QStringLiteral("https://example.invalid/old-dedicated.addon64"),
            QString(),
            QString(),
            current,
            QStringLiteral("https://example.invalid/generic-ue.addon64"));

        QVERIFY(!decision.updateDetected);
        QVERIFY(!decision.updateAvailable);
        QVERIFY(decision.catalogMissing);
    }

    void legacyManagedInstallRequestsOneRefresh() {
        RenoDxUpdateTarget current;
        current.url = QStringLiteral("https://example.invalid/game.addon64");
        current.sourceKey = QStringLiteral("owner/repo:src/games/game");
        current.sourceRevision = QStringLiteral("current-revision");
        current.catalogMatched = true;
        current.exact = true;

        const auto decision = RenoDxUpdatePolicy::evaluate(
            true, true, false,
            current.url,
            QString(),
            QString(),
            current);

        QVERIFY(decision.updateDetected);
        QVERIFY(decision.updateAvailable);
        QVERIFY(decision.legacyRefreshRequired);
    }
};

QTEST_MAIN(RenoDxUpdatePolicyTests)
#include "RenoDxUpdatePolicyTests.moc"
