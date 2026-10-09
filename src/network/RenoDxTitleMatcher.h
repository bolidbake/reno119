#pragma once

#include <QString>
#include <QVector>

struct RenoDxCatalogEntry {
    QString title;
    QString url;
    QString sourceOwner;
    QString sourceRepo;
    QString sourcePath;

    QString sourceKey() const {
        if (sourceOwner.isEmpty() || sourceRepo.isEmpty())
            return {};
        return sourceOwner + QStringLiteral("/") + sourceRepo +
               (sourcePath.isEmpty() ? QString() : QStringLiteral(":") + sourcePath);
    }
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
    QString sourceOwner;
    QString sourceRepo;
    QString sourcePath;
    Method method = Method::None;

    QString sourceKey() const {
        if (sourceOwner.isEmpty() || sourceRepo.isEmpty())
            return {};
        return sourceOwner + QStringLiteral("/") + sourceRepo +
               (sourcePath.isEmpty() ? QString() : QStringLiteral(":") + sourcePath);
    }

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
