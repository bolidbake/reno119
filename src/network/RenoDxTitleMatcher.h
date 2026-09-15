#pragma once

#include <QString>
#include <QVector>

struct RenoDxCatalogEntry {
    QString title;
    QString url;
};

struct RenoDxTitleMatch {
    enum class Method {
        None,
        ExactTitle,
        SubtitleExact,
        Prefix
    };

    QString title;
    QString url;
    Method method = Method::None;

    bool matched() const { return !url.isEmpty() && method != Method::None; }
    bool exact() const { return method == Method::ExactTitle; }
    bool requiresConfirmation() const {
        return method == Method::SubtitleExact || method == Method::Prefix;
    }
};

class RenoDxTitleMatcher {
public:
    static QString normalizeGameName(const QString &rawName);
    static QString subtitleBaseName(const QString &rawName);
    static QString cleanDisplayTitle(const QString &rawName);
    static RenoDxTitleMatch resolve(const QString &gameName, const QVector<RenoDxCatalogEntry> &entries);
    static QString methodDisplayName(RenoDxTitleMatch::Method method);
};
