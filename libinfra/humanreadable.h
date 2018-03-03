#ifndef HUMANREADABLE_H
#define HUMANREADABLE_H

#include "libinfra_global.h"

#include <QString>
#include <QByteArray>
#include <QDebug>

class LIBINFRASHARED_EXPORT HumanReadable
{
public:
    HumanReadable() = delete;

    static QString byteCount(quint64 count, bool base1000 = true, bool base1024 = true);
    static QString timeDuration(qint64 msec, bool exact = true);

    struct Hexdump {
        const QByteArray &data;
        bool hex       = true;
        bool ascii     = false;
        bool byteCount = false;
        bool compressAllOneBits  = true;
        bool compressAllZeroBits = true;
        bool compressTrailing    = false;

        Hexdump &enableByteCount();
        Hexdump &enableCompressTrailing();
        Hexdump &enableAll();
    };

    struct FlagConverter {
        QStringList falseFlags = { "0", "false" };
        QStringList trueFlags  = { "1", "true" };

        bool flagToBool(const QVariant &flag, bool *ok = nullptr) const;
    };
};

LIBINFRASHARED_EXPORT QDebug operator<<(QDebug debug, const HumanReadable::Hexdump &dump);

#endif // HUMANREADABLE_H
