#include "InstallerManagerInternal.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace Reno119::InstallerInternal {
QJsonObject tweakProfileSettings(const QVariantMap &plan) {
    QJsonObject result;
    for (const QString &key : {QStringLiteral("id"), QStringLiteral("engineChanges"), QStringLiteral("reshadeChanges"), QStringLiteral("engineReadOnly")})
        result.insert(key, QJsonValue::fromVariant(plan.value(key)));
    return result;
}

QByteArray mergedTweakIni(const QString &text, const QVariantList &changes) {
    QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    for (const QVariant &entry : changes) {
        const QVariantMap c = entry.toMap();
        const QString section = c.value(QStringLiteral("section")).toString().trimmed();
        const QString key = c.value(QStringLiteral("key")).toString().trimmed();
        const QString value = c.value(QStringLiteral("value")).toString();
        if (section.isEmpty() || key.isEmpty())
            continue;

        int sectionStart = -1;
        int sectionEnd = lines.size();
        for (int i = 0; i < lines.size(); ++i) {
            const QString trimmed = lines.at(i).trimmed();
            if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
                const QString current = trimmed.mid(1, trimmed.size() - 2).trimmed();
                if (sectionStart >= 0) {
                    sectionEnd = i;
                    break;
                }
                if (current.compare(section, Qt::CaseInsensitive) == 0)
                    sectionStart = i;
            }
        }

        if (sectionStart < 0) {
            if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
                lines << QString();
            lines << QStringLiteral("[%1]").arg(section);
            lines << key + QLatin1Char('=') + value;
            continue;
        }

        int keyIndex = -1;
        const QRegularExpression keyRx(QStringLiteral("^\\s*%1\\s*=").arg(QRegularExpression::escape(key)),
                                       QRegularExpression::CaseInsensitiveOption);
        for (int i = sectionStart + 1; i < sectionEnd; ++i) {
            if (keyRx.match(lines.at(i)).hasMatch()) {
                keyIndex = i;
                break;
            }
        }
        if (keyIndex >= 0)
            lines[keyIndex] = key + QLatin1Char('=') + value;
        else
            lines.insert(sectionEnd, key + QLatin1Char('=') + value);
    }

    return (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
}

QJsonObject tweakFileState(const QString &path) {
    QJsonObject result{{QStringLiteral("exists"), QFileInfo::exists(path)}};
    if (QFileInfo::exists(path)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            result.insert(QStringLiteral("error"), QStringLiteral("Cannot read ") + path);
            return result;
        }
        result.insert(QStringLiteral("sha256"), QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()));
        result.insert(QStringLiteral("permissions"), static_cast<int>(QFile::permissions(path).toInt()));
    }
    return result;
}

QString virtualKeyName(int code) {
    switch (code) {
    case 0x08: return QStringLiteral("Backspace");
    case 0x09: return QStringLiteral("Tab");
    case 0x0D: return QStringLiteral("Enter");
    case 0x1B: return QStringLiteral("Escape");
    case 0x20: return QStringLiteral("Space");
    case 0x21: return QStringLiteral("Page Up");
    case 0x22: return QStringLiteral("Page Down");
    case 0x23: return QStringLiteral("End");
    case 0x24: return QStringLiteral("Home");
    case 0x25: return QStringLiteral("Left");
    case 0x26: return QStringLiteral("Up");
    case 0x27: return QStringLiteral("Right");
    case 0x28: return QStringLiteral("Down");
    case 0x2D: return QStringLiteral("Insert");
    case 0x2E: return QStringLiteral("Delete");
    default:
        if (code >= 0x70 && code <= 0x7B)
            return QStringLiteral("F%1").arg(code - 0x6F);
        if (code >= 0x30 && code <= 0x39)
            return QString(QChar(code));
        if (code >= 0x41 && code <= 0x5A)
            return QString(QChar(code));
        return QStringLiteral("VK 0x%1").arg(code, 2, 16, QLatin1Char('0')).toUpper();
    }
}

