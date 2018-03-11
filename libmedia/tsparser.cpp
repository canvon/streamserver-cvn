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

int BitStream::bitsLeft() const
{
    return _implPtr->_bitsLeft;
}

bool BitStream::atEnd() const
{
    if (_implPtr->_bitsLeft > 0)
        return false;

    if (_implPtr->_offsetBytes + 1 < _implPtr->_bytes.length())
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
        throw std::runtime_error("TS parser: Not byte-aligned");

    quint8 byte = impl._curByte;
    impl._bitsLeft -= 8;

    return byte;
}

}  // namespace TS
