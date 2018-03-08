#ifndef HUMANREADABLE_H
#define HUMANREADABLE_H

#include "libinfra_global.h"

#include <QString>
#include <QByteArray>
#include <QDebug>

namespace HumanReadable {


    LIBINFRASHARED_EXPORT QString byteCount(quint64 count, bool base1000 = true, bool base1024 = true);

    LIBINFRASHARED_EXPORT QString timeDuration(qint64 msec, bool exact = true);


    struct LIBINFRASHARED_EXPORT Hexdump {
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

    LIBINFRASHARED_EXPORT QDebug operator<<(QDebug debug, const Hexdump &dump);


    struct LIBINFRASHARED_EXPORT FlagConverter {
        QStringList falseFlags = { "0", "false" };
        QStringList trueFlags  = { "1", "true" };

        bool flagToBool(const QVariant &flag, bool *ok = nullptr) const;
        QStringList flagPairs() const;
    };


    struct LIBINFRASHARED_EXPORT KeyValueOption {
        QString  buf;
        QString  fieldSep      = "=";
        QString  interFieldSep = ",";

        QString takeKey();
        QString takeValue();
        QString takeRest();
    };


}  // namespace HumanReadable

#endif // HUMANREADABLE_H