int parseVirtualKey(const QString &raw, int fallback) {
    QString value = raw.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0)
        return fallback;
    bool ok = false;
    int code = value.toInt(&ok, 0);
    if (ok)
        return code;
    QString upper = value.toUpper();
    if (upper.startsWith(QStringLiteral("VK_")))
        upper.remove(0, 3);
    if (upper == QStringLiteral("INSERT")) return 0x2D;
    if (upper == QStringLiteral("DELETE")) return 0x2E;
    if (upper == QStringLiteral("HOME")) return 0x24;
    if (upper == QStringLiteral("END")) return 0x23;
    if (upper == QStringLiteral("PRIOR") || upper == QStringLiteral("PAGEUP")) return 0x21;
    if (upper == QStringLiteral("NEXT") || upper == QStringLiteral("PAGEDOWN")) return 0x22;
    if (upper.size() >= 2 && upper.at(0) == QLatin1Char('F')) {
        const int f = upper.mid(1).toInt(&ok);
        if (ok && f >= 1 && f <= 12) return 0x6F + f;
    }
    if (upper.size() == 1 && upper.at(0).isLetterOrNumber())
        return upper.at(0).unicode();
    return fallback;
}

bool writeSimpleKeyValue(const QString &path, const QString &key, const QString &value) {
    QFile input(path);
    QString text;
    if (input.open(QIODevice::ReadOnly | QIODevice::Text))
        text = QString::fromUtf8(input.readAll());

    const QRegularExpression rx(QStringLiteral("(?im)^(\\s*%1\\s*=\\s*)([^\\r\\n]*)(.*)$")
                                    .arg(QRegularExpression::escape(key)));
    const auto match = rx.match(text);
    if (match.hasMatch()) {
        text.replace(match.capturedStart(2), match.capturedLength(2), value);
    } else {
        if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
            text += QLatin1Char('\n');
        text += key + QLatin1Char('=') + value + QLatin1Char('\n');
    }

    QSaveFile out(path);
    out.setDirectWriteFallback(true);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray bytes = text.toUtf8();
    return out.write(bytes) == bytes.size() && out.commit();
}

QString findExistingReFrameworkConfig(const QString &exeDir, QString *rawMenuKey, bool *legacyMenuKey) {
    if (rawMenuKey)
        rawMenuKey->clear();
    if (legacyMenuKey)
        *legacyMenuKey = false;

    QDir dir(exeDir);
    QFileInfoList candidates;
    const QFileInfo canonical(dir.absoluteFilePath(QStringLiteral("re2_fw_config.txt")));
    if (canonical.exists() && canonical.isFile() && canonical.isReadable())
        candidates.push_back(canonical);

    const QFileInfoList discovered = dir.entryInfoList({QStringLiteral("*_fw_config.txt")},
                                                       QDir::Files | QDir::Readable,
                                                       QDir::Time);
    for (const QFileInfo &info : discovered) {
        if (canonical.exists() && info.absoluteFilePath() == canonical.absoluteFilePath())
            continue;
        candidates.push_back(info);
    }

    QString fallbackPath;
    const QRegularExpression currentRx(
        QStringLiteral("(?im)^\\s*REFrameworkConfig_MenuKey_V2\\s*=\\s*([^\\r\\n;]+)"));
    const QRegularExpression legacyRx(
        QStringLiteral("(?im)^\\s*MenuKey_V2\\s*=\\s*([^\\r\\n;]+)"));

    for (const QFileInfo &info : candidates) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString text = QString::fromUtf8(file.readAll());

        const auto currentMatch = currentRx.match(text);
        if (currentMatch.hasMatch()) {
            if (rawMenuKey)
                *rawMenuKey = currentMatch.captured(1).trimmed();
            return info.absoluteFilePath();
        }

        const auto legacyMatch = legacyRx.match(text);
        if (legacyMatch.hasMatch()) {
            if (rawMenuKey)
                *rawMenuKey = legacyMatch.captured(1).trimmed();
            if (legacyMenuKey)
                *legacyMenuKey = true;
            return info.absoluteFilePath();
        }

        if (fallbackPath.isEmpty())
            fallbackPath = info.absoluteFilePath();
    }
    return fallbackPath;
}

QString backupComponentForReason(const QString &reason) {
    const QString lower = reason.toLower();
    if (lower.contains(QStringLiteral("reframework")))
        return QStringLiteral("reframework");
    if (lower.contains(QStringLiteral("renodx")))
        return QStringLiteral("renodx");
    if (lower.contains(QStringLiteral("reshade")))
        return QStringLiteral("reshade");
    return QStringLiteral("component");
}

} // namespace Reno119::InstallerInternal
