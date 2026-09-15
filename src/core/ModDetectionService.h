#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

class ModDetectionService final {
public:
    static QStringList reShadeProxyNames();
    static QStringList optiScalerProxyNames();

    static bool looksLikeReShadeBinary(const QString &path);
    static bool looksLikeReFrameworkBinary(const QString &path);
    static bool looksLikeOptiScalerBinary(const QString &path);

    static QString firstReShadeProxy(const QString &exeDir);
    static QString detectOptiScalerProxy(const QString &exeDir,
                                         const QString &reshadePath,
                                         const QString &expectedProxy,
                                         bool hasOptiScalerIni);

private:
    static QByteArray readBytes(const QString &path, qint64 maxBytes = -1);
};
