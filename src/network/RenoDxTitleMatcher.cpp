#include "RenoDxTitleMatcher.h"

#include <QRegularExpression>

QString RenoDxTitleMatcher::cleanDisplayTitle(const QString &rawName) {
    QString name = rawName;
    static const QRegularExpression markdownLink(
        R"(\[([^\]]+)\]\([^\)]+\))",
        QRegularExpression::CaseInsensitiveOption);
    name.replace(markdownLink, QStringLiteral("\\1"));
    name.replace(QStringLiteral("&amp;"), QStringLiteral("&"), Qt::CaseInsensitive);
    name.replace(QStringLiteral("&quot;"), QStringLiteral("\""), Qt::CaseInsensitive);
    name.replace(QChar(0x2013), QChar('-'));
    name.replace(QChar(0x2014), QChar('-'));
    return name.trimmed();
}

QString RenoDxTitleMatcher::subtitleBaseName(const QString &rawName) {
    QString name = cleanDisplayTitle(rawName);
    static const QRegularExpression subtitleBreak(R"(\s*(?::|\s-\s)\s*)");
    const auto match = subtitleBreak.match(name);
    if (match.hasMatch() && match.capturedStart() > 0)
        name = name.left(match.capturedStart());
    return name.trimmed();
}

QString RenoDxTitleMatcher::normalizeGameName(const QString &rawName) {
    QString name = cleanDisplayTitle(rawName);
    name.replace(QChar(0x2018), QChar('\''));
    name.replace(QChar(0x2019), QChar('\''));
    name.replace(QChar(0x201C), QChar('"'));
    name.replace(QChar(0x201D), QChar('"'));
    name.remove(QChar(0x00AE));
    name.remove(QChar(0x2122));
    name.remove(QChar(0x00A9));
    name = name.normalized(QString::NormalizationForm_KD).toLower();

    QString key;
    key.reserve(name.size());
    for (const QChar ch : name) {
        if (ch.isLetterOrNumber())
            key.append(ch);
    }
    return key;
}

RenoDxTitleMatch RenoDxTitleMatcher::resolve(const QString &gameName, const QVector<RenoDxCatalogEntry> &entries) {
    RenoDxTitleMatch result;
    const QString fullKey = normalizeGameName(gameName);
    if (fullKey.isEmpty())
        return result;

    for (const auto &entry : entries) {
        if (normalizeGameName(entry.title) == fullKey) {
            result.title = cleanDisplayTitle(entry.title);
            result.url = entry.url;
            result.method = RenoDxTitleMatch::Method::ExactTitle;
            return result;
        }
    }

    const QString baseKey = normalizeGameName(subtitleBaseName(gameName));
    if (!baseKey.isEmpty() && baseKey != fullKey) {
        for (const auto &entry : entries) {
            if (normalizeGameName(entry.title) == baseKey) {
                result.title = cleanDisplayTitle(entry.title);
                result.url = entry.url;
                result.method = RenoDxTitleMatch::Method::SubtitleExact;
                return result;
            }
        }
    }

    int bestScore = -1;
    int bestIndex = -1;
    bool ambiguous = false;
    for (int i = 0; i < entries.size(); ++i) {
        const QString catalogKey = normalizeGameName(entries.at(i).title);
        if (fullKey.size() < 8 || catalogKey.size() < 8)
            continue;
        if (!fullKey.startsWith(catalogKey) && !catalogKey.startsWith(fullKey))
            continue;

        const int score = qMin(fullKey.size(), catalogKey.size());
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
            ambiguous = false;
        } else if (score == bestScore && bestIndex >= 0 && entries.at(bestIndex).url != entries.at(i).url) {
            ambiguous = true;
        }
    }

    if (bestIndex >= 0 && !ambiguous) {
        result.title = cleanDisplayTitle(entries.at(bestIndex).title);
        result.url = entries.at(bestIndex).url;
        result.method = RenoDxTitleMatch::Method::Prefix;
    }
    return result;
}

QString RenoDxTitleMatcher::methodDisplayName(RenoDxTitleMatch::Method method) {
    switch (method) {
    case RenoDxTitleMatch::Method::ExactTitle:
        return QStringLiteral("Exact match");
    case RenoDxTitleMatch::Method::SubtitleExact:
        return QStringLiteral("Partial match");
    case RenoDxTitleMatch::Method::Prefix:
        return QStringLiteral("Close title match");
    case RenoDxTitleMatch::Method::None:
    default:
        return QStringLiteral("No match");
    }
}
