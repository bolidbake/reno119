#include "AppSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {
QVariantMap trueEntries(const QVariantMap &source) {
    QVariantMap result;
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        if (it.value().toBool())
            result.insert(it.key(), true);
    }
    return result;
}

QVariantMap nonZeroPositions(const QVariantMap &source) {
    QVariantMap result;
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        bool ok = false;
        const double value = it.value().toDouble(&ok);
        if (ok && value != 0.0)
            result.insert(it.key(), value);
    }
    return result;
}

QVariantMap normalizedWorkspaceState(const QVariantMap &state) {
    QVariantMap result;

    const QString gameId = state.value(QStringLiteral("gameId")).toString().trimmed();
    if (!gameId.isEmpty())
        result.insert(QStringLiteral("gameId"), gameId);

    bool listYOk = false;
    const double listY = state.value(QStringLiteral("listY")).toDouble(&listYOk);
    if (listYOk && listY != 0.0)
        result.insert(QStringLiteral("listY"), listY);

    const QVariantMap positions = nonZeroPositions(state.value(QStringLiteral("positions")).toMap());
    if (!positions.isEmpty())
        result.insert(QStringLiteral("positions"), positions);

    const QStringList expansionKeys{
        QStringLiteral("components"),
        QStringLiteral("notes"),
        QStringLiteral("restore"),
        QStringLiteral("opti")
    };
    for (const QString &key : expansionKeys) {
        const QVariantMap entries = trueEntries(state.value(key).toMap());
        if (!entries.isEmpty())
            result.insert(key, entries);
    }

    if (state.value(QStringLiteral("tools")).toBool())
        result.insert(QStringLiteral("tools"), true);
    if (state.value(QStringLiteral("actions")).toBool())
        result.insert(QStringLiteral("actions"), true);

    return result;
}
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent),
      m_settings(QStringLiteral("Reno119"), QStringLiteral("Reno119")),
      m_themeMode(m_settings.value(QStringLiteral("appearance/themeMode"),
                                   m_settings.value(QStringLiteral("appearance/amoledBlack"), false).toBool()
                                       ? QStringLiteral("amoled")
                                       : QStringLiteral("system")).toString()),
      m_amoledBlack(m_themeMode == QStringLiteral("amoled")),
      m_backupBeforeChanges(m_settings.value(QStringLiteral("safety/backupBeforeChanges"), true).toBool()),
      m_gameCoversEnabled(m_settings.value(QStringLiteral("appearance/gameCoversEnabled"), true).toBool()),
      m_pursuitModeEnabled(m_settings.value(QStringLiteral("appearance/pursuitModeEnabled"), false).toBool()),
      m_customReShadeSource(m_settings.value(QStringLiteral("reshade/custom/source"), QStringLiteral("version")).toString()),
      m_customReShadeVersion(m_settings.value(QStringLiteral("reshade/custom/version"), QStringLiteral("")).toString()),
      m_customReShadeUrl(m_settings.value(QStringLiteral("reshade/custom/url"), QStringLiteral("")).toString()),
      m_customReShadeFile(m_settings.value(QStringLiteral("reshade/custom/file"), QStringLiteral("")).toString()),
      m_updateChecksOnStartup(m_settings.value(QStringLiteral("updates/checkOnStartup"), false).toBool()) {
    if (m_themeMode != QStringLiteral("system") &&
        m_themeMode != QStringLiteral("dark") &&
        m_themeMode != QStringLiteral("amoled") &&
        m_themeMode != QStringLiteral("material") &&
        m_themeMode != QStringLiteral("noctalia"))
        m_themeMode = QStringLiteral("system");
    m_amoledBlack = m_themeMode == QStringLiteral("amoled");
    migrateLegacyWorkspaceState();
}

void AppSettings::writeValue(const QString &key, const QVariant &value) {
    m_settings.setValue(key, value);
}

QString AppSettings::themeMode() const { return m_themeMode; }
bool AppSettings::amoledBlack() const { return m_themeMode == QStringLiteral("amoled"); }
bool AppSettings::backupBeforeChanges() const { return m_backupBeforeChanges; }
bool AppSettings::gameCoversEnabled() const { return m_gameCoversEnabled; }
bool AppSettings::pursuitModeEnabled() const { return m_pursuitModeEnabled; }
QString AppSettings::customReShadeSource() const { return m_customReShadeSource; }
QString AppSettings::customReShadeVersion() const { return m_customReShadeVersion; }
QString AppSettings::customReShadeUrl() const { return m_customReShadeUrl; }
QString AppSettings::customReShadeFile() const { return m_customReShadeFile; }
QString AppSettings::settingsTransferStatus() const { return m_settingsTransferStatus; }
bool AppSettings::updateChecksOnStartup() const { return m_updateChecksOnStartup; }

