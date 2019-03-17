#ifndef TSPRIMITIVE_H
#define TSPRIMITIVE_H

#include "exceptionbuilder.h"

#include <typeinfo>
#include <stdexcept>
#include <QByteArray>

namespace TS {


// A source of bits.

class BitStream
{
    QByteArray  _bytes;
    bool        _isDirty     = false;
    quint8      _curByte     = 0;
    int         _offsetBytes = -1;
    int         _bitsLeft    = 0;

public:
    BitStream(const QByteArray &bytes) : _bytes(bytes) { }

    const QByteArray &bytes()       { if (_isDirty) flush(); return _bytes; }
    const QByteArray &bytes() const { if (_isDirty) throw std::runtime_error("TS bit stream: Caller forgot to call flush"); return _bytes; }
    int offsetBytes() const         { return _offsetBytes; }
    int bytesLeft() const           { return _bytes.length() - (_offsetBytes + 1); }
    int bitsLeft() const            { return _bitsLeft; }
    bool isByteAligned() const      { return _bitsLeft == 0 || _bitsLeft == 8; }

    bool atEnd() const
    {
        if (_bitsLeft > 0)
            return false;

        if (bytesLeft() > 0)
            return false;

        return true;
    }

    void flush()
    {
        if (!_isDirty)
            return;

        if (!(0 <= _offsetBytes && _offsetBytes < _bytes.length()))
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "TS bit stream: Can't flush dirty byte: Offset" << _offsetBytes << "out of range");

        _bytes[_offsetBytes] = _curByte;
        _isDirty = false;
        _curByte = 0;
    }

private:
    void _nextByte()
    {
        if (_isDirty) {
            // Put back previous value. It seems to have been changed...
            flush();
        }

        if (!(++_offsetBytes < _bytes.length()))
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "TS bit stream: Input/output bytes exceeded"
                << "at offset" << _offsetBytes);

        _curByte = _bytes.at(_offsetBytes);
        _bitsLeft = 8;
    }

public:
    bool takeBit()
    {
        if (!(_bitsLeft > 0))
            _nextByte();

        return (_curByte >> --_bitsLeft) & 0x01;
    }

    void putBit(bool value)
    {
        if (!(_bitsLeft > 0))
            _nextByte();

        const quint8 mask = 0x01 << --_bitsLeft;
        if (value)
            // Set bit.
            _curByte |= mask;
        else
            // Clear bit.
            _curByte &= ~mask;

        _isDirty = true;
    }

    quint8 takeByteAligned()
    {
        if (!(_bitsLeft > 0))
            _nextByte();

        if (_bitsLeft != 8)
            throw std::runtime_error("TS bit stream: Not byte-aligned for take byte");

        quint8 byte = _curByte;
        _bitsLeft -= 8;

        return byte;
    }

    QByteArray takeByteArrayAligned(int bytesCount)
    {
        if (!(_bitsLeft == 0))
            throw std::runtime_error("TS bit stream: Not byte-aligned for take byte array");

        if (bytesCount < 0) {
            QByteArray ret = _bytes.mid(_offsetBytes + 1);
            _offsetBytes = _bytes.length() - 1;
            return ret;
        }

        if (!(bytesLeft() >= bytesCount))
            throw std::runtime_error("TS bit stream: Not enough input bytes available");

        QByteArray ret = _bytes.mid(_offsetBytes + 1, bytesCount);
        if (ret.length() != bytesCount)
            throw std::runtime_error("TS bit stream: Internal error: Check against taking too many bytes failed");

        _offsetBytes += bytesCount;

        return ret;
    }
};


// Store finite number of bits:
// bslbf is MPEG-TS mnemonic for "bit string, left bit first".

template <size_t StreamBitSize, typename WorkingType>
struct bslbf_base {
    static constexpr size_t stream_bit_size = StreamBitSize;

    using working_type = WorkingType;
    static constexpr size_t working_bit_size = 8u * sizeof(working_type);

    working_type  value;

    static_assert(1 <= stream_bit_size, "TS bslbf: Stream bit size must at least be 1");
    static_assert(stream_bit_size <= working_bit_size, "TS bslbf: Working type not large enough");
};

