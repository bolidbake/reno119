#include "SteamScanner.h"
#include "../graphics/PeParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <utility>

QString SteamScanner::vdfValue(const QString &text, const QString &key) {
    const QRegularExpression rx(QStringLiteral("\\\"%1\\\"\\s+\\\"([^\\\"]*)\\\"").arg(QRegularExpression::escape(key)),
                                QRegularExpression::CaseInsensitiveOption);
    const auto m = rx.match(text);
    return m.hasMatch() ? m.captured(1) : QString();
}

QStringList SteamScanner::steamRoots() {
    const QString home = QDir::homePath();
    QStringList roots = {
        home + "/.local/share/Steam",
        home + "/.steam/steam",
        home + "/.var/app/com.valvesoftware.Steam/data/Steam"
    };
    roots.removeDuplicates();
    QStringList existing;
    for (const auto &r : roots)
        if (QDir(r + "/steamapps").exists()) existing << QDir(r).canonicalPath();
    existing.removeDuplicates();
    return existing;
}

QStringList SteamScanner::librariesForRoot(const QString &root) {
    QStringList libs{QDir(root).canonicalPath()};
    QFile f(root + "/steamapps/libraryfolders.vdf");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return libs;
    const QString text = QString::fromUtf8(f.readAll());
    const QRegularExpression rx(QStringLiteral("\\\"path\\\"\\s+\\\"([^\\\"]+)\\\""),
                                QRegularExpression::CaseInsensitiveOption);
    auto it = rx.globalMatch(text);
    while (it.hasNext()) {
        QString p = it.next().captured(1);
        p.replace("\\\\", "\\");
        if (QDir(p).exists()) {
            const QString canonical = QDir(p).canonicalPath();
            if (!libs.contains(canonical)) libs << canonical;
        }
    }
    return libs;
}

void SteamScanner::collectExecutables(const QString &dir, int depth, QStringList &out) {
    if (depth < 0 || out.size() >= 256) return;
    QDir d(dir);
    if (!d.exists()) return;

    const auto files = d.entryInfoList({"*.exe", "*.EXE"}, QDir::Files | QDir::NoSymLinks);
    for (const auto &fi : files) {
        out << fi.absoluteFilePath();
        if (out.size() >= 256) return;
    }

    if (depth == 0) return;
    static const QSet<QString> skip = {
        "__installer", "redist", "_commonredist", "directxredist", "easyanticheat",
        "battleye", "crashreportclient", "support", "prerequisites", "prereqs"
    };
    const auto dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &sub : dirs) {
        if (skip.contains(sub.fileName().toLower())) continue;
        collectExecutables(sub.absoluteFilePath(), depth - 1, out);
        if (out.size() >= 256) return;
    }
}

namespace {
QString executableKey(const QString &value) {
    const QString normalized = value.normalized(QString::NormalizationForm_KD).toLower();
    QString out;
    out.reserve(normalized.size());
    for (const QChar ch : normalized) {
        if (ch.isLetterOrNumber())
            out.append(ch);
    }
    return out;
}

QStringList meaningfulExecutableTokens(const QString &value) {
    static const QSet<QString> ignored = {
        QStringLiteral("the"), QStringLiteral("and"), QStringLiteral("for"),
        QStringLiteral("game"), QStringLiteral("edition"), QStringLiteral("standard"),
        QStringLiteral("deluxe"), QStringLiteral("ultimate"), QStringLiteral("gold"),
        QStringLiteral("complete"), QStringLiteral("remastered"), QStringLiteral("remaster"),
        QStringLiteral("windows"), QStringLiteral("steam"), QStringLiteral("pc")
    };
    QString normalized = value.normalized(QString::NormalizationForm_KD).toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    QStringList out;
    for (const QString &token : normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (token.size() >= 3 && !ignored.contains(token) && !out.contains(token))
            out << token;
    }
    return out;
}

QString executableInitialism(const QString &value) {
    QString normalized = value.normalized(QString::NormalizationForm_KD).toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    QString out;
    for (const QString &token : normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (token == QStringLiteral("the") || token == QStringLiteral("a") || token == QStringLiteral("an"))
            continue;
        if (!token.isEmpty())
            out.append(token.front());
    }
    return out;
}

QStringList ubisoftGamesRoots(const QString &prefix) {
    const QString cleanPrefix = QDir::cleanPath(prefix.trimmed());
    if (cleanPrefix.isEmpty())
        return {};
    const QStringList candidates = {
        QDir(cleanPrefix).filePath(QStringLiteral("drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games")),
        QDir(cleanPrefix).filePath(QStringLiteral("drive_c/Program Files/Ubisoft/Ubisoft Game Launcher/games")),
        QDir(cleanPrefix).filePath(QStringLiteral("drive_c/Program Files (x86)/Ubisoft Game Launcher/games")),
        QDir(cleanPrefix).filePath(QStringLiteral("drive_c/Program Files/Ubisoft Game Launcher/games"))
    };
    QStringList roots;
    for (const QString &candidate : candidates) {
        if (QDir(candidate).exists()) {
            const QString canonical = QDir(candidate).canonicalPath();
            const QString normalized = canonical.isEmpty() ? QDir(candidate).absolutePath() : canonical;
            if (!roots.contains(normalized))
                roots << normalized;
        }
    }
    return roots;
}

bool hasAnyExecutableMarker(const QString &value, const QStringList &markers) {
    for (const QString &marker : markers) {
        if (value.contains(marker))
            return true;
    }
    return false;
}
}


