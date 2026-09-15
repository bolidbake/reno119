#pragma once

#include "../core/GameInfo.h"
#include <QString>
#include <QVector>

class HeroicScanner {
public:
    static QVector<GameInfo> scan(const QString &additionalAppFolder = QString());
};