bool AppSettings::customReShadeConfigured() const {
    if (m_customReShadeSource == QStringLiteral("version"))
        return !m_customReShadeVersion.trimmed().isEmpty();
    if (m_customReShadeSource == QStringLiteral("url")) {
        const QUrl url = QUrl::fromUserInput(m_customReShadeUrl.trimmed());
        return url.isValid() && !url.isLocalFile() && !url.scheme().isEmpty();
    }
    if (m_customReShadeSource == QStringLiteral("file"))
        return QFileInfo::exists(m_customReShadeFile.trimmed());
    return false;
}

QString AppSettings::customReShadeSummary() const {
    if (!customReShadeConfigured())
        return QStringLiteral("Custom (not configured)");

    if (m_customReShadeSource == QStringLiteral("version"))
        return QStringLiteral("Custom — %1").arg(m_customReShadeVersion.trimmed());
    if (m_customReShadeSource == QStringLiteral("url"))
        return QStringLiteral("Custom — URL");
    if (m_customReShadeSource == QStringLiteral("file"))
        return QStringLiteral("Custom — %1").arg(QFileInfo(m_customReShadeFile.trimmed()).fileName());
    return QStringLiteral("Custom");
}


void AppSettings::setUpdateChecksOnStartup(bool enabled) {
    if (m_updateChecksOnStartup == enabled)
        return;
    m_updateChecksOnStartup = enabled;
    writeValue(QStringLiteral("updates/checkOnStartup"), enabled);
    emit updatePreferencesChanged();
}

QString AppSettings::skippedUpdateTarget(const QString &gameId, const QString &component) const {
    if (gameId.trimmed().isEmpty() || component.trimmed().isEmpty())
        return {};
    const QString key = QStringLiteral("updates/skipped/") +
        QString::fromLatin1(QUrl::toPercentEncoding(gameId)) + QLatin1Char('/') +
        QString::fromLatin1(QUrl::toPercentEncoding(component.toLower()));
    return m_settings.value(key).toString();
}

bool AppSettings::skipUpdateTarget(const QString &gameId, const QString &component, const QString &target) {
    if (gameId.trimmed().isEmpty() || component.trimmed().isEmpty() || target.trimmed().isEmpty())
        return false;
    const QString key = QStringLiteral("updates/skipped/") +
        QString::fromLatin1(QUrl::toPercentEncoding(gameId)) + QLatin1Char('/') +
        QString::fromLatin1(QUrl::toPercentEncoding(component.toLower()));
    m_settings.setValue(key, target);
    m_settings.sync();
    emit updatePreferencesChanged();
    return m_settings.status() == QSettings::NoError;
}

bool AppSettings::clearSkippedUpdateTarget(const QString &gameId, const QString &component) {
    if (gameId.trimmed().isEmpty() || component.trimmed().isEmpty())
        return false;
    const QString key = QStringLiteral("updates/skipped/") +
        QString::fromLatin1(QUrl::toPercentEncoding(gameId)) + QLatin1Char('/') +
        QString::fromLatin1(QUrl::toPercentEncoding(component.toLower()));
    m_settings.remove(key);
    m_settings.sync();
    emit updatePreferencesChanged();
    return m_settings.status() == QSettings::NoError;
}

void AppSettings::setThemeMode(const QString &mode) {
    const QString normalized = mode.trimmed().toLower();
    QString finalValue = QStringLiteral("system");
    if (normalized == QStringLiteral("dark") || normalized == QStringLiteral("reno-dark"))
        finalValue = QStringLiteral("dark");
    else if (normalized == QStringLiteral("amoled"))
        finalValue = QStringLiteral("amoled");
    else if (normalized == QStringLiteral("material") || normalized == QStringLiteral("material3") || normalized == QStringLiteral("material-3"))
        finalValue = QStringLiteral("material");
    else if (normalized == QStringLiteral("noctalia"))
        finalValue = QStringLiteral("noctalia");

    if (m_themeMode == finalValue)
        return;

    const bool oldAmoled = amoledBlack();
    m_themeMode = finalValue;
    m_amoledBlack = amoledBlack();
    writeValue(QStringLiteral("appearance/themeMode"), finalValue);
    // Keep the legacy key synchronized for older Reno119 builds.
    writeValue(QStringLiteral("appearance/amoledBlack"), m_amoledBlack);
    emit themeModeChanged();
    if (oldAmoled != m_amoledBlack)
        emit amoledBlackChanged();
}

