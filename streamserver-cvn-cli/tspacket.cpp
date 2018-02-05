#include "tspacket.h"

#include <stdexcept>
#include <QDebug>

extern int verbose;

TSPacket::TSPacket(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (!_bytes.startsWith(syncByte)) {
        if (verbose >= 1)
            qInfo() << "TS packet: Does not start with sync byte" << syncByte << "but" << _bytes.left(4);

        throw std::runtime_error("TS packet: Does not start with sync byte");
    }
}

const QByteArray &TSPacket::bytes() const
{
    return _bytes;
}
