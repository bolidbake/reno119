#include "ThemeManager.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

namespace {
QString normalizedColor(const QJsonValue &value) {
    if (!value.isString())
        return {};
    const QColor color(value.toString().trimmed());
    if (!color.isValid())
        return {};
    return color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb);
}

QString simplifiedRoleKey(QString key) {
    key = key.trimmed();
    if (key.startsWith(QLatin1Char('m')) && key.size() > 1 && key.at(1).isUpper())
        key.remove(0, 1); // Noctalia legacy mPrimary-style names.

    QString out;
    out.reserve(key.size() + 8);
    for (qsizetype i = 0; i < key.size(); ++i) {
        const QChar ch = key.at(i);
        if (ch == QLatin1Char('-') || ch == QLatin1Char(' ') || ch == QLatin1Char('.')) {
            if (!out.endsWith(QLatin1Char('_')))
                out += QLatin1Char('_');
            continue;
        }
        if (ch.isUpper() && i > 0 && !out.endsWith(QLatin1Char('_')))
            out += QLatin1Char('_');
        out += ch.toLower();
    }
    return out;
}
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent),
      m_configRoot(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/reno119")),
      m_materialThemePath(m_configRoot + QStringLiteral("/material-theme.json")),
      m_noctaliaThemePath(m_configRoot + QStringLiteral("/noctalia-theme.json")),
      m_materialPalette(defaultMaterialPalette()),
      m_noctaliaPalette(defaultMaterialPalette()) {
    QDir().mkpath(m_configRoot);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (path == m_materialThemePath)
            reloadMaterial();
        else if (path == m_noctaliaThemePath)
            reloadNoctalia();
        refreshWatchPaths();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        reloadAll();
    });

    reloadAll();
}

QVariantMap ThemeManager::materialPalette() const { return m_materialPalette; }
bool ThemeManager::materialDark() const { return m_materialDark; }
bool ThemeManager::materialPureBlack() const { return paletteIsPureBlack(m_materialPalette); }
bool ThemeManager::materialCustomReady() const { return m_materialCustomReady; }
QString ThemeManager::materialStatus() const { return m_materialStatus; }
QString ThemeManager::materialThemePath() const { return m_materialThemePath; }
QVariantMap ThemeManager::noctaliaPalette() const { return m_noctaliaPalette; }
bool ThemeManager::noctaliaDark() const { return m_noctaliaDark; }
bool ThemeManager::noctaliaPureBlack() const { return paletteIsPureBlack(m_noctaliaPalette); }
bool ThemeManager::noctaliaReady() const { return m_noctaliaReady; }
QString ThemeManager::noctaliaStatus() const { return m_noctaliaStatus; }
QString ThemeManager::noctaliaThemePath() const { return m_noctaliaThemePath; }

