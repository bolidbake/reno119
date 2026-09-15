#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantMap>

class ThemeManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap materialPalette READ materialPalette NOTIFY materialThemeChanged)
    Q_PROPERTY(bool materialDark READ materialDark NOTIFY materialThemeChanged)
    Q_PROPERTY(bool materialPureBlack READ materialPureBlack NOTIFY materialThemeChanged)
    Q_PROPERTY(bool materialCustomReady READ materialCustomReady NOTIFY materialThemeChanged)
    Q_PROPERTY(QString materialStatus READ materialStatus NOTIFY materialThemeChanged)
    Q_PROPERTY(QString materialThemePath READ materialThemePath CONSTANT)
    Q_PROPERTY(QVariantMap noctaliaPalette READ noctaliaPalette NOTIFY noctaliaThemeChanged)
    Q_PROPERTY(bool noctaliaDark READ noctaliaDark NOTIFY noctaliaThemeChanged)
    Q_PROPERTY(bool noctaliaPureBlack READ noctaliaPureBlack NOTIFY noctaliaThemeChanged)
    Q_PROPERTY(bool noctaliaReady READ noctaliaReady NOTIFY noctaliaThemeChanged)
    Q_PROPERTY(QString noctaliaStatus READ noctaliaStatus NOTIFY noctaliaThemeChanged)
    Q_PROPERTY(QString noctaliaThemePath READ noctaliaThemePath CONSTANT)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QVariantMap materialPalette() const;
    bool materialDark() const;
    bool materialPureBlack() const;
    bool materialCustomReady() const;
    QString materialStatus() const;
    QString materialThemePath() const;

    QVariantMap noctaliaPalette() const;
    bool noctaliaDark() const;
    bool noctaliaPureBlack() const;
    bool noctaliaReady() const;
    QString noctaliaStatus() const;
    QString noctaliaThemePath() const;

    Q_INVOKABLE void reloadMaterial();
    Q_INVOKABLE void reloadNoctalia();
    Q_INVOKABLE void reloadAll();

signals:
    void materialThemeChanged();
    void noctaliaThemeChanged();

private:
    struct ParsedTheme {
        QVariantMap palette;
        bool dark = true;
        QString name;
        QString error;
        int suppliedRoles = 0;
        bool ok = false;
    };

    static QVariantMap defaultMaterialPalette(bool dark = true);
    static ParsedTheme readThemeFile(const QString &path);
    static bool paletteLooksDark(const QVariantMap &palette);
    static bool paletteIsPureBlack(const QVariantMap &palette);
    static QString canonicalRole(const QString &key);
    void refreshWatchPaths();

    QFileSystemWatcher m_watcher;
    QString m_configRoot;
    QString m_materialThemePath;
    QString m_noctaliaThemePath;

    QVariantMap m_materialPalette;
    bool m_materialDark = true;
    bool m_materialCustomReady = false;
    QString m_materialStatus;

    QVariantMap m_noctaliaPalette;
    bool m_noctaliaDark = true;
    bool m_noctaliaReady = false;
    QString m_noctaliaStatus;
};
