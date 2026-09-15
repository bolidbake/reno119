#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

class PeParser {
public:
    struct Result {
        bool valid = false;
        QString architecture = "Unknown";
        QString graphicsApi = "Unknown";
        QStringList imports;
    };

    static Result inspect(const QString &path);
    static QString detectGraphicsApiFallback(const QString &path, QStringList *evidence = nullptr);

    struct Section {
        quint32 virtualAddress = 0;
        quint32 virtualSize = 0;
        quint32 rawAddress = 0;
        quint32 rawSize = 0;
    };
};
