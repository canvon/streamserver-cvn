#include "tsparser.h"

#include <stdexcept>

namespace TS {

namespace impl {
class BitStreamImpl {
    QByteArray  _bytes;
    quint8      _curByte     = 0;
    int         _offsetBytes = -1;
    int         _bitsLeft    = 0;

public:
    BitStreamImpl(const QByteArray &bytes) : _bytes(bytes)
    {

    }

private:
    void _nextByte();

    friend BitStream;
};
}

BitStream::BitStream(const QByteArray &bytes) :
    _implPtr(std::make_unique<impl::BitStreamImpl>(bytes))
{

}

BitStream::~BitStream()
{

}

const QByteArray &BitStream::bytes() const
{
    return _implPtr->_bytes;
}

int BitStream::offsetBytes() const
{
    return _implPtr->_offsetBytes;
}

int BitStream::bytesLeft() const
{
    return _implPtr->_bytes.length() - (_implPtr->_offsetBytes + 1);
}

int BitStream::bitsLeft() const
{
    return _implPtr->_bitsLeft;
}

bool BitStream::atEnd() const
{
    if (bitsLeft() > 0)
        return false;

    if (bytesLeft() > 0)
        return false;

    return true;
}

void impl::BitStreamImpl::_nextByte()
{
    if (!(++_offsetBytes < _bytes.length()))
        throw std::runtime_error("TS parser: Input bytes exceeded");

    _curByte = _bytes.at(_offsetBytes);
    _bitsLeft = 8;
}

bool BitStream::takeBit()
{
    if (!_implPtr)
        throw std::runtime_error("TS parser: Internal error: Implementation data missing");
    impl::BitStreamImpl &impl(*_implPtr);

    if (!(impl._bitsLeft > 0))
        impl._nextByte();

    return (impl._curByte >> --impl._bitsLeft) & 0x01;
}

quint8 BitStream::takeByteAligned()
{
    if (!_implPtr)
        throw std::runtime_error("TS parser: Internal error: Implementation data missing");
    impl::BitStreamImpl &impl(*_implPtr);

    if (!(impl._bitsLeft > 0))
        impl._nextByte();

    if (impl._bitsLeft != 8)
        throw std::runtime_error("TS parser: Not byte-aligned for take byte");

    quint8 byte = impl._curByte;
    impl._bitsLeft -= 8;

    return byte;
}

QByteArray BitStream::takeByteArrayAligned(int bytesCount)
{
    if (!_implPtr)
        throw std::runtime_error("TS parser: Internal error: Implementation data missing");
    impl::BitStreamImpl &impl(*_implPtr);

    if (!(impl._bitsLeft == 0))
        throw std::runtime_error("TS parser: Not byte-aligned for take byte array");

    if (bytesCount < 0) {
        QByteArray ret = impl._bytes.mid(impl._offsetBytes + 1);
        impl._offsetBytes = impl._bytes.length() - 1;
        return ret;
    }

    if (!(bytesLeft() >= bytesCount))
        throw std::runtime_error("TS parser: Not enough input bytes available");

    QByteArray ret = impl._bytes.mid(impl._offsetBytes + 1, bytesCount);
    if (ret.length() != bytesCount)
        throw std::runtime_error("TS parser: Internal error: Check against taking too many bytes failed");

    impl._offsetBytes += bytesCount;

    return ret;
}

}  // namespace TS
