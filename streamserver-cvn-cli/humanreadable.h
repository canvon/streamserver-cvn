#ifndef HUMANREADABLE_H
#define HUMANREADABLE_H

#include <QString>

class HumanReadable
{
public:
    HumanReadable() = delete;

    static QString byteCount(quint64 count, bool base1000 = true, bool base1024 = true);
    static QString timeDuration(qint64 msec, bool exact = true);
};

#endif // HUMANREADABLE_H
