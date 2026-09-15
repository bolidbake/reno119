#include "network/RenoDxTitleMatcher.h"

#include <QtTest>

class RenoDxTitleMatcherTests final : public QObject {
    Q_OBJECT

private slots:
    void peaceWalkerDoesNotResolveToDelta() {
        const QVector<RenoDxCatalogEntry> entries{
            {QStringLiteral("METAL GEAR SOLID Δ: SNAKE EATER"), QStringLiteral("https://example.invalid/mgs-delta.addon64")}
        };
        const auto match = RenoDxTitleMatcher::resolve(QStringLiteral("Metal Gear Solid: Peace Walker HD"), entries);
        QVERIFY(!match.matched());
    }

    void exactCatalogTitleWins() {
        const QVector<RenoDxCatalogEntry> entries{
            {QStringLiteral("METAL GEAR SOLID Δ: SNAKE EATER"), QStringLiteral("https://example.invalid/mgs-delta.addon64")}
        };
        const auto match = RenoDxTitleMatcher::resolve(QStringLiteral("METAL GEAR SOLID Δ: SNAKE EATER"), entries);
        QVERIFY(match.matched());
        QVERIFY(match.method == RenoDxTitleMatch::Method::ExactTitle);
        QVERIFY(match.exact());
        QVERIFY(!match.requiresConfirmation());
    }

    void subtitleStrippedExactStillWorksButNeedsConfirmation() {
        const QVector<RenoDxCatalogEntry> entries{
            {QStringLiteral("Control"), QStringLiteral("https://example.invalid/control.addon64")}
        };
        const auto match = RenoDxTitleMatcher::resolve(QStringLiteral("Control: Ultimate Edition"), entries);
        QVERIFY(match.matched());
        QCOMPARE(match.title, QStringLiteral("Control"));
        QVERIFY(match.method == RenoDxTitleMatch::Method::SubtitleExact);
        QVERIFY(match.requiresConfirmation());
    }

    void longerEditionCanUseUniqueSafePrefix() {
        const QVector<RenoDxCatalogEntry> entries{
            {QStringLiteral("NieR Replicant ver.1.22474487139"), QStringLiteral("https://example.invalid/nier.addon64")}
        };
        const auto match = RenoDxTitleMatcher::resolve(QStringLiteral("NieR Replicant ver.1.22474487139 Deluxe Edition"), entries);
        QVERIFY(match.matched());
        QVERIFY(match.method == RenoDxTitleMatch::Method::Prefix);
        QVERIFY(match.requiresConfirmation());
    }

    void equalPrefixCandidatesResolveToNothing() {
        const QVector<RenoDxCatalogEntry> entries{
            {QStringLiteral("Super Game Alpha"), QStringLiteral("https://example.invalid/a.addon64")},
            {QStringLiteral("Super Game Beta"), QStringLiteral("https://example.invalid/b.addon64")}
        };
        const auto match = RenoDxTitleMatcher::resolve(QStringLiteral("Super Game"), entries);
        QVERIFY(!match.matched());
    }
};

QTEST_MAIN(RenoDxTitleMatcherTests)
#include "RenoDxTitleMatcherTests.moc"
