#include "PeParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QSet>
#include <QtEndian>
#include <algorithm>

namespace {
quint16 u16(const QByteArray &b, qsizetype off) {
    if (off < 0 || off + 2 > b.size()) return 0;
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData() + off));
}
quint32 u32(const QByteArray &b, qsizetype off) {
    if (off < 0 || off + 4 > b.size()) return 0;
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + off));
}

qint64 rvaToOffset(const QVector<PeParser::Section> &sections, quint32 rva) {
    for (const auto &s : sections) {
        const quint32 span = std::max(s.virtualSize, s.rawSize);
        if (rva >= s.virtualAddress && rva < s.virtualAddress + span)
            return static_cast<qint64>(s.rawAddress) + (rva - s.virtualAddress);
    }
    return -1;
}

QString readCStringAt(QFile &f, qint64 off, int maxLen = 260) {
    if (off < 0 || !f.seek(off)) return {};
    QByteArray out;
    out.reserve(maxLen);
    for (int i = 0; i < maxLen; ++i) {
        char c = 0;
        if (f.read(&c, 1) != 1 || c == '\0') break;
        out.append(c);
    }
    return QString::fromLatin1(out).trimmed().toLower();
}

void scanImports(QFile &f, const QVector<PeParser::Section> &sections,
                 quint32 tableRva, int descriptorSize, int nameOffset,
                 QStringList &imports) {
    const qint64 tableOff = rvaToOffset(sections, tableRva);
    if (tableOff < 0) return;

    for (int i = 0; i < 2048; ++i) {
        const qint64 descOff = tableOff + static_cast<qint64>(i) * descriptorSize;
        if (!f.seek(descOff)) break;
        const QByteArray desc = f.read(descriptorSize);
        if (desc.size() != descriptorSize) break;
        const quint32 nameRva = u32(desc, nameOffset);
        if (nameRva == 0) break;
        const QString name = readCStringAt(f, rvaToOffset(sections, nameRva));
        if (!name.isEmpty() && !imports.contains(name, Qt::CaseInsensitive))
            imports.append(name);
    }
}
}

PeParser::Result PeParser::inspect(const QString &path) {
    Result result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return result;

    QByteArray dos = f.read(64);
    if (dos.size() < 64 || dos[0] != 'M' || dos[1] != 'Z') return result;
    const quint32 peOff = u32(dos, 0x3c);

    if (!f.seek(peOff)) return result;
    QByteArray pe = f.read(24);
    if (pe.size() != 24 || pe.mid(0, 4) != QByteArray("PE\0\0", 4)) return result;

    const quint16 machine = u16(pe, 4);
    const quint16 sectionCount = u16(pe, 6);
    const quint16 optionalSize = u16(pe, 20);
    if (sectionCount == 0 || sectionCount > 96 || optionalSize < 96) return result;

    switch (machine) {
    case 0x8664: result.architecture = "x64"; break;
    case 0x014c: result.architecture = "x86"; break;
    case 0xAA64: result.architecture = "ARM64"; break;
    default: result.architecture = QString("Machine 0x%1").arg(machine, 4, 16, QLatin1Char('0')); break;
    }

    const qint64 optionalOff = peOff + 24;
    if (!f.seek(optionalOff)) return result;
    const QByteArray optional = f.read(optionalSize);
    if (optional.size() != optionalSize) return result;
    const quint16 magic = u16(optional, 0);
    const int dataDirOff = (magic == 0x10b) ? 96 : (magic == 0x20b ? 112 : -1);
    if (dataDirOff < 0 || dataDirOff + (14 * 8) > optional.size()) return result;

    const quint32 importRva = u32(optional, dataDirOff + 8);          // IMAGE_DIRECTORY_ENTRY_IMPORT
    const quint32 delayImportRva = u32(optional, dataDirOff + 13*8); // IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT

    if (!f.seek(optionalOff + optionalSize)) return result;
    QVector<Section> sections;
    sections.reserve(sectionCount);
    for (quint16 i = 0; i < sectionCount; ++i) {
        const QByteArray sh = f.read(40);
        if (sh.size() != 40) return result;
        Section s;
        s.virtualSize = u32(sh, 8);
        s.virtualAddress = u32(sh, 12);
        s.rawSize = u32(sh, 16);
        s.rawAddress = u32(sh, 20);
        sections.push_back(s);
    }

    if (importRva)
        scanImports(f, sections, importRva, 20, 12, result.imports);

    auto has = [&](const char *dll) { return result.imports.contains(QString::fromLatin1(dll), Qt::CaseInsensitive); };

    // Detection priority: explicit D3D imports win;
    // DXGI-only modern binaries are inferred as DX12.
    if (has("d3d12.dll")) result.graphicsApi = "DirectX 12";
    else if (has("d3d11.dll")) result.graphicsApi = "DirectX 11";
    else if (has("d3d10.dll") || has("d3d10_1.dll")) result.graphicsApi = "DirectX 10";
    else if (has("d3d9.dll")) result.graphicsApi = "DirectX 9";
    else if (has("d3d8.dll")) result.graphicsApi = "DirectX 8";
    else if (has("vulkan-1.dll")) result.graphicsApi = "Vulkan";
    else if (has("opengl32.dll")) result.graphicsApi = "OpenGL";
    else if (has("dxgi.dll")) result.graphicsApi = "DirectX 12";

    // Delay imports only promote legacy/unknown results to DX11+.
    const bool needsDelay = result.graphicsApi == "Unknown" || result.graphicsApi == "DirectX 8" ||
                            result.graphicsApi == "DirectX 9" || result.graphicsApi == "DirectX 10" ||
                            result.graphicsApi == "OpenGL";
    if (needsDelay && delayImportRva) {
        QStringList delayed;
        scanImports(f, sections, delayImportRva, 32, 4, delayed);
        for (const auto &dll : delayed)
            if (!result.imports.contains(dll, Qt::CaseInsensitive)) result.imports.append(dll);
        if (delayed.contains("d3d12.dll", Qt::CaseInsensitive)) result.graphicsApi = "DirectX 12";
        else if (delayed.contains("d3d11.dll", Qt::CaseInsensitive)) result.graphicsApi = "DirectX 11";
    }

    result.valid = true;
    return result;
}


