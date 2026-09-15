#include "ModDetectionService.h"

#include <QFile>
#include <QFileInfo>

QByteArray ModDetectionService::readBytes(const QString &path, qint64 maxBytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return maxBytes < 0 ? file.readAll() : file.read(maxBytes);
}

QStringList ModDetectionService::reShadeProxyNames() {
    return {
        QStringLiteral("dxgi.dll"),
        QStringLiteral("d3d9.dll"),
        QStringLiteral("d3d10.dll"),
        QStringLiteral("d3d11.dll"),
        QStringLiteral("d3d12.dll"),
        QStringLiteral("opengl32.dll")
    };
}

QStringList ModDetectionService::optiScalerProxyNames() {
    return {
        QStringLiteral("dxgi.dll"),
        QStringLiteral("winmm.dll"),
        QStringLiteral("d3d12.dll"),
        QStringLiteral("dbghelp.dll"),
        QStringLiteral("version.dll"),
        QStringLiteral("wininet.dll"),
        QStringLiteral("winhttp.dll"),
        QStringLiteral("OptiScaler.asi")
    };
}

bool ModDetectionService::looksLikeReShadeBinary(const QString &path) {
    const QByteArray bytes = readBytes(path, 8 * 1024 * 1024);
    return bytes.contains("ReShade") || bytes.contains("reshade.me");
}

bool ModDetectionService::looksLikeReFrameworkBinary(const QString &path) {
    const QByteArray bytes = readBytes(path, 12 * 1024 * 1024).toLower();
    return bytes.contains("reframework") || bytes.contains("re2_framework") || bytes.contains("praydog");
}

bool ModDetectionService::looksLikeOptiScalerBinary(const QString &path) {
    const QByteArray bytes = readBytes(path);
    if (bytes.isEmpty())
        return false;

    // Generic proxy and ASI loaders often reuse the same proxy filenames as
    // OptiScaler. A single incidental "OptiScaler" string is therefore not
    // sufficient evidence. Keep the v0.13.3 false-positive fix as a central
    // invariant: require the product name plus at least one independent marker.
    const bool hasName = bytes.contains("OptiScaler");
    const bool hasIni = bytes.contains("OptiScaler.ini");
    const bool hasOptiFg = bytes.contains("OptiFG");
    const bool hasOptiDllPath = bytes.contains("OptiDllPath");
    return hasName && (hasIni || hasOptiFg || hasOptiDllPath);
}

QString ModDetectionService::firstReShadeProxy(const QString &exeDir) {
    for (const QString &name : reShadeProxyNames()) {
        const QString path = exeDir + QLatin1Char('/') + name;
        if (QFileInfo::exists(path) && looksLikeReShadeBinary(path))
            return name;
    }
    return {};
}

QString ModDetectionService::detectOptiScalerProxy(const QString &exeDir,
                                                    const QString &reshadePath,
                                                    const QString &expectedProxy,
                                                    bool hasOptiScalerIni) {
    if (!expectedProxy.isEmpty() && QFileInfo::exists(exeDir + QLatin1Char('/') + expectedProxy))
        return expectedProxy;

    QStringList existingCandidates;
    for (const QString &proxy : optiScalerProxyNames()) {
        if (proxy.compare(reshadePath, Qt::CaseInsensitive) == 0)
            continue;
        const QString path = exeDir + QLatin1Char('/') + proxy;
        if (!QFileInfo::exists(path))
            continue;
        existingCandidates << proxy;
        if (looksLikeOptiScalerBinary(path))
            return proxy;
    }

    const QString canonical = exeDir + QStringLiteral("/OptiScaler.dll");
    if (QFileInfo::exists(canonical))
        return QStringLiteral("OptiScaler.dll");

    // Preserve the existing conservative fallback: only infer ownership by
    // filename when OptiScaler.ini is present and exactly one plausible proxy
    // remains after excluding the active ReShade path.
    if (hasOptiScalerIni && existingCandidates.size() == 1)
        return existingCandidates.first();

    return {};
}