template <size_t StreamBitSize, typename WorkingType>
struct bslbf : public bslbf_base<StreamBitSize, WorkingType> {
    using base = bslbf_base<StreamBitSize, WorkingType>;
    bslbf() : base() { }
    bslbf(const bslbf &other) : base(other) { }
    bslbf(WorkingType value) : base { value } { }
};

// Specialization for single-bit (e.g., bit flag):
// Allow easy access to value.
template<>
struct bslbf<1, bool> : public bslbf_base<1, bool> {
    using base = bslbf_base;
    bslbf() : base() { }
    bslbf(const bslbf &other) : base(other) { }
    bslbf(bool value) : base { value } { }

    operator bool() const
    {
        return value;
    }
};

using bslbf1 = bslbf<1, bool>;    // Single-bit (e.g., bit flag).
using bslbf8 = bslbf<8, quint8>;  // 8 bits, aka a byte.


// Store unsigned integer:
// uimsbf is MPEG-TS mnemonic for "unsigned integer, most significant bit first".
//
// This is just like bslbf in implementation, just without specialization for bit flag.

template <size_t StreamBitSize, typename WorkingType>
struct uimsbf {
    static constexpr size_t stream_bit_size = StreamBitSize;

    using working_type = WorkingType;
    static constexpr size_t working_bit_size = 8u * sizeof(working_type);

    working_type  value;

    static_assert(1 <= stream_bit_size, "TS uimsbf: Stream bit size must at least be 1");
    static_assert(stream_bit_size <= working_bit_size, "TS uimsbf: Working type not large enough");
};


// Store signed integer:
// tcimsbf is MPEG-TS mnemonic for "two's complement integer, msb (sign) bit first".

template <size_t StreamBitSize, typename WorkingType>
struct tcimsbf {
    static constexpr size_t stream_bit_size = StreamBitSize;

    using working_type = WorkingType;
    static constexpr size_t working_bit_size = 8u * sizeof(working_type);

    working_type  value;

    static_assert(2 <= stream_bit_size, "TS tcimsbf: Stream bit size must at least be 2");  // Needs one bit more for sign.
    static_assert(stream_bit_size <= working_bit_size, "TS tcimsbf: Working type not large enough");
};


// Extract & store bits from a bit source.

namespace impl {

template <typename Dest, typename Source>
inline void assignMaybeCast(Dest &dst, Source &src) { dst = static_cast<Dest>(src); }

template <typename Dest, typename Source, typename = std::enable_if_t<typeid(Dest) == typeid(Source)>>
inline void assignMaybeCast(Dest &dst, Source &src) { dst = src; }

template <typename T,
          typename Tmp = typename T::working_type,
          bool SignExtend = std::numeric_limits<Tmp>::is_signed>
inline BitStream &doInputFromBitStream(BitStream &bitSource, T &outT)
{
    Tmp tmp = 0;

    bool aligned = bitSource.isByteAligned();
    if (T::stream_bit_size == 8u && aligned) {
        tmp = bitSource.takeByteAligned();
    }
    else if (T::stream_bit_size == 16u && aligned) {
        tmp = (static_cast<quint16>(bitSource.takeByteAligned()) << 8) |
               static_cast<quint16>(bitSource.takeByteAligned());
    }
    else {
        // General case.
        for (size_t bitsLeft = T::stream_bit_size; bitsLeft > 0; bitsLeft--) {
            if (SignExtend && bitsLeft == T::stream_bit_size) {
                // Do sign extension.
                tmp = bitSource.takeBit() ? -1 : 0;
                continue;
            }

            if (bitsLeft >= 8 && bitSource.isByteAligned()) {
                // Temporarily switch to byte-wise mode.
                tmp = (tmp << 8) | bitSource.takeByteAligned();
                // This means we'll have to adjust the loop variable manually
                // by so many extra bits...
                bitsLeft -= 7;
            }
            else {
                tmp = (tmp << 1) | (bitSource.takeBit() ? 1 : 0);
            }
        }
    }

    assignMaybeCast(outT.value, tmp);
    return bitSource;
}

}  // namespace impl

template <size_t StreamBitSize, typename WorkingType, typename = std::enable_if_t<StreamBitSize <= 8>>
inline BitStream &operator>>(BitStream &bitSource, bslbf<StreamBitSize, WorkingType> &outBSLBF)
{
    return impl::doInputFromBitStream<bslbf<StreamBitSize, WorkingType>, quint8, false>(bitSource, outBSLBF);
}

