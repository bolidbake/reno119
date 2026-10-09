#pragma once

#include <QString>

struct RenoDxUpdateTarget {
    QString url;
    QString sourceKey;
    QString sourceRevision;
    bool catalogMatched = false;
    bool exact = false;
    bool requiresConfirmation = false;
};

struct RenoDxUpdateDecision {
    bool updateDetected = false;
    bool updateAvailable = false;
    bool reviewRequired = false;
    bool takeoverSuggested = false;
    bool comparisonUnknown = false;
    bool legacyRefreshRequired = false;
    bool catalogMissing = false;
    QString targetIdentity;
};

class RenoDxUpdatePolicy {
public:
    static RenoDxUpdateDecision evaluate(bool installed,
                                         bool managed,
                                         bool external,
                                         const QString &installedUrl,
                                         const QString &installedSourceKey,
                                         const QString &installedSourceRevision,
                                         const RenoDxUpdateTarget &current,
                                         const QString &fallbackDesiredUrl = QString());
};
