#include "RenoDxUpdatePolicy.h"

namespace {
QString targetIdentityFor(const RenoDxUpdateTarget &current, const QString &desiredUrl) {
    if (current.catalogMatched && !current.sourceKey.isEmpty() && !current.sourceRevision.isEmpty())
        return current.sourceKey + QStringLiteral("@") + current.sourceRevision;
    if (current.catalogMatched && !current.sourceKey.isEmpty())
        return current.sourceKey;
    return desiredUrl;
}
}

RenoDxUpdateDecision RenoDxUpdatePolicy::evaluate(bool installed,
                                                   bool managed,
                                                   bool external,
                                                   const QString &installedUrl,
                                                   const QString &installedSourceKey,
                                                   const QString &installedSourceRevision,
                                                   const RenoDxUpdateTarget &current,
                                                   const QString &fallbackDesiredUrl) {
    RenoDxUpdateDecision decision;
    if (!installed)
        return decision;

    if (!current.catalogMatched &&
        (!installedSourceKey.isEmpty() ||
         (!installedUrl.isEmpty() && !fallbackDesiredUrl.isEmpty() && installedUrl != fallbackDesiredUrl))) {
        decision.catalogMissing = true;
        return decision;
    }

    const QString desiredUrl = current.catalogMatched ? current.url : fallbackDesiredUrl;
    decision.targetIdentity = targetIdentityFor(current, desiredUrl);

    if (desiredUrl.isEmpty()) {
        decision.catalogMissing = true;
        return decision;
    }

    if (external) {
        decision.takeoverSuggested = current.catalogMatched;
        return decision;
    }

    if (!managed)
        return decision;

    const bool urlChanged = !installedUrl.isEmpty() && installedUrl != desiredUrl;
    const bool sourceKeyChanged = current.catalogMatched &&
                                  !installedSourceKey.isEmpty() &&
                                  !current.sourceKey.isEmpty() &&
                                  installedSourceKey != current.sourceKey;
    const bool revisionComparable = current.catalogMatched &&
                                    !installedSourceRevision.isEmpty() &&
                                    !current.sourceRevision.isEmpty();
    const bool revisionChanged = revisionComparable &&
                                 installedSourceRevision != current.sourceRevision;

    if (current.catalogMatched && current.requiresConfirmation) {
        decision.reviewRequired = true;
        decision.updateDetected = urlChanged || sourceKeyChanged || revisionChanged;
        decision.comparisonUnknown = !decision.updateDetected &&
                                     !current.sourceRevision.isEmpty() &&
                                     installedSourceRevision.isEmpty();
        return decision;
    }

    if (current.catalogMatched && current.exact &&
        !current.sourceRevision.isEmpty() && installedSourceRevision.isEmpty()) {
        decision.legacyRefreshRequired = true;
        decision.updateDetected = true;
        decision.updateAvailable = true;
        return decision;
    }

    if (current.catalogMatched && current.exact &&
        ((!current.sourceKey.isEmpty() && current.sourceRevision.isEmpty()) ||
         (current.sourceKey.isEmpty() && !urlChanged))) {
        decision.comparisonUnknown = true;
    }

    decision.updateDetected = urlChanged || sourceKeyChanged || revisionChanged;
    decision.updateAvailable = decision.updateDetected;
    return decision;
}
