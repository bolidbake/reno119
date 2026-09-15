#include "core/ModDetectionService.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ModDetectionServiceTests final : public QObject {
    Q_OBJECT

private:
    static QString writeFixture(QTemporaryDir &dir, const QString &name, const QByteArray &bytes) {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(bytes);
        file.close();
        return path;
    }

private slots:
    void reshadeSignatureRequiresReShadeEvidence() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString positive = writeFixture(dir, QStringLiteral("dxgi.dll"), QByteArray("prefix ReShade 6.0 suffix"));
        const QString negative = writeFixture(dir, QStringLiteral("d3d11.dll"), QByteArray("ordinary proxy loader"));
        QVERIFY(ModDetectionService::looksLikeReShadeBinary(positive));
        QVERIFY(!ModDetectionService::looksLikeReShadeBinary(negative));
    }

    void reFrameworkRecognizesKnownMarkers() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(ModDetectionService::looksLikeReFrameworkBinary(
            writeFixture(dir, QStringLiteral("dinput8.dll"), QByteArray("PrAyDoG REFramework"))));
        QVERIFY(!ModDetectionService::looksLikeReFrameworkBinary(
            writeFixture(dir, QStringLiteral("other.dll"), QByteArray("generic dinput proxy"))));
    }

    void optiScalerRejectsSingleIncidentalName() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("Ultimate ASI Loader ... OptiScaler ..."));
        QVERIFY(!ModDetectionService::looksLikeOptiScalerBinary(path));
    }

    void optiScalerAcceptsCompoundSignature() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("OptiScaler ... OptiFG ..."));
        QVERIFY(ModDetectionService::looksLikeOptiScalerBinary(path));
    }

    void optiProxyDetectionDoesNotClaimGranblueStyleAsiLoader() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("Ultimate ASI Loader ... OptiScaler ...")).isEmpty());
        QCOMPARE(ModDetectionService::detectOptiScalerProxy(dir.path(), QString(), QString(), false), QString());
    }

    void optiProxyDetectionRecognizesRealRenamedProxy() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("OptiScaler ... OptiScaler.ini ... OptiFG ...")).isEmpty());
        QCOMPARE(ModDetectionService::detectOptiScalerProxy(dir.path(), QString(), QString(), false), QStringLiteral("winmm.dll"));
    }

    void optiIniFallbackRequiresSingleCandidate() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("unidentified proxy")).isEmpty());
        QCOMPARE(ModDetectionService::detectOptiScalerProxy(dir.path(), QString(), QString(), true), QStringLiteral("winmm.dll"));

        QVERIFY(!writeFixture(dir, QStringLiteral("version.dll"), QByteArray("another unidentified proxy")).isEmpty());
        QCOMPARE(ModDetectionService::detectOptiScalerProxy(dir.path(), QString(), QString(), true), QString());
    }

    void reshadeProxyIsExcludedFromOptiFallback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeFixture(dir, QStringLiteral("dxgi.dll"), QByteArray("ReShade")).isEmpty());
        QVERIFY(!writeFixture(dir, QStringLiteral("winmm.dll"), QByteArray("unidentified proxy")).isEmpty());
        QCOMPARE(ModDetectionService::detectOptiScalerProxy(dir.path(), QStringLiteral("dxgi.dll"), QString(), true), QStringLiteral("winmm.dll"));
    }
};

QTEST_MAIN(ModDetectionServiceTests)
#include "ModDetectionServiceTests.moc"
