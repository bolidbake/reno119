#include "OptiScalerIntegration.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
QString optiVirtualKeyName(int code) {
    switch (code) {
    case 0x21: return QStringLiteral("Page Up");
    case 0x22: return QStringLiteral("Page Down");
    case 0x23: return QStringLiteral("End");
    case 0x24: return QStringLiteral("Home");
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

int optiParseVirtualKey(const QString &raw, int fallback = 0x2D) {
    const QString value = raw.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0)
        return fallback;
    bool ok = false;
    const int code = value.toInt(&ok, 0);
    return ok ? code : fallback;
}

bool optiIniHasSection(const QString &path, const QString &section) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString text = QString::fromUtf8(f.readAll());
    const QRegularExpression sectionRx(QStringLiteral("(?im)^\\s*\\[%1\\]\\s*$")
                                           .arg(QRegularExpression::escape(section)));
    return sectionRx.match(text).hasMatch();
}
}

QString OptiScalerIntegration::readIniValue(const QString &path, const QString &section, const QString &key) const {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString text = QString::fromUtf8(f.readAll());
    QString body = text;
    if (!section.isEmpty()) {
        const QRegularExpression sectionRx(QStringLiteral("(?ims)^\\s*\\[%1\\]\\s*$([\\s\\S]*?)(?=^\\s*\\[|\\z)")
                                               .arg(QRegularExpression::escape(section)));
        const auto sectionMatch = sectionRx.match(text);
        if (!sectionMatch.hasMatch())
            return {};
        body = sectionMatch.captured(1);
    } else {
        const int firstSection = text.indexOf(QRegularExpression(QStringLiteral("(?m)^\\s*\\[")));
        if (firstSection >= 0)
            body = text.left(firstSection);
    }
    const QRegularExpression keyRx(QStringLiteral("(?im)^\\s*%1\\s*=\\s*([^;\\r\\n]*)")
                                       .arg(QRegularExpression::escape(key)));
    const auto keyMatch = keyRx.match(body);
    return keyMatch.hasMatch() ? keyMatch.captured(1).trimmed() : QString();
}

bool OptiScalerIntegration::writeIniValue(const QString &path, const QString &section, const QString &key, const QString &value) const {
    QFile f(path);
    QString text;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        text = QString::fromUtf8(f.readAll());
        f.close();
    }

    if (section.isEmpty()) {
        int firstSection = text.indexOf(QRegularExpression(QStringLiteral("(?m)^\\s*\\[")));
        if (firstSection < 0)
            firstSection = text.size();
        QString prefix = text.left(firstSection);
        QString suffix = text.mid(firstSection);
        const QRegularExpression keyRx(QStringLiteral("(?im)^(\\s*%1\\s*=\\s*)([^;\\r\\n]*)(.*)$")
                                           .arg(QRegularExpression::escape(key)));
        const auto keyMatch = keyRx.match(prefix);
        if (keyMatch.hasMatch()) {
            prefix.replace(keyMatch.capturedStart(2), keyMatch.capturedLength(2), value);
        } else {
            if (!prefix.isEmpty() && !prefix.endsWith('\n'))
                prefix += '\n';
            prefix += QStringLiteral("%1=%2\n").arg(key, value);
        }
        text = prefix + suffix;
    } else {
        const QRegularExpression sectionRx(QStringLiteral("(?ims)^\\s*\\[%1\\]\\s*$([\\s\\S]*?)(?=^\\s*\\[|\\z)")
                                               .arg(QRegularExpression::escape(section)));
        const auto sectionMatch = sectionRx.match(text);
        if (!sectionMatch.hasMatch()) {
            if (!text.isEmpty() && !text.endsWith('\n'))
                text += '\n';
            text += QStringLiteral("\n[%1]\n%2=%3\n").arg(section, key, value);
        } else {
            const int bodyStart = sectionMatch.capturedStart(1);
            const int bodyLength = sectionMatch.capturedLength(1);
            QString body = sectionMatch.captured(1);
            const QRegularExpression keyRx(QStringLiteral("(?im)^(\\s*%1\\s*=\\s*)([^;\\r\\n]*)(.*)$")
                                               .arg(QRegularExpression::escape(key)));
            const auto keyMatch = keyRx.match(body);
            if (keyMatch.hasMatch()) {
                body.replace(keyMatch.capturedStart(2), keyMatch.capturedLength(2), value);
            } else {
                if (!body.startsWith('\n') && !body.startsWith('\r'))
                    body.prepend('\n');
                body += QStringLiteral("%1=%2\n").arg(key, value);
            }
            text.replace(bodyStart, bodyLength, body);
        }
    }

    QSaveFile out(path);
    out.setDirectWriteFallback(true);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray bytes = text.toUtf8();
    return out.write(bytes) == bytes.size() && out.commit();
}