QStringList SteamScanner::knownLibraries() {
    QStringList libraries;
    QSet<QString> seen;
    for (const QString &root : steamRoots()) {
        for (const QString &library : librariesForRoot(root)) {
            const QString canonical = QDir(library).canonicalPath();
            const QString normalized = canonical.isEmpty() ? QDir(library).absolutePath() : canonical;
            if (!normalized.isEmpty() && !seen.contains(normalized)) {
                seen.insert(normalized);
                libraries << normalized;
            }
        }
    }
    return libraries;
}

QString SteamScanner::findProtonPrefix(const QString &appId, const QStringList &libraries,
                                       const QString &preferredLibrary,
                                       QStringList *candidatePaths,
                                       QString *resolvedLibrary) {
    if (candidatePaths) candidatePaths->clear();
    if (resolvedLibrary) resolvedLibrary->clear();
    if (appId.trimmed().isEmpty()) return {};

    QStringList ordered;
    const QString preferred = preferredLibrary.trimmed();
    if (!preferred.isEmpty()) {
        const QString canonical = QDir(preferred).canonicalPath();
        ordered << (canonical.isEmpty() ? QDir(preferred).absolutePath() : canonical);
    }
    for (const QString &library : libraries) {
        const QString canonical = QDir(library).canonicalPath();
        const QString normalized = canonical.isEmpty() ? QDir(library).absolutePath() : canonical;
        if (!normalized.isEmpty() && !ordered.contains(normalized))
            ordered << normalized;
    }

    QString existingIncomplete;
    QString existingIncompleteLibrary;
    for (const QString &library : ordered) {
        const QString candidate = QDir(library).filePath(QStringLiteral("steamapps/compatdata/%1/pfx").arg(appId));
        if (candidatePaths) candidatePaths->append(candidate);
        if (QDir(QDir(candidate).filePath(QStringLiteral("drive_c"))).exists()) {
            if (resolvedLibrary) *resolvedLibrary = library;
            return candidate;
        }
        if (existingIncomplete.isEmpty() && QDir(candidate).exists()) {
            existingIncomplete = candidate;
            existingIncompleteLibrary = library;
        }
    }

    if (!existingIncomplete.isEmpty() && resolvedLibrary)
        *resolvedLibrary = existingIncompleteLibrary;
    return existingIncomplete;
}

QString SteamScanner::titleInitialism(const QString &title) {
    return executableInitialism(title);
}

QString SteamScanner::findBestExecutable(const QString &gameDir, QString &api, QString &arch) {
    return findBestExecutable(gameDir, QFileInfo(gameDir).fileName(), api, arch, nullptr);
}

