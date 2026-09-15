#pragma once

#include "../core/GameInfo.h"
#include <QString>
#include <QVector>

class LutrisScanner {
public:
    static QVector<GameInfo> scan(const QString &additionalDataRoot = QString(),
                                  const QString &additionalConfigRoot = QString());
};