QString PeParser::detectGraphicsApiFallback(const QString &path, QStringList *evidence) {
    if (evidence)
        evidence->clear();

    auto apiFromName = [](const QString &name) -> QString {
        const QString key = name.toLower();
        if (key.contains(QStringLiteral("d3d12")) || key.contains(QStringLiteral("dx12"))) return QStringLiteral("DirectX 12");
        if (key.contains(QStringLiteral("d3d11")) || key.contains(QStringLiteral("dx11"))) return QStringLiteral("DirectX 11");
        if (key.contains(QStringLiteral("d3d10")) || key.contains(QStringLiteral("dx10"))) return QStringLiteral("DirectX 10");
        if (key.contains(QStringLiteral("d3d9")) || key.contains(QStringLiteral("dx9"))) return QStringLiteral("DirectX 9");
        if (key.contains(QStringLiteral("vulkan"))) return QStringLiteral("Vulkan");
        if (key.contains(QStringLiteral("opengl"))) return QStringLiteral("OpenGL");
        return {};
    };

    auto addScore = [&](QHash<QString, int> &scores, const QString &api, int amount, const QString &why) {
        if (api.isEmpty() || api == QStringLiteral("Unknown"))
            return;
        scores[api] += amount;
        if (evidence && evidence->size() < 12)
            evidence->append(QStringLiteral("%1 (+%2): %3").arg(api).arg(amount).arg(why));
    };

    auto clearWinner = [](const QHash<QString, int> &scores, int minimumScore) -> QString {
        QString bestApi;
        int bestScore = 0;
        int secondScore = 0;
        for (auto it = scores.cbegin(); it != scores.cend(); ++it) {
            if (it.value() > bestScore) {
                secondScore = bestScore;
                bestScore = it.value();
                bestApi = it.key();
            } else if (it.value() > secondScore) {
                secondScore = it.value();
            }
        }
        return (bestScore >= minimumScore && bestScore >= secondScore + 2) ? bestApi : QString();
    };

    QHash<QString, int> scores;

    // Some engines resolve graphics runtimes with LoadLibrary/GetProcAddress,
    // so the DLL never appears in the import table. Keep this fallback cheap:
    // scan at most a bounded head/tail sample instead of reading a multi-GB EXE
    // in full during every library refresh.
    struct RuntimeProbe { QString api; QList<QByteArray> needles; QList<QByteArray> wideNeedles; };
    QList<RuntimeProbe> probes = {
        {QStringLiteral("DirectX 12"), {"d3d12.dll", "d3d12createdevice"}, {}},
        {QStringLiteral("DirectX 11"), {"d3d11.dll", "d3d11createdevice"}, {}},
        {QStringLiteral("DirectX 10"), {"d3d10.dll", "d3d10createdevice"}, {}},
        {QStringLiteral("DirectX 9"),  {"d3d9.dll", "direct3dcreate9"}, {}},
        {QStringLiteral("Vulkan"),     {"vulkan-1.dll", "vkcreateinstance"}, {}},
        {QStringLiteral("OpenGL"),     {"opengl32.dll", "wglcreatecontext"}, {}}
    };
    for (auto &probe : probes) {
        for (const QByteArray &needle : probe.needles) {
            QByteArray wide;
            wide.reserve(needle.size() * 2);
            for (char c : needle) {
                wide.append(c);
                wide.append('\0');
            }
            probe.wideNeedles.append(wide);
        }
    }

    auto scanRuntimeBuffer = [&](const QByteArray &data) {
        const QByteArray lower = data.toLower();
        for (const auto &probe : probes) {
            bool matched = false;
            for (int i = 0; i < probe.needles.size(); ++i) {
                if (lower.contains(probe.needles.at(i)) || lower.contains(probe.wideNeedles.at(i))) {
                    matched = true;
                    break;
                }
            }
            if (matched)
                addScore(scores, probe.api, 6, QStringLiteral("runtime reference in %1").arg(QFileInfo(path).fileName()));
        }
    };

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        constexpr qint64 fullReadLimit = 8ll * 1024 * 1024;
        constexpr qint64 headReadLimit = 4ll * 1024 * 1024;
        constexpr qint64 tailReadLimit = 1ll * 1024 * 1024;
        const qint64 size = file.size();
        if (size <= fullReadLimit) {
            scanRuntimeBuffer(file.readAll());
        } else {
            scanRuntimeBuffer(file.read(headReadLimit));
            if (file.seek(qMax<qint64>(0, size - tailReadLimit)))
                scanRuntimeBuffer(file.read(tailReadLimit));
        }
    }

    // Inspect adjacent module *names* first. This is enough for common layouts
    // such as Watch Dogs 2's GFSDK_*_D3D11 modules and avoids opening dozens of
    // DLLs merely to discover an API that their filenames already identify.
    QDir dir(QFileInfo(path).absolutePath());
    QFileInfoList dlls = dir.entryInfoList(QStringList{QStringLiteral("*.dll")}, QDir::Files | QDir::Readable);

    const QSet<QString> proxyNames = {
        QStringLiteral("dxgi.dll"), QStringLiteral("d3d8.dll"), QStringLiteral("d3d9.dll"),
        QStringLiteral("d3d10.dll"), QStringLiteral("d3d10_1.dll"), QStringLiteral("d3d11.dll"),
        QStringLiteral("d3d12.dll"), QStringLiteral("opengl32.dll"), QStringLiteral("vulkan-1.dll")
    };

    for (const QFileInfo &dll : dlls) {
        if (proxyNames.contains(dll.fileName().toLower()))
            continue;
        const QString nameApi = apiFromName(dll.completeBaseName());
        if (!nameApi.isEmpty())
            addScore(scores, nameApi, 6, QStringLiteral("renderer module name %1").arg(dll.fileName()));
    }

    if (const QString winner = clearWinner(scores, 6); !winner.isEmpty())
        return winner;

    // If names/runtime strings are still inconclusive, PE-inspect only a small
    // shortlist of likely engine/renderer modules. PeParser::inspect itself is
    // header/import based, but opening 64 arbitrary DLLs per game made startup
    // scale poorly on large libraries.
    struct ModuleCandidate { QFileInfo info; int priority = 0; };
    QVector<ModuleCandidate> modules;
    modules.reserve(dlls.size());
    for (const QFileInfo &dll : dlls) {
        const QString lowerName = dll.fileName().toLower();
        if (proxyNames.contains(lowerName))
            continue;

        int priority = 0;
        if (lowerName.contains(QStringLiteral("render")) || lowerName.contains(QStringLiteral("renderer"))) priority += 80;
        if (lowerName.contains(QStringLiteral("engine")) || lowerName.contains(QStringLiteral("rhi"))) priority += 60;
        if (lowerName.contains(QStringLiteral("graphics")) || lowerName.contains(QStringLiteral("gpu"))) priority += 50;
        if (lowerName.contains(QStringLiteral("disrupt"))) priority += 45;
        if (!apiFromName(lowerName).isEmpty()) priority += 100;
        if (dll.size() >= 64ll * 1024 * 1024) priority += 30;
        else if (dll.size() >= 16ll * 1024 * 1024) priority += 20;
        else if (dll.size() >= 4ll * 1024 * 1024) priority += 8;

        if (priority > 0)
            modules.push_back({dll, priority});
    }
    std::sort(modules.begin(), modules.end(), [](const ModuleCandidate &a, const ModuleCandidate &b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.info.size() > b.info.size();
    });

    const int inspectLimit = qMin(8, static_cast<int>(modules.size()));
    for (int i = 0; i < inspectLimit; ++i) {
        const QFileInfo &dll = modules.at(i).info;
        const Result module = inspect(dll.absoluteFilePath());
        if (!module.valid || module.graphicsApi == QStringLiteral("Unknown"))
            continue;
        const int weight = dll.size() >= 16ll * 1024 * 1024 ? 3 : 1;
        addScore(scores, module.graphicsApi, weight, QStringLiteral("imports from %1").arg(dll.fileName()));
    }

    if (const QString winner = clearWinner(scores, 4); !winner.isEmpty())
        return winner;
    return QStringLiteral("Unknown");
}
