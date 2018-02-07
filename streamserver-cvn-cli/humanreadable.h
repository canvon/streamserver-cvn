#ifndef HUMANREADABLE_H
#define HUMANREADABLE_H

#include <QString>
#include <QByteArray>
#include <QDebug>

class HumanReadable
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

        Hexdump &enableAll();
    };
};

QDebug operator<<(QDebug debug, const HumanReadable::Hexdump &dump);

#endif // HUMANREADABLE_H