void AppSettings::setAmoledBlack(bool enabled) {
    setThemeMode(enabled ? QStringLiteral("amoled") : QStringLiteral("dark"));
}

void AppSettings::setBackupBeforeChanges(bool enabled) {
    if (m_backupBeforeChanges == enabled)
        return;
    m_backupBeforeChanges = enabled;
    writeValue(QStringLiteral("safety/backupBeforeChanges"), enabled);
    emit backupBeforeChangesChanged();
}

void AppSettings::setGameCoversEnabled(bool enabled) {
    if (m_gameCoversEnabled == enabled)
        return;
    m_gameCoversEnabled = enabled;
    writeValue(QStringLiteral("appearance/gameCoversEnabled"), enabled);
    emit gameCoversEnabledChanged();
}

void AppSettings::setPursuitModeEnabled(bool enabled) {
    if (m_pursuitModeEnabled == enabled)
        return;
    m_pursuitModeEnabled = enabled;
    writeValue(QStringLiteral("appearance/pursuitModeEnabled"), enabled);
    emit pursuitModeEnabledChanged();
}

void AppSettings::setCustomReShadeSource(const QString &source) {
    const QString normalized = source.trimmed().toLower();
    const QString finalValue = (normalized == QStringLiteral("url") || normalized == QStringLiteral("file"))
        ? normalized
        : QStringLiteral("version");
    if (m_customReShadeSource == finalValue)
        return;
    m_customReShadeSource = finalValue;
    writeValue(QStringLiteral("reshade/custom/source"), finalValue);
    emit customReShadeChanged();
}

void AppSettings::setCustomReShadeVersion(const QString &version) {
    if (m_customReShadeVersion == version)
        return;
    m_customReShadeVersion = version;
    writeValue(QStringLiteral("reshade/custom/version"), version);
    emit customReShadeChanged();
}

void AppSettings::setCustomReShadeUrl(const QString &url) {
    if (m_customReShadeUrl == url)
        return;
    m_customReShadeUrl = url;
    writeValue(QStringLiteral("reshade/custom/url"), url);
    emit customReShadeChanged();
}

void AppSettings::setCustomReShadeFile(const QString &path) {
    if (m_customReShadeFile == path)
        return;
    m_customReShadeFile = path;
    writeValue(QStringLiteral("reshade/custom/file"), path);
    emit customReShadeChanged();
}