// Specialization for single-bit (e.g., bit flag).
template <>
inline BitStream &operator>> <1, bool>(BitStream &bitSource, bslbf1 &outBSLBF)
{
    outBSLBF.value = bitSource.takeBit();
    return bitSource;
}

// uimsbf is like bslbf, just without specialization for <1, bool>,
// and without need for a cast from tmp as we'll work in the result type, directly.
template <size_t StreamBitSize, typename WorkingType>
inline BitStream &operator>>(BitStream &bitSource, uimsbf<StreamBitSize, WorkingType> &outUIMSBF)
{
    return impl::doInputFromBitStream<uimsbf<StreamBitSize, WorkingType>, WorkingType, false>(bitSource, outUIMSBF);
}

// tcimsbf is like uimsbf, but interprets first (sign) bit specially.
template <size_t StreamBitSize, typename WorkingType>
inline BitStream &operator>>(BitStream &bitSource, tcimsbf<StreamBitSize, WorkingType> &outTCIMSBF)
{
    return impl::doInputFromBitStream<tcimsbf<StreamBitSize, WorkingType>, WorkingType, true>(bitSource, outTCIMSBF);
}


// Compose source bits to a bit sink.

// bslbf.
template <size_t StreamBitSize, typename WorkingType>
inline BitStream &operator<<(BitStream &bitSink, const bslbf<StreamBitSize, WorkingType> &inBSLBF)
{
    for (WorkingType mask = 1u << (StreamBitSize - 1); mask; mask >>= 1) {
        bitSink.putBit(inBSLBF.value & mask);
    }
    return bitSink;
}

// bslbf: Specialization for single-bit (e.g., bit flag).
template <>
inline BitStream &operator<< <1, bool>(BitStream &bitSink, const bslbf1 &inBSLBF)
{
    bitSink.putBit(inBSLBF.value);
    return bitSink;
}

// uimsbf.
template <size_t StreamBitSize, typename WorkingType>
inline BitStream &operator<<(BitStream &bitSink, const uimsbf<StreamBitSize, WorkingType> &inUIMSBF)
{
    for (size_t workingBitsLeft = inUIMSBF.working_bit_size; workingBitsLeft > 0; --workingBitsLeft) {
        const size_t workingBitIndex = workingBitsLeft - 1;
        const WorkingType mask = 1u << workingBitIndex;
        const bool bit = inUIMSBF.value & mask;

        if (workingBitsLeft > inUIMSBF.stream_bit_size) {
            if (bit) {
                QString errmsg;
                QDebug(&errmsg).nospace() << "TS bit stream: uimsbf<" << StreamBitSize << "> to bit sink: "
                    << "Invalid bit set at bit " << workingBitIndex << "; "
                    << "value " << inUIMSBF.value << " out of range!";
                throw std::runtime_error(errmsg.toStdString());
            }
        }
        else {
            bitSink.putBit(bit);
        }
    }

    return bitSink;
}

// tcimsbf: Treat sign bit specially, check for proper sign extension.
template <size_t StreamBitSize, typename WorkingType>
inline BitStream &operator<<(BitStream &bitSink, const tcimsbf<StreamBitSize, WorkingType> &inTCIMSBF)
{
    bool signBit;
    for (size_t workingBitsLeft = inTCIMSBF.working_bit_size; workingBitsLeft > 0; --workingBitsLeft) {
        const size_t workingBitIndex = workingBitsLeft - 1;
        const WorkingType mask = 1u << workingBitIndex;
        const bool bit = inTCIMSBF.value & mask;

        if (workingBitsLeft == inTCIMSBF.working_bit_size) {
            signBit = bit;
            bitSink.putBit(bit);
        }
        else if (workingBitsLeft > inTCIMSBF.stream_bit_size /* sign bit: */ - 1) {
            if (bit != signBit) {
                QString errmsg;
                QDebug(&errmsg).nospace() << "TS bit stream: tcimsbf<" << StreamBitSize << "> to bit sink: "
                    << "No proper sign extension at bit " << workingBitIndex << "; "
                    << "value " << inTCIMSBF.value << " out of range!";
                throw std::runtime_error(errmsg.toStdString());
            }
        }
        else {
            bitSink.putBit(bit);
        }
    }

    return bitSink;
}


}  // namespace TS

#endif // TSPRIMITIVE_H
