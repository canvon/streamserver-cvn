#ifndef HUMANREADABLE_H
#define HUMANREADABLE_H

#include <QString>

class HumanReadable
{
public:
    HumanReadable() = delete;

    static QString byteCount(quint64 count, bool base1000 = true, bool base1024 = true);
};

#endif // HUMANREADABLE_H