QString AppSettings::localPathFromUrl(const QUrl &url) const {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QUrl AppSettings::filePickerFolder(const QString &path) const {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return {};

    const QFileInfo info(trimmed);
    QString folderPath;
    if (info.exists() && info.isDir())
        folderPath = info.absoluteFilePath();
    else
        folderPath = info.absolutePath();

    if (folderPath.isEmpty())
        return {};
    return QUrl::fromLocalFile(folderPath);
}


bool AppSettings::isPortableConfigurationKey(const QString &key) {
    return key.startsWith(QStringLiteral("gameNotes/")) ||
           key.startsWith(QStringLiteral("appearance/")) ||
           key.startsWith(QStringLiteral("safety/")) ||
           key.startsWith(QStringLiteral("reshade/")) ||
           key.startsWith(QStringLiteral("games/")) ||
           key.startsWith(QStringLiteral("customPrograms/")) ||
           key.startsWith(QStringLiteral("library/")) ||
           key.startsWith(QStringLiteral("verification/")) ||
           key.startsWith(QStringLiteral("updates/"));
}

void AppSettings::setSettingsTransferStatus(const QString &status) {
    if (m_settingsTransferStatus == status)
        return;
    m_settingsTransferStatus = status;
    emit settingsTransferStatusChanged();
}

void AppSettings::reloadFromSettings() {
    const QString oldTheme = m_themeMode;
    const bool oldBackup = m_backupBeforeChanges;
    const bool oldCovers = m_gameCoversEnabled;
    const bool oldPursuit = m_pursuitModeEnabled;
    const QString oldSource = m_customReShadeSource;
    const QString oldVersion = m_customReShadeVersion;
    const QString oldUrl = m_customReShadeUrl;
    const QString oldFile = m_customReShadeFile;
    const bool oldUpdateChecks = m_updateChecksOnStartup;

    m_themeMode = m_settings.value(QStringLiteral("appearance/themeMode"),
                                   m_settings.value(QStringLiteral("appearance/amoledBlack"), false).toBool()
                                       ? QStringLiteral("amoled")
                                       : QStringLiteral("system")).toString();
    if (m_themeMode != QStringLiteral("system") &&
        m_themeMode != QStringLiteral("dark") &&
        m_themeMode != QStringLiteral("amoled") &&
        m_themeMode != QStringLiteral("material") &&
        m_themeMode != QStringLiteral("noctalia"))
        m_themeMode = QStringLiteral("system");
    m_amoledBlack = m_themeMode == QStringLiteral("amoled");
    m_backupBeforeChanges = m_settings.value(QStringLiteral("safety/backupBeforeChanges"), true).toBool();
    m_gameCoversEnabled = m_settings.value(QStringLiteral("appearance/gameCoversEnabled"), true).toBool();
    m_pursuitModeEnabled = m_settings.value(QStringLiteral("appearance/pursuitModeEnabled"), false).toBool();
    m_customReShadeSource = m_settings.value(QStringLiteral("reshade/custom/source"), QStringLiteral("version")).toString();
    m_customReShadeVersion = m_settings.value(QStringLiteral("reshade/custom/version"), QString()).toString();
    m_customReShadeUrl = m_settings.value(QStringLiteral("reshade/custom/url"), QString()).toString();
    m_customReShadeFile = m_settings.value(QStringLiteral("reshade/custom/file"), QString()).toString();
    m_updateChecksOnStartup = m_settings.value(QStringLiteral("updates/checkOnStartup"), false).toBool();

    if (oldTheme != m_themeMode) {
        emit themeModeChanged();
        emit amoledBlackChanged();
    }
    if (oldBackup != m_backupBeforeChanges)
        emit backupBeforeChangesChanged();
    if (oldCovers != m_gameCoversEnabled)
        emit gameCoversEnabledChanged();
    if (oldPursuit != m_pursuitModeEnabled)
        emit pursuitModeEnabledChanged();
    if (oldSource != m_customReShadeSource || oldVersion != m_customReShadeVersion ||
        oldUrl != m_customReShadeUrl || oldFile != m_customReShadeFile)
        emit customReShadeChanged();
    if (oldUpdateChecks != m_updateChecksOnStartup)
        emit updatePreferencesChanged();
}

bool AppSettings::exportConfiguration(const QUrl &fileUrl) {
    QString path = localPathFromUrl(fileUrl).trimmed();
    if (path.isEmpty()) {
        setSettingsTransferStatus(QStringLiteral("Choose a destination for the Reno119 backup."));
        return false;
    }
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");

    QJsonObject settingsObject;
    for (const QString &key : m_settings.allKeys()) {
        if (isPortableConfigurationKey(key))
            settingsObject.insert(key, QJsonValue::fromVariant(m_settings.value(key)));
    }

    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("reno119-settings")},
        {QStringLiteral("schemaVersion"), 3},
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
        {QStringLiteral("exportedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("settings"), settingsObject},
        {QStringLiteral("workspaceState"), QJsonObject::fromVariantMap(viewState())}
    };

    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        setSettingsTransferStatus(QStringLiteral("Could not open the Reno119 backup for writing: %1").arg(file.errorString()));
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        setSettingsTransferStatus(QStringLiteral("Could not save the Reno119 backup."));
        return false;
    }

    setSettingsTransferStatus(QStringLiteral("Backup exported to %1").arg(path));
    return true;
}