QString SteamScanner::findBestExecutable(const QString &gameDir, const QString &preferredName,
                                         QString &api, QString &arch, QStringList *candidateSummary) {
    QStringList exes;
    collectExecutables(gameDir, 6, exes);
    if (exes.isEmpty()) return {};

    struct Candidate {
        QString path;
        QString api;
        QString arch;
        int score = -9999;
    };
    QVector<Candidate> ranked;
    ranked.reserve(exes.size());

    const QString base = QFileInfo(gameDir).fileName().toLower();
    const QString baseKey = executableKey(base);
    const QString titleKey = executableKey(preferredName);
    const QString titleInitialism = executableInitialism(preferredName);
    const QStringList titleTokens = meaningfulExecutableTokens(preferredName);
    const QStringList baseTokens = meaningfulExecutableTokens(base);

    const QStringList hardLauncherMarkers = {
        QStringLiteral("ubisoftconnect"), QStringLiteral("ubisoftgamelauncher"),
        QStringLiteral("uplaylauncher"), QStringLiteral("uplaywebcore"),
        QStringLiteral("uplaybrowser"), QStringLiteral("ubisoftconnectinstaller"),
        QStringLiteral("ubisoftextension"), QStringLiteral("upc")
    };
    const QStringList genericToolMarkers = {
        QStringLiteral("launcher"), QStringLiteral("updater"), QStringLiteral("patcher"),
        QStringLiteral("autopatch"), QStringLiteral("installer"), QStringLiteral("helper"),
        QStringLiteral("crash"), QStringLiteral("report"), QStringLiteral("benchmark"),
        QStringLiteral("webcore"), QStringLiteral("browser"), QStringLiteral("overlay"),
        QStringLiteral("service"), QStringLiteral("bootstrap"), QStringLiteral("unins"),
        QStringLiteral("setup"), QStringLiteral("redist")
    };

    for (const auto &path : exes) {
        const QFileInfo fi(path);
        const QString n = fi.completeBaseName().toLower();
        const QString nKey = executableKey(n);
        const QString p = QDir::fromNativeSeparators(path).toLower();
        auto pe = PeParser::inspect(path);
        if (!pe.valid) continue;

        int score = 0;
        if (!titleKey.isEmpty() && nKey == titleKey) score += 180;
        if (!baseKey.isEmpty() && nKey == baseKey) score += 145;
        if (titleInitialism.size() >= 3 && nKey == titleInitialism) score += 170;
        else if (titleInitialism.size() >= 3 && nKey.startsWith(titleInitialism)) score += 70;
        if (nKey.size() >= 5 && !titleKey.isEmpty() &&
            (titleKey.contains(nKey) || nKey.contains(titleKey))) score += 70;
        if (nKey.size() >= 5 && !baseKey.isEmpty() &&
            (baseKey.contains(nKey) || nKey.contains(baseKey))) score += 55;

        int tokenScore = 0;
        for (const QString &token : titleTokens) {
            if (nKey.contains(executableKey(token)))
                tokenScore += 30;
        }
        score += qMin(tokenScore, 100);
        int baseTokenScore = 0;
        for (const QString &token : baseTokens) {
            if (nKey.contains(executableKey(token)))
                baseTokenScore += 15;
        }
        score += qMin(baseTokenScore, 45);

        if (titleInitialism.size() >= 3) {
            QString relativeForSegments = QDir::fromNativeSeparators(QDir(gameDir).relativeFilePath(path));
            const QStringList segments = relativeForSegments.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (const QString &segment : segments) {
                const QString segmentKey = executableKey(QFileInfo(segment).completeBaseName());
                if (segmentKey == titleInitialism) {
                    score += 120;
                    break;
                }
            }
        }

        if (p.contains(QStringLiteral("/binaries/win64/"))) score += 60;
        if (p.contains(QStringLiteral("/binaries/win32/"))) score += 45;
        if (n.contains(QStringLiteral("shipping"))) score += 45;
        if (pe.graphicsApi != QStringLiteral("Unknown")) score += 40;
        if (pe.architecture == QStringLiteral("x64")) score += 8;

        const qint64 sizeMiB = fi.size() / (1024 * 1024);
        if (sizeMiB >= 256) score += 35;
        else if (sizeMiB >= 64) score += 28;
        else if (sizeMiB >= 16) score += 18;
        else if (sizeMiB >= 4) score += 8;

        QString relative = QDir(gameDir).relativeFilePath(path);
        relative = QDir::fromNativeSeparators(relative);
        const int depth = relative.count(QLatin1Char('/'));
        if (depth == 0) score += 25;
        else if (depth <= 2) score += 12;
        else if (depth >= 5) score -= 8;

        if (hasAnyExecutableMarker(nKey, hardLauncherMarkers)) score -= 240;
        if (hasAnyExecutableMarker(nKey, genericToolMarkers)) score -= 110;
        if (p.contains(QStringLiteral("/ubisoft connect/")) ||
            p.contains(QStringLiteral("/ubisoftconnect/")) ||
            p.contains(QStringLiteral("/uplay/"))) score -= 120;
        if (p.contains(QStringLiteral("/support/")) ||
            p.contains(QStringLiteral("/_commonredist/")) ||
            p.contains(QStringLiteral("/redistributable")) ||
            p.contains(QStringLiteral("/installer/"))) score -= 80;

        ranked.push_back({path, pe.graphicsApi, pe.architecture, score});
    }

    if (ranked.isEmpty()) return {};
    std::sort(ranked.begin(), ranked.end(), [](const Candidate &a, const Candidate &b) {
        if (a.score != b.score) return a.score > b.score;
        return QString::localeAwareCompare(a.path, b.path) < 0;
    });

    Candidate &best = ranked.first();
    QStringList apiEvidence;
    if (best.api == QStringLiteral("Unknown")) {
        const QString fallbackApi = PeParser::detectGraphicsApiFallback(best.path, &apiEvidence);
        if (fallbackApi != QStringLiteral("Unknown"))
            best.api = fallbackApi;
    }
    api = best.api;
    arch = best.arch;

    if (candidateSummary) {
        candidateSummary->clear();
        const int limit = qMin(8, static_cast<int>(ranked.size()));
        for (int i = 0; i < limit; ++i) {
            const Candidate &candidate = ranked.at(i);
            candidateSummary->append(QStringLiteral("%1 points | %2 | %3 | %4")
                                         .arg(candidate.score)
                                         .arg(candidate.api.isEmpty() ? QStringLiteral("Unknown API") : candidate.api)
                                         .arg(candidate.arch.isEmpty() ? QStringLiteral("Unknown arch") : candidate.arch)
                                         .arg(candidate.path));
        }
        if (!apiEvidence.isEmpty()) {
            candidateSummary->append(QStringLiteral("Graphics API fallback evidence:"));
            for (const QString &line : std::as_const(apiEvidence))
                candidateSummary->append(QStringLiteral("  %1").arg(line));
        }
    }
    return best.path;
}