QVariantMap ThemeManager::defaultMaterialPalette(bool dark) {
    // Material Design 3 baseline palettes. External files may override any
    // subset of these roles using the same canonical role names.
    if (!dark) {
        return {
            {QStringLiteral("primary"), QStringLiteral("#6750a4")},
            {QStringLiteral("on_primary"), QStringLiteral("#ffffff")},
            {QStringLiteral("primary_container"), QStringLiteral("#eaddff")},
            {QStringLiteral("on_primary_container"), QStringLiteral("#21005d")},
            {QStringLiteral("secondary"), QStringLiteral("#625b71")},
            {QStringLiteral("on_secondary"), QStringLiteral("#ffffff")},
            {QStringLiteral("secondary_container"), QStringLiteral("#e8def8")},
            {QStringLiteral("on_secondary_container"), QStringLiteral("#1d192b")},
            {QStringLiteral("tertiary"), QStringLiteral("#7d5260")},
            {QStringLiteral("on_tertiary"), QStringLiteral("#ffffff")},
            {QStringLiteral("tertiary_container"), QStringLiteral("#ffd8e4")},
            {QStringLiteral("on_tertiary_container"), QStringLiteral("#31111d")},
            {QStringLiteral("error"), QStringLiteral("#b3261e")},
            {QStringLiteral("on_error"), QStringLiteral("#ffffff")},
            {QStringLiteral("error_container"), QStringLiteral("#f9dedc")},
            {QStringLiteral("on_error_container"), QStringLiteral("#410e0b")},
            {QStringLiteral("surface"), QStringLiteral("#fffbfe")},
            {QStringLiteral("on_surface"), QStringLiteral("#1c1b1f")},
            {QStringLiteral("surface_variant"), QStringLiteral("#e7e0ec")},
            {QStringLiteral("on_surface_variant"), QStringLiteral("#49454f")},
            {QStringLiteral("surface_container_lowest"), QStringLiteral("#ffffff")},
            {QStringLiteral("surface_container_low"), QStringLiteral("#f7f2fa")},
            {QStringLiteral("surface_container"), QStringLiteral("#f3edf7")},
            {QStringLiteral("surface_container_high"), QStringLiteral("#ece6f0")},
            {QStringLiteral("surface_container_highest"), QStringLiteral("#e6e0e9")},
            {QStringLiteral("outline"), QStringLiteral("#79747e")},
            {QStringLiteral("outline_variant"), QStringLiteral("#cac4d0")},
            {QStringLiteral("inverse_surface"), QStringLiteral("#313033")},
            {QStringLiteral("inverse_on_surface"), QStringLiteral("#f4eff4")},
            {QStringLiteral("inverse_primary"), QStringLiteral("#d0bcff")},
            {QStringLiteral("background"), QStringLiteral("#fffbfe")},
            {QStringLiteral("on_background"), QStringLiteral("#1c1b1f")},
            {QStringLiteral("shadow"), QStringLiteral("#000000")},
            {QStringLiteral("scrim"), QStringLiteral("#000000")}
        };
    }

    return {
        {QStringLiteral("primary"), QStringLiteral("#d0bcff")},
        {QStringLiteral("on_primary"), QStringLiteral("#381e72")},
        {QStringLiteral("primary_container"), QStringLiteral("#4f378b")},
        {QStringLiteral("on_primary_container"), QStringLiteral("#eaddff")},
        {QStringLiteral("secondary"), QStringLiteral("#ccc2dc")},
        {QStringLiteral("on_secondary"), QStringLiteral("#332d41")},
        {QStringLiteral("secondary_container"), QStringLiteral("#4a4458")},
        {QStringLiteral("on_secondary_container"), QStringLiteral("#e8def8")},
        {QStringLiteral("tertiary"), QStringLiteral("#efb8c8")},
        {QStringLiteral("on_tertiary"), QStringLiteral("#492532")},
        {QStringLiteral("tertiary_container"), QStringLiteral("#633b48")},
        {QStringLiteral("on_tertiary_container"), QStringLiteral("#ffd8e4")},
        {QStringLiteral("error"), QStringLiteral("#f2b8b5")},
        {QStringLiteral("on_error"), QStringLiteral("#601410")},
        {QStringLiteral("error_container"), QStringLiteral("#8c1d18")},
        {QStringLiteral("on_error_container"), QStringLiteral("#f9dedc")},
        {QStringLiteral("surface"), QStringLiteral("#1c1b1f")},
        {QStringLiteral("on_surface"), QStringLiteral("#e6e1e5")},
        {QStringLiteral("surface_variant"), QStringLiteral("#49454f")},
        {QStringLiteral("on_surface_variant"), QStringLiteral("#cac4d0")},
        {QStringLiteral("surface_container_lowest"), QStringLiteral("#0f0d13")},
        {QStringLiteral("surface_container_low"), QStringLiteral("#1d1b20")},
        {QStringLiteral("surface_container"), QStringLiteral("#211f26")},
        {QStringLiteral("surface_container_high"), QStringLiteral("#2b2930")},
        {QStringLiteral("surface_container_highest"), QStringLiteral("#36343b")},
        {QStringLiteral("outline"), QStringLiteral("#938f99")},
        {QStringLiteral("outline_variant"), QStringLiteral("#49454f")},
        {QStringLiteral("inverse_surface"), QStringLiteral("#e6e1e5")},
        {QStringLiteral("inverse_on_surface"), QStringLiteral("#313033")},
        {QStringLiteral("inverse_primary"), QStringLiteral("#6750a4")},
        {QStringLiteral("background"), QStringLiteral("#1c1b1f")},
        {QStringLiteral("on_background"), QStringLiteral("#e6e1e5")},
        {QStringLiteral("shadow"), QStringLiteral("#000000")},
        {QStringLiteral("scrim"), QStringLiteral("#000000")}
    };
}

QString ThemeManager::canonicalRole(const QString &key) {
    const QString role = simplifiedRoleKey(key);
    if (role == QStringLiteral("hover"))
        return QStringLiteral("surface_container_high");
    if (role == QStringLiteral("on_hover"))
        return QStringLiteral("on_surface");
    return role;
}

ThemeManager::ParsedTheme ThemeManager::readThemeFile(const QString &path) {
    ParsedTheme result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Could not open %1").arg(path);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("Invalid theme JSON: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = document.object();
    QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    if (colors.isEmpty())
        colors = root.value(QStringLiteral("palette")).toObject();
    if (colors.isEmpty())
        colors = root;

    QVariantMap supplied;
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        const QString canonical = canonicalRole(it.key());
        if (canonical == QStringLiteral("mode") || canonical == QStringLiteral("name") || canonical == QStringLiteral("format"))
            continue;
        const QString color = normalizedColor(it.value());
        if (color.isEmpty())
            continue;
        supplied.insert(canonical, color);
        ++result.suppliedRoles;
    }

    if (result.suppliedRoles == 0) {
        result.error = QStringLiteral("Theme JSON does not contain any valid color roles.");
        return result;
    }

    result.name = root.value(QStringLiteral("name")).toString();
    const QString mode = root.value(QStringLiteral("mode")).toString().trimmed().toLower();
    result.dark = mode == QStringLiteral("dark") ? true
                : mode == QStringLiteral("light") ? false
                : paletteLooksDark(supplied);
    result.palette = defaultMaterialPalette(result.dark);
    for (auto it = supplied.constBegin(); it != supplied.constEnd(); ++it)
        result.palette.insert(it.key(), it.value());
    result.ok = true;
    return result;
}