bool AppSettings::importConfiguration(const QUrl &fileUrl) {
    const QString path = localPathFromUrl(fileUrl).trimmed();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        setSettingsTransferStatus(QStringLiteral("Could not open the Reno119 backup file."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setSettingsTransferStatus(QStringLiteral("Invalid Reno119 backup JSON: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("reno119-settings") ||
        (schemaVersion != 1 && schemaVersion != 2 && schemaVersion != 3) ||
        !root.value(QStringLiteral("settings")).isObject()) {
        setSettingsTransferStatus(QStringLiteral("This file is not a supported Reno119 backup/settings export."));
        return false;
    }

    const QJsonObject imported = root.value(QStringLiteral("settings")).toObject();
    QVariantMap importedWorkspaceState;
    bool hasImportedWorkspaceState = false;
    if (schemaVersion >= 3 && root.value(QStringLiteral("workspaceState")).isObject()) {
        importedWorkspaceState = root.value(QStringLiteral("workspaceState")).toObject().toVariantMap();
        hasImportedWorkspaceState = true;
    } else if (imported.value(QStringLiteral("workspace/viewState")).isObject()) {
        // v1/v2 backups stored the workspace map inside QSettings. Import it
        // directly into the new JSON workspace file instead of recreating the
        // opaque @Variant entry in reno119.conf.
        importedWorkspaceState = imported.value(QStringLiteral("workspace/viewState")).toObject().toVariantMap();
        hasImportedWorkspaceState = true;
    }

    const QStringList existingKeys = m_settings.allKeys();
    for (const QString &key : existingKeys) {
        if (isPortableConfigurationKey(key) || key.startsWith(QStringLiteral("workspace/")))
            m_settings.remove(key);
    }

    int importedCount = 0;
    for (auto it = imported.constBegin(); it != imported.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("workspace/")) || !isPortableConfigurationKey(it.key()))
            continue;
        m_settings.setValue(it.key(), it.value().toVariant());
        ++importedCount;
    }
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        setSettingsTransferStatus(QStringLiteral("Reno119 could not commit the imported settings."));
        return false;
    }

    const bool workspaceOk = hasImportedWorkspaceState
        ? writeWorkspaceStateFile(importedWorkspaceState)
        : clearWorkspaceStateFile();
    if (!workspaceOk) {
        setSettingsTransferStatus(QStringLiteral("Settings were imported, but Reno119 could not save the workspace state."));
        return false;
    }
    if (hasImportedWorkspaceState)
        ++importedCount;

    reloadFromSettings();
    setSettingsTransferStatus(QStringLiteral("Imported %1 backup entries. Refreshing the library applies per-game metadata.").arg(importedCount));
    return true;
}

QString AppSettings::gameNotes(const QString &gameId) const {
    if (gameId.isEmpty()) return {};
    return m_settings.value(QStringLiteral("gameNotes/") + QString::fromLatin1(QUrl::toPercentEncoding(gameId))).toString();
}

bool AppSettings::setGameNotes(const QString &gameId, const QString &notes) {
    if (gameId.isEmpty()) return false;
    m_settings.setValue(QStringLiteral("gameNotes/") + QString::fromLatin1(QUrl::toPercentEncoding(gameId)), notes);
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
}

QString AppSettings::workspaceStatePath() const {
    QString root = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (root.isEmpty())
        root = QDir::homePath() + QStringLiteral("/.config");
    return root + QStringLiteral("/reno119/workspace-state.json");
}

bool AppSettings::readWorkspaceStateFile(QVariantMap *state) const {
    if (state)
        state->clear();

    QFile file(workspaceStatePath());
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("reno119-workspace-state") ||
        root.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
        !root.value(QStringLiteral("state")).isObject())
        return false;

    if (state)
        *state = root.value(QStringLiteral("state")).toObject().toVariantMap();
    return true;
}

bool AppSettings::writeWorkspaceStateFile(const QVariantMap &state) const {
    const QString path = workspaceStatePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("reno119-workspace-state")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("state"), QJsonObject::fromVariantMap(normalizedWorkspaceState(state))}
    };

    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return file.write(payload) == payload.size() && file.commit();
}

bool AppSettings::clearWorkspaceStateFile() const {
    const QString path = workspaceStatePath();
    return !QFileInfo::exists(path) || QFile::remove(path);
}

void AppSettings::migrateLegacyWorkspaceState() {
    const QString legacyKey = QStringLiteral("workspace/viewState");
    if (!m_settings.contains(legacyKey))
        return;

    QVariantMap existingState;
    const bool alreadyMigrated = readWorkspaceStateFile(&existingState);
    const QVariantMap legacyState = m_settings.value(legacyKey).toMap();
    if (!alreadyMigrated && !writeWorkspaceStateFile(legacyState))
        return;

    m_settings.remove(legacyKey);
    m_settings.sync();
}

QVariantMap AppSettings::viewState() const {
    QVariantMap state;
    return readWorkspaceStateFile(&state) ? state : QVariantMap{};
}

void AppSettings::saveViewState(const QVariantMap &state) {
    if (!writeWorkspaceStateFile(state))
        return;

    // Clean up the old key even if an older build or imported settings managed
    // to recreate it during this session.
    if (m_settings.contains(QStringLiteral("workspace/viewState"))) {
        m_settings.remove(QStringLiteral("workspace/viewState"));
        m_settings.sync();
    }
}