bool SteamScanner::isLikelyLauncherExecutable(const QString &path) {
    const QString key = executableKey(QFileInfo(path).completeBaseName());
    const QStringList markers = {
        QStringLiteral("ubisoftconnect"), QStringLiteral("ubisoftgamelauncher"),
        QStringLiteral("uplaylauncher"), QStringLiteral("uplaywebcore"),
        QStringLiteral("uplaybrowser"), QStringLiteral("ubisoftconnectinstaller"),
        QStringLiteral("ubisoftextension"), QStringLiteral("upc")
    };
    return hasAnyExecutableMarker(key, markers);
}

QString SteamScanner::findBestUbisoftExecutable(const QString &prefix, const QString &preferredName,
                                                 QString &api, QString &arch, QString &installPath,
                                                 QStringList *candidateSummary) {
    installPath.clear();
    const QStringList roots = ubisoftGamesRoots(prefix);
    if (roots.isEmpty())
        return {};

    struct RootCandidate {
        QString path;
        QString api;
        QString arch;
        QString installPath;
        QStringList summary;
        int score = -9999;
    };
    QVector<RootCandidate> ranked;

    for (const QString &root : roots) {
        QString candidateApi;
        QString candidateArch;
        QStringList summary;
        const QString path = findBestExecutable(root, preferredName, candidateApi, candidateArch, &summary);
        if (path.isEmpty())
            continue;

        int score = 0;
        if (!summary.isEmpty()) {
            const QRegularExpression scoreRx(QStringLiteral("^(-?\\d+)\\s+points"));
            const auto match = scoreRx.match(summary.first());
            if (match.hasMatch())
                score = match.captured(1).toInt();
        }

        const QString relative = QDir::fromNativeSeparators(QDir(root).relativeFilePath(path));
        const QString topLevel = relative.section(QLatin1Char('/'), 0, 0);
        const QString resolvedInstall = topLevel.isEmpty() || topLevel == QStringLiteral(".")
            ? root
            : QDir(root).filePath(topLevel);

        QStringList labelledSummary;
        labelledSummary.reserve(summary.size());
        for (const QString &line : std::as_const(summary))
            labelledSummary << QStringLiteral("Ubisoft games root %1 | %2").arg(root, line);

        ranked.push_back({path, candidateApi, candidateArch, resolvedInstall, labelledSummary, score});
    }

    if (ranked.isEmpty())
        return {};
    std::sort(ranked.begin(), ranked.end(), [](const RootCandidate &a, const RootCandidate &b) {
        if (a.score != b.score) return a.score > b.score;
        return QString::localeAwareCompare(a.path, b.path) < 0;
    });

    const RootCandidate &best = ranked.first();
    api = best.api;
    arch = best.arch;
    installPath = QDir(best.installPath).absolutePath();
    if (candidateSummary) {
        candidateSummary->clear();
        for (const RootCandidate &candidate : std::as_const(ranked)) {
            for (const QString &line : candidate.summary) {
                candidateSummary->append(line);
                if (candidateSummary->size() >= 12)
                    return best.path;
            }
        }
    }
    return best.path;
}

