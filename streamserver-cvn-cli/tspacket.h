#ifndef TSPACKET_H
#define TSPACKET_H

#include <QByteArray>

class TSPacket
{
    QByteArray  _bytes;

public:
    explicit TSPacket(const QByteArray &bytes);

    static const char syncByte = '\x47';
};

#endif // TSPACKET_H