bool ThemeManager::paletteLooksDark(const QVariantMap &palette) {
    const QColor surface(palette.value(QStringLiteral("surface"), QStringLiteral("#1c1b1f")).toString());
    if (!surface.isValid())
        return true;
    const qreal luminance = surface.redF() * 0.299 + surface.greenF() * 0.587 + surface.blueF() * 0.114;
    return luminance < 0.5;
}

bool ThemeManager::paletteIsPureBlack(const QVariantMap &palette) {
    const QColor surface(palette.value(QStringLiteral("surface")).toString());
    const QColor background(palette.value(QStringLiteral("background")).toString());
    if (!surface.isValid() || !background.isValid())
        return false;
    return surface.alpha() == 255 && background.alpha() == 255
        && surface.red() == 0 && surface.green() == 0 && surface.blue() == 0
        && background.red() == 0 && background.green() == 0 && background.blue() == 0;
}

void ThemeManager::refreshWatchPaths() {
    const QStringList watchedFiles = m_watcher.files();
    const QStringList watchedDirs = m_watcher.directories();
    if (!watchedDirs.contains(m_configRoot) && QFileInfo::exists(m_configRoot))
        m_watcher.addPath(m_configRoot);
    if (QFileInfo::exists(m_materialThemePath) && !watchedFiles.contains(m_materialThemePath))
        m_watcher.addPath(m_materialThemePath);
    if (QFileInfo::exists(m_noctaliaThemePath) && !watchedFiles.contains(m_noctaliaThemePath))
        m_watcher.addPath(m_noctaliaThemePath);
}

void ThemeManager::reloadMaterial() {
    if (!QFileInfo::exists(m_materialThemePath)) {
        m_materialPalette = defaultMaterialPalette(true);
        m_materialDark = true;
        m_materialCustomReady = false;
        m_materialStatus = QStringLiteral("Using Reno119's built-in Material 3 dark palette.");
        refreshWatchPaths();
        emit materialThemeChanged();
        return;
    }

    const ParsedTheme parsed = readThemeFile(m_materialThemePath);
    if (!parsed.ok) {
        m_materialPalette = defaultMaterialPalette(true);
        m_materialDark = true;
        m_materialCustomReady = false;
        m_materialStatus = QStringLiteral("Custom Material palette ignored: %1").arg(parsed.error);
    } else {
        m_materialPalette = parsed.palette;
        m_materialDark = parsed.dark;
        m_materialCustomReady = true;
        m_materialStatus = parsed.name.trimmed().isEmpty()
            ? QStringLiteral("Loaded custom Material palette (%1 roles).").arg(parsed.suppliedRoles)
            : QStringLiteral("Loaded %1 (%2 roles).").arg(parsed.name.trimmed()).arg(parsed.suppliedRoles);
    }
    refreshWatchPaths();
    emit materialThemeChanged();
}

void ThemeManager::reloadNoctalia() {
    if (!QFileInfo::exists(m_noctaliaThemePath)) {
        m_noctaliaPalette = defaultMaterialPalette(true);
        m_noctaliaDark = true;
        m_noctaliaReady = false;
        m_noctaliaStatus = QStringLiteral("No Noctalia-generated palette found yet.");
        refreshWatchPaths();
        emit noctaliaThemeChanged();
        return;
    }

    const ParsedTheme parsed = readThemeFile(m_noctaliaThemePath);
    if (!parsed.ok) {
        m_noctaliaPalette = defaultMaterialPalette(true);
        m_noctaliaDark = true;
        m_noctaliaReady = false;
        m_noctaliaStatus = QStringLiteral("Noctalia palette ignored: %1").arg(parsed.error);
    } else {
        m_noctaliaPalette = parsed.palette;
        m_noctaliaDark = parsed.dark;
        m_noctaliaReady = true;
        const QString source = parsed.name.trimmed().isEmpty()
            ? QStringLiteral("the generated Noctalia Material palette")
            : parsed.name.trimmed();
        m_noctaliaStatus = paletteIsPureBlack(m_noctaliaPalette)
            ? QStringLiteral("Following %1 · Pure Black surfaces detected.").arg(source)
            : QStringLiteral("Following %1.").arg(source);
    }
    refreshWatchPaths();
    emit noctaliaThemeChanged();
}

void ThemeManager::reloadAll() {
    reloadMaterial();
    reloadNoctalia();
    refreshWatchPaths();
}
