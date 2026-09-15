#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace Reno119::InstallerInternal {
QJsonObject tweakProfileSettings(const QVariantMap &plan);
QByteArray mergedTweakIni(const QString &text, const QVariantList &changes);
QJsonObject tweakFileState(const QString &path);
QString virtualKeyName(int code);
int parseVirtualKey(const QString &raw, int fallback = 0x2D);
bool writeSimpleKeyValue(const QString &path, const QString &key, const QString &value);
QString findExistingReFrameworkConfig(const QString &exeDir, QString *rawMenuKey = nullptr, bool *legacyMenuKey = nullptr);
QString backupComponentForReason(const QString &reason);
}
