#pragma once

#include "../core/GameInfo.h"
#include <QVector>
#include <QStringList>

class SteamScanner {
public:
    static QVector<GameInfo> scan();
    static QStringList knownLibraries();
    static QString findProtonPrefix(const QString &appId, const QStringList &libraries,
                                    const QString &preferredLibrary = QString(),
                                    QStringList *candidatePaths = nullptr,
                                    QString *resolvedLibrary = nullptr);
    static QString titleInitialism(const QString &title);
    static QString detectEngine(const QString &gameDir, const QString &exePath);
    static bool supportsReFramework(const QString &gameName, const QString &engine);
    static QString findBestExecutable(const QString &gameDir, QString &api, QString &arch);
    static QString findBestExecutable(const QString &gameDir, const QString &preferredName,
                                      QString &api, QString &arch, QStringList *candidateSummary = nullptr);
    static QString findBestUbisoftExecutable(const QString &prefix, const QString &preferredName,
                                             QString &api, QString &arch, QString &installPath,
                                             QStringList *candidateSummary = nullptr);
    static bool isLikelyLauncherExecutable(const QString &path);

private:
    static QStringList steamRoots();
    static QStringList librariesForRoot(const QString &root);
    static QString vdfValue(const QString &text, const QString &key);
    static bool isSteamTool(const QString &name, const QString &installDir);
    static void collectExecutables(const QString &dir, int depth, QStringList &out);
};