bool SteamScanner::isSteamTool(const QString &name, const QString &installDir) {
    const QString n = name.trimmed().toLower();
    const QString d = installDir.trimmed().toLower();

    // Steam installs compatibility tools/runtimes as normal appmanifest entries.
    // They can contain Windows helper EXEs, so executable discovery alone is not
    // enough to distinguish them from games. Keep this deliberately focused on
    // packages Steam itself exposes as Proton/runtime components.
    const auto protonName = [](const QString &value) {
        return value == QStringLiteral("proton") ||
               value.startsWith(QStringLiteral("proton ")) ||
               value.startsWith(QStringLiteral("proton-")) ||
               value.startsWith(QStringLiteral("proton_"));
    };

    if (protonName(n) || protonName(d)) return true;
    if (n.startsWith(QStringLiteral("steam linux runtime")) ||
        d.startsWith(QStringLiteral("steam linux runtime"))) return true;
    if (n == QStringLiteral("steamworks common redistributables") ||
        d == QStringLiteral("steamworks shared")) return true;

    return false;
}

QString SteamScanner::detectEngine(const QString &gameDir, const QString &exePath) {
    const QString lowerExe = exePath.toLower();
    if (lowerExe.contains("/binaries/win64/") || lowerExe.contains("/binaries/win32/") ||
        QDir(gameDir + "/Engine/Binaries").exists() || QDir(gameDir + "/engine/binaries").exists())
        return "Unreal Engine";

    QDir d(gameDir);
    // RE Engine games normally ship one or more re_chunk_000.pak files in the
    // game root. This is a stronger local signal than trying to infer the
    // engine from the executable name alone.
    if (!d.entryList({QStringLiteral("re_chunk_000.pak*"), QStringLiteral("re_dlc_*.pak*")}, QDir::Files).isEmpty())
        return "RE Engine";

    if (!d.entryList({"UnityPlayer.dll", "unityplayer.dll"}, QDir::Files).isEmpty()) return "Unity";
    const auto dirs = d.entryList({"*_Data", "*_data"}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (!dirs.isEmpty()) return "Unity";
    return "Unknown";
}

bool SteamScanner::supportsReFramework(const QString &gameName, const QString &engine) {
    // REFramework is the standard runtime for RE Engine titles. Treat a
    // positive local engine detection as sufficient instead of waiting for a
    // title to appear in an external support list. Keep the title aliases
    // below as a fallback when engine detection is inconclusive.
    if (engine.compare(QStringLiteral("RE Engine"), Qt::CaseInsensitive) == 0)
        return true;

    QString key = gameName.normalized(QString::NormalizationForm_KD).toLower();
    QString compact;
    compact.reserve(key.size());
    for (const QChar ch : key) {
        if (ch.isLetterOrNumber()) compact.append(ch);
    }

    const QStringList supported = {
        QStringLiteral("devilmaycry5"),
        QStringLiteral("residentevil2"),
        QStringLiteral("residentevil3"),
        QStringLiteral("residentevil4"),
        QStringLiteral("residentevil7"),
        QStringLiteral("residentevilbiohazard"),
        QStringLiteral("residentevil8"),
        QStringLiteral("residentevilvillage"),
        QStringLiteral("residentevil9"),
        QStringLiteral("residentevilrequiem"),
        QStringLiteral("monsterhunterrise"),
        QStringLiteral("monsterhunterwilds"),
        QStringLiteral("monsterhunterstories3"),
        QStringLiteral("streetfighter6"),
        QStringLiteral("dragonsdogma2"),
        QStringLiteral("dragonsdogmaii"),
        QStringLiteral("deadrisingdeluxeremaster"),
        QStringLiteral("ghostsngoblinsresurrection"),
        QStringLiteral("apollojusticeaceattorneytrilogy"),
        QStringLiteral("kunitsugamipathofthegoddess"),
        QStringLiteral("onimusha2samuraisdestiny"),
        QStringLiteral("onimushawayofthesword"),
        QStringLiteral("pragmata"),
        QStringLiteral("megamanstarforcelegacycollection"),
        QStringLiteral("starforcelegacycollection")
    };
    for (const QString &alias : supported) {
        if (compact.contains(alias))
            return true;
    }
    return false;
}

QVector<GameInfo> SteamScanner::scan() {
    QVector<GameInfo> games;
    QSet<QString> seenAppIds;

    struct LibraryInfo {
        QString root;
        QString path;
    };

    // Build one global view of every Steam library before scanning manifests.
    // A game's install and its compatdata prefix do not always remain in the
    // same library (for example after library moves or custom storage layouts),
    // so prefix discovery must not be constrained to the manifest's library.
    QVector<LibraryInfo> allLibraries;
    QSet<QString> seenLibraries;
    for (const auto &root : steamRoots()) {
        for (const auto &library : librariesForRoot(root)) {
            const QString canonical = QDir(library).canonicalPath();
            const QString normalized = canonical.isEmpty() ? QDir(library).absolutePath() : canonical;
            if (normalized.isEmpty() || seenLibraries.contains(normalized))
                continue;
            seenLibraries.insert(normalized);
            allLibraries.push_back({root, normalized});
        }
    }

    QStringList allLibraryPaths;
    allLibraryPaths.reserve(allLibraries.size());
    for (const auto &libraryInfo : allLibraries)
        allLibraryPaths << libraryInfo.path;

    for (const auto &libraryInfo : allLibraries) {
        const QString &root = libraryInfo.root;
        const QString &library = libraryInfo.path;
        const QString steamapps = library + "/steamapps";
        QDir apps(steamapps);
        const auto manifests = apps.entryInfoList({"appmanifest_*.acf"}, QDir::Files, QDir::Name);
        for (const auto &acf : manifests) {
            QFile f(acf.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString text = QString::fromUtf8(f.readAll());
            QString appId = vdfValue(text, "appid");
            if (appId.isEmpty()) {
                const QRegularExpression idRx("appmanifest_(\\d+)\\.acf", QRegularExpression::CaseInsensitiveOption);
                const auto m = idRx.match(acf.fileName());
                if (m.hasMatch()) appId = m.captured(1);
            }
            if (appId.isEmpty() || seenAppIds.contains(appId)) continue;

            const QString name = vdfValue(text, "name");
            const QString installDir = vdfValue(text, "installdir");
            if (name.isEmpty() || installDir.isEmpty()) continue;
            if (isSteamTool(name, installDir)) continue;
            const QString gameDir = steamapps + "/common/" + installDir;
            if (!QDir(gameDir).exists()) continue;

            GameInfo g;
            g.name = name;
            g.appId = appId;
            g.installPath = QDir(gameDir).absolutePath();
            g.steamLibrary = library;

            // Prefer the install library, then search every other Steam library
            // for this AppID's compatdata prefix. The shared helper is also
            // covered by regression tests so moved compatdata stays working.
            QStringList prefixCandidatePaths;
            QString resolvedPrefixLibrary;
            g.protonPrefix = findProtonPrefix(appId, allLibraryPaths, library,
                                               &prefixCandidatePaths, &resolvedPrefixLibrary);
            if (!g.protonPrefix.isEmpty() &&
                QDir(QDir(g.protonPrefix).filePath(QStringLiteral("drive_c"))).exists()) {
                if (resolvedPrefixLibrary != library) {
                    g.importDiagnostics << QStringLiteral("Steam compatdata prefix resolved from another Steam library: %1")
                                               .arg(g.protonPrefix);
                }
            } else if (!g.protonPrefix.isEmpty()) {
                g.importDiagnostics << QStringLiteral("Steam compatdata directory exists but does not contain drive_c: %1")
                                           .arg(g.protonPrefix);
            } else {
                g.importDiagnostics << QStringLiteral("No Steam compatdata prefix was found for AppID %1 across %2 Steam librar%3.")
                                           .arg(appId)
                                           .arg(prefixCandidatePaths.size())
                                           .arg(prefixCandidatePaths.size() == 1 ? QStringLiteral("y") : QStringLiteral("ies"));
            }

            QStringList executableCandidates;
            g.exePath = findBestExecutable(g.installPath, g.name, g.graphicsApi, g.architecture, &executableCandidates);
            const bool launcherOnlyCandidate = !g.exePath.isEmpty() && isLikelyLauncherExecutable(g.exePath);
            if ((g.exePath.isEmpty() || launcherOnlyCandidate) && !g.protonPrefix.isEmpty()) {
                QString ubisoftInstallPath;
                QStringList ubisoftCandidates;
                const QString ubisoftExe = findBestUbisoftExecutable(g.protonPrefix, g.name, g.graphicsApi, g.architecture,
                                                                     ubisoftInstallPath, &ubisoftCandidates);
                if (!ubisoftExe.isEmpty()) {
                    g.sourceMetadata.insert(QStringLiteral("launcherInstallPath"), g.installPath);
                    if (launcherOnlyCandidate)
                        g.sourceMetadata.insert(QStringLiteral("rejectedLauncherExecutable"), g.exePath);
                    g.installPath = ubisoftInstallPath;
                    g.exePath = ubisoftExe;
                    executableCandidates = ubisoftCandidates;
                    g.importDiagnostics << QStringLiteral("Resolved the game executable inside the Ubisoft Connect prefix: %1")
                                               .arg(ubisoftExe);
                } else if (launcherOnlyCandidate) {
                    g.importDiagnostics << QStringLiteral("The only executable candidate was a Ubisoft/Uplay launcher, not the game executable: %1")
                                               .arg(g.exePath);
                    g.exePath.clear();
                }
            }
            if (g.exePath.isEmpty())
                g.importDiagnostics << QStringLiteral("Could not resolve a Windows executable from the Steam install directory or Ubisoft Connect games inside the resolved prefix.");
            if (g.graphicsApi.isEmpty()) g.graphicsApi = QStringLiteral("Unknown");
            if (g.architecture.isEmpty()) g.architecture = QStringLiteral("Unknown");
            g.detectedExePath = g.exePath;
            g.engine = detectEngine(g.installPath, g.exePath);
            g.reframeworkSupported = supportsReFramework(g.name, g.engine);
            g.sourceMetadata.insert(QStringLiteral("manifest"), acf.absoluteFilePath());
            g.sourceMetadata.insert(QStringLiteral("steamRoot"), root);
            g.sourceMetadata.insert(QStringLiteral("library"), library);
            g.sourceMetadata.insert(QStringLiteral("installDir"), installDir);
            g.sourceMetadata.insert(QStringLiteral("preferredPrefix"), prefixCandidatePaths.isEmpty() ? QString() : prefixCandidatePaths.first());
            g.sourceMetadata.insert(QStringLiteral("resolvedPrefix"), g.protonPrefix);
            g.sourceMetadata.insert(QStringLiteral("resolvedPrefixLibrary"), resolvedPrefixLibrary);
            g.sourceMetadata.insert(QStringLiteral("prefixCandidates"), prefixCandidatePaths);
            g.sourceMetadata.insert(QStringLiteral("autoSelectedExecutable"), g.exePath);
            g.sourceMetadata.insert(QStringLiteral("executableCandidates"), executableCandidates);

            if (!g.exePath.isEmpty()) {
                const QString exeDir = QFileInfo(g.exePath).absolutePath();
                const QStringList proxies = {"dxgi.dll", "d3d9.dll", "d3d10.dll", "d3d11.dll", "d3d12.dll", "opengl32.dll"};
                QFile stateFile(exeDir + "/.reno119-state.json");
                if (stateFile.open(QIODevice::ReadOnly)) {
                    const auto state = QJsonDocument::fromJson(stateFile.readAll()).object();
                    const QString ownedProxy = state.value("reshadeProxy").toString();
                    const QString ownedAddon = state.value("renodxFile").toString();
                    if (!ownedProxy.isEmpty() && proxies.contains(ownedProxy, Qt::CaseInsensitive) &&
                        QFileInfo::exists(exeDir + "/" + ownedProxy))
                        g.reshadeInstalled = true;
                    if (!ownedAddon.isEmpty() && QFileInfo::exists(exeDir + "/reshade-addons/" + ownedAddon))
                        g.renodxInstalled = true;
                }
            }

            seenAppIds.insert(appId);
            games.push_back(g);
        }
    }

    std::sort(games.begin(), games.end(), [](const GameInfo &a, const GameInfo &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return games;
}
