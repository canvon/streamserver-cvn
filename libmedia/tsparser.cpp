#include "tsparser.h"

#include <stdexcept>

namespace TS {

namespace impl {
class ParserImpl {
    QByteArray  _bytes;
    quint8      _curByte     = 0;
    int         _offsetBytes = -1;
    int         _bitsLeft    = 0;

public:
    ParserImpl(const QByteArray &bytes) : _bytes(bytes)
    {

    }

private:
    void _nextByte();

    friend Parser;
};
}

Parser::Parser(const QByteArray &bytes) :
    _implPtr(std::make_unique<impl::ParserImpl>(bytes))
{

}

Parser::~Parser()
{

}

const QByteArray &Parser::bytes() const
{
    return _implPtr->_bytes;
}

int Parser::offsetBytes() const
{
    return _implPtr->_offsetBytes;
}

int Parser::bitsLeft() const
{
    return _implPtr->_bitsLeft;
}

bool Parser::atEnd() const
{
    if (_implPtr->_bitsLeft > 0)
        return false;

    if (_implPtr->_offsetBytes + 1 < _implPtr->_bytes.length())
        return false;

    return true;
}

void impl::ParserImpl::_nextByte()
{
    if (!(++_offsetBytes < _bytes.length()))
        throw std::runtime_error("TS parser: Input bytes exceeded");

    _curByte = _bytes.at(_offsetBytes);
    _bitsLeft = 8;
}

bool Parser::takeBit()
{
    if (!_implPtr)
        throw std::runtime_error("TS parser: Internal error: Implementation data missing");
    impl::ParserImpl &impl(*_implPtr);

    if (!(impl._bitsLeft > 0))
        impl._nextByte();

    return (impl._curByte >> --impl._bitsLeft) & 0x01;
}

quint8 Parser::takeByteAligned()
{
    if (!_implPtr)
        throw std::runtime_error("TS parser: Internal error: Implementation data missing");
    impl::ParserImpl &impl(*_implPtr);

    if (!(impl._bitsLeft > 0))
        impl._nextByte();

    if (impl._bitsLeft != 8)
        throw std::runtime_error("TS parser: Not byte-aligned");

    quint8 byte = impl._curByte;
    impl._bitsLeft -= 8;

    return byte;
}

}  // namespace TS
