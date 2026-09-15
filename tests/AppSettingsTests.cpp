#include <QtTest>

#include "core/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

class AppSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void migratesLegacyWorkspaceState();
    void portableBackupUsesSchema3AndRestoresWorkspaceState();
    void prunesDefaultWorkspaceValues();
    void importsLegacySchema2WorkspaceState();

private:
    QString workspacePath() const;
    void clearSettings();
};

QString AppSettingsTests::workspacePath() const {
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME")) + QStringLiteral("/reno119/workspace-state.json");
}

void AppSettingsTests::clearSettings() {
    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    settings.clear();
    settings.sync();
    QFile::remove(workspacePath());
}

void AppSettingsTests::cleanup() {
    clearSettings();
}

void AppSettingsTests::migratesLegacyWorkspaceState() {
    clearSettings();

    const QVariantMap legacy{
        {QStringLiteral("gameId"), QStringLiteral("3280350")},
        {QStringLiteral("listY"), 42.5},
        {QStringLiteral("components"), QVariantMap{{QStringLiteral("3280350"), true}}},
        {QStringLiteral("tools"), true}
    };

    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    settings.setValue(QStringLiteral("workspace/viewState"), legacy);
    settings.sync();
    QVERIFY(settings.contains(QStringLiteral("workspace/viewState")));

    AppSettings appSettings;
    QCOMPARE(appSettings.viewState(), legacy);

    QSettings migrated(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    QVERIFY(!migrated.contains(QStringLiteral("workspace/viewState")));

    QFile file(workspacePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("reno119-workspace-state"));
    QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("state")).toObject().toVariantMap(), legacy);
}

void AppSettingsTests::portableBackupUsesSchema3AndRestoresWorkspaceState() {
    clearSettings();
    AppSettings appSettings;

    const QVariantMap original{
        {QStringLiteral("gameId"), QStringLiteral("1361210")},
        {QStringLiteral("listY"), 88.0},
        {QStringLiteral("positions"), QVariantMap{{QStringLiteral("1361210"), 315.25}}},
        {QStringLiteral("notes"), QVariantMap{{QStringLiteral("1361210"), true}}}
    };
    appSettings.saveViewState(original);
    appSettings.setThemeMode(QStringLiteral("amoled"));

    const QString backupPath = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME")) + QStringLiteral("/settings-backup.json");
    QVERIFY(appSettings.exportConfiguration(QUrl::fromLocalFile(backupPath)));

    QFile backup(backupPath);
    QVERIFY(backup.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(backup.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 3);
    QVERIFY(root.value(QStringLiteral("workspaceState")).isObject());
    QCOMPARE(root.value(QStringLiteral("workspaceState")).toObject().toVariantMap(), original);
    QVERIFY(!root.value(QStringLiteral("settings")).toObject().contains(QStringLiteral("workspace/viewState")));

    appSettings.saveViewState(QVariantMap{{QStringLiteral("gameId"), QStringLiteral("different")}});
    appSettings.setThemeMode(QStringLiteral("dark"));
    QVERIFY(appSettings.importConfiguration(QUrl::fromLocalFile(backupPath)));
    QCOMPARE(appSettings.viewState(), original);
    QCOMPARE(appSettings.themeMode(), QStringLiteral("amoled"));

    QSettings settings(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    QVERIFY(!settings.contains(QStringLiteral("workspace/viewState")));
}

void AppSettingsTests::prunesDefaultWorkspaceValues() {
    clearSettings();
    AppSettings appSettings;

    appSettings.saveViewState(QVariantMap{
        {QStringLiteral("gameId"), QStringLiteral("3280350")},
        {QStringLiteral("listY"), 0.0},
        {QStringLiteral("positions"), QVariantMap{{QStringLiteral("3280350"), 0.0}, {QStringLiteral("2527390"), 120.0}}},
        {QStringLiteral("components"), QVariantMap{{QStringLiteral("3280350"), false}, {QStringLiteral("2527390"), true}}},
        {QStringLiteral("notes"), QVariantMap{{QStringLiteral("3280350"), false}}},
        {QStringLiteral("tools"), false},
        {QStringLiteral("actions"), true}
    });

    const QVariantMap state = appSettings.viewState();
    QCOMPARE(state.value(QStringLiteral("gameId")).toString(), QStringLiteral("3280350"));
    QVERIFY(!state.contains(QStringLiteral("listY")));
    QCOMPARE(state.value(QStringLiteral("positions")).toMap(), QVariantMap{{QStringLiteral("2527390"), 120.0}});
    QCOMPARE(state.value(QStringLiteral("components")).toMap(), QVariantMap{{QStringLiteral("2527390"), true}});
    QVERIFY(!state.contains(QStringLiteral("notes")));
    QVERIFY(!state.contains(QStringLiteral("tools")));
    QCOMPARE(state.value(QStringLiteral("actions")).toBool(), true);
}

void AppSettingsTests::importsLegacySchema2WorkspaceState() {
    clearSettings();
    AppSettings appSettings;

    const QVariantMap legacy{
        {QStringLiteral("gameId"), QStringLiteral("2527390")},
        {QStringLiteral("actions"), true},
        {QStringLiteral("opti"), QVariantMap{{QStringLiteral("2527390"), true}}}
    };

    const QJsonObject settings{
        {QStringLiteral("appearance/themeMode"), QStringLiteral("dark")},
        {QStringLiteral("workspace/viewState"), QJsonObject::fromVariantMap(legacy)}
    };
    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("reno119-settings")},
        {QStringLiteral("schemaVersion"), 2},
        {QStringLiteral("settings"), settings}
    };

    const QString backupPath = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME")) + QStringLiteral("/legacy-backup.json");
    QFile file(backupPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
    file.close();

    QVERIFY(appSettings.importConfiguration(QUrl::fromLocalFile(backupPath)));
    QCOMPARE(appSettings.viewState(), legacy);

    QSettings migrated(QStringLiteral("Reno119"), QStringLiteral("Reno119"));
    QVERIFY(!migrated.contains(QStringLiteral("workspace/viewState")));
}

int main(int argc, char **argv) {
    QTemporaryDir configDir;
    if (!configDir.isValid())
        return 2;

    qputenv("XDG_CONFIG_HOME", configDir.path().toLocal8Bit());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, configDir.path() + QStringLiteral("/qsettings"));
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir.path() + QStringLiteral("/qsettings"));

    QCoreApplication app(argc, argv);
    AppSettingsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "AppSettingsTests.moc"