QVariantMap OptiScalerIntegration::overlayHotkey(int row) const {
    QVariantMap out;
    const QString dir = exeDirFor(row);
    if (dir.isEmpty())
        return out;
    const QString ini = findOptiIni(dir);
    if (ini.isEmpty()) {
        out[QStringLiteral("available")] = false;
        out[QStringLiteral("code")] = 0x2D;
        out[QStringLiteral("name")] = QStringLiteral("Insert");
        return out;
    }
    // Current OptiScaler keeps the overlay shortcut in [Menu].  Older
    // configurations placed ShortcutKey before the first section, so retain a
    // read fallback for those files.
    QString raw = readIniValue(ini, QStringLiteral("Menu"), QStringLiteral("ShortcutKey"));
    if (raw.isEmpty())
        raw = readIniValue(ini, QString(), QStringLiteral("ShortcutKey"));
    const int code = optiParseVirtualKey(raw, 0x2D);
    out[QStringLiteral("available")] = true;
    out[QStringLiteral("code")] = code;
    out[QStringLiteral("name")] = optiVirtualKeyName(code);
    out[QStringLiteral("raw")] = raw.isEmpty() ? QStringLiteral("auto") : raw;
    out[QStringLiteral("path")] = ini;
    return out;
}

bool OptiScalerIntegration::setOverlayHotkey(int row, int virtualKey) {
    beginStatusForRow(row);
    const QString dir = exeDirFor(row);
    if (dir.isEmpty()) {
        setStatus(QStringLiteral("No game executable selected."));
        return false;
    }
    const QString ini = findOptiIni(dir);
    if (ini.isEmpty()) {
        setStatus(QStringLiteral("OptiScaler.ini was not found, so the overlay hotkey cannot be changed."));
        return false;
    }
    if (virtualKey < 1 || virtualKey > 255) {
        setStatus(QStringLiteral("Invalid Windows virtual-key code."));
        return false;
    }
    // OptiScaler 0.9+ stores ShortcutKey in [Menu].  Preserve compatibility
    // with legacy configs that still have a root-level ShortcutKey, but prefer
    // [Menu] whenever that section exists (and create it for modern configs
    // that happen to be missing the key).
    const bool hasMenuSection = optiIniHasSection(ini, QStringLiteral("Menu"));
    const QString menuPrevious = readIniValue(ini, QStringLiteral("Menu"), QStringLiteral("ShortcutKey"));
    const QString legacyPrevious = readIniValue(ini, QString(), QStringLiteral("ShortcutKey"));
    const QString targetSection = (hasMenuSection || legacyPrevious.isEmpty())
                                      ? QStringLiteral("Menu")
                                      : QString();
    const QString previous = targetSection.isEmpty() ? legacyPrevious : menuPrevious;
    const QString next = QStringLiteral("0x%1")
                             .arg(QString::number(virtualKey, 16).toUpper().rightJustified(2, QLatin1Char('0')));
    if (!writeIniValue(ini, targetSection, QStringLiteral("ShortcutKey"), next)) {
        setStatus(QStringLiteral("Could not update OptiScaler.ini %1ShortcutKey.")
                      .arg(targetSection.isEmpty() ? QString() : QStringLiteral("[Menu] ")));
        return false;
    }
    QVariantMap state = readIntegrationState(row);
    appendHistory(state, QStringLiteral("Changed OptiScaler overlay hotkey from %1 to %2")
                             .arg(optiVirtualKeyName(optiParseVirtualKey(previous, 0x2D)), optiVirtualKeyName(virtualKey)));
    writeIntegrationState(row, state);
    bumpRevision();
    setStatus(QStringLiteral("OptiScaler overlay hotkey changed to %1.").arg(optiVirtualKeyName(virtualKey)));
    return true;
}
