#include "tspacket.h"

#include <stdexcept>
#include <QDebug>

TSPacket::TSPacket(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (!_bytes.startsWith(syncByte)) {
        qDebug() << "TS packet: Does not start with sync byte" << syncByte << "but" << _bytes.left(4);

        throw std::runtime_error("TS packet: Does not start with sync byte");
    }
}

const QByteArray &TSPacket::bytes() const
{
    return _bytes;
}
