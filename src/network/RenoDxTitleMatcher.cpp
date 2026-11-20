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
    static const QHash<QString, int> kEmptyIndex;
    return resolve(gameName, entries, kEmptyIndex);
}

RenoDxTitleMatch RenoDxTitleMatcher::resolve(const QString &gameName, const QVector<RenoDxCatalogEntry> &entries,
                                             const QHash<QString, int> &exactIndex) {
    RenoDxTitleMatch result;
    const QString fullKey = normalizeGameName(gameName);
    if (fullKey.isEmpty())
        return result;

    const auto fillResult = [&](const RenoDxCatalogEntry &entry, RenoDxTitleMatch::Method method) {
        result.title = cleanDisplayTitle(entry.title);
        result.url = entry.url;
        result.sourceOwner = entry.sourceOwner;
        result.sourceRepo = entry.sourceRepo;
        result.sourcePath = entry.sourcePath;
        result.method = method;
        return result;
    };

    if (!exactIndex.isEmpty()) {
        const auto exactIt = exactIndex.constFind(fullKey);
        if (exactIt != exactIndex.cend() && exactIt.value() >= 0 && exactIt.value() < entries.size())
            return fillResult(entries.at(exactIt.value()), RenoDxTitleMatch::Method::ExactTitle);

        const QString baseKey = normalizeGameName(subtitleBaseName(gameName));
        if (!baseKey.isEmpty() && baseKey != fullKey) {
            const auto baseIt = exactIndex.constFind(baseKey);
            if (baseIt != exactIndex.cend() && baseIt.value() >= 0 && baseIt.value() < entries.size())
                return fillResult(entries.at(baseIt.value()), RenoDxTitleMatch::Method::SubtitleExact);
        }
    }

    QVector<QString> computedKeys;
    const auto keyForIndex = [&](int i) -> const QString & {
        const auto &entry = entries.at(i);
        if (!entry.normalizedTitle.isEmpty())
            return entry.normalizedTitle;
        if (computedKeys.isEmpty())
            computedKeys.resize(entries.size());
        if (computedKeys[i].isEmpty())
            computedKeys[i] = normalizeGameName(entry.title);
        return computedKeys[i];
    };

    if (exactIndex.isEmpty()) {
        for (int i = 0; i < entries.size(); ++i) {
            if (keyForIndex(i) == fullKey)
                return fillResult(entries.at(i), RenoDxTitleMatch::Method::ExactTitle);
        }

        const QString baseKey = normalizeGameName(subtitleBaseName(gameName));
        if (!baseKey.isEmpty() && baseKey != fullKey) {
            for (int i = 0; i < entries.size(); ++i) {
                if (keyForIndex(i) == baseKey)
                    return fillResult(entries.at(i), RenoDxTitleMatch::Method::SubtitleExact);
            }
        }
    }

    if (fullKey.size() < 8)
        return result;

    int bestScore = -1;
    int bestIndex = -1;
    bool ambiguous = false;
    for (int i = 0; i < entries.size(); ++i) {
        const QString &catalogKey = keyForIndex(i);
        if (catalogKey.size() < 8)
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

    if (bestIndex >= 0 && !ambiguous)
        return fillResult(entries.at(bestIndex), RenoDxTitleMatch::Method::Prefix);
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
