#ifndef TSPARSER_H
#define TSPARSER_H

#include <memory>
#include <stdexcept>
#include <QByteArray>

namespace TS {


// A source of bits.

namespace impl {
class BitStreamImpl;
}

class BitStream
{
    std::unique_ptr<impl::BitStreamImpl>  _implPtr;

public:
    explicit BitStream(const QByteArray &bytes);
    ~BitStream();

    const QByteArray &bytes() const;
    int offsetBytes() const;
    int bytesLeft() const;
    int bitsLeft() const;
    bool isByteAligned() const;
    bool atEnd() const;

    bool   takeBit();
    quint8 takeByteAligned();
    QByteArray takeByteArrayAligned(int bytesCount);
};


// Store finite number of bits:
// bslbf is MPEG-TS mnemonic for "bit string, left bit first".

template <int Bits, typename R>
struct bslbf_base {
    using type = R;
    static constexpr int bit_size = Bits;

    R  value;

    static_assert(1 <= Bits, "TS bslbf: Bits must at least be 1");
    static_assert(Bits <= 8 * sizeof(R), "TS bslbf: Result type not large enough");
};

template <int Bits, typename R>
struct bslbf : public bslbf_base<Bits, R> {
    using base = bslbf_base<Bits, R>;
    bslbf() : base() { }
    bslbf(const bslbf &other) : base(other) { }
    bslbf(R value) : base { value } { }
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

template <int Bits, typename R>
struct uimsbf {
    using type = R;
    static constexpr int bit_size = Bits;

    R  value;

    static_assert(1 <= Bits, "TS uimsbf: Bits must at least be 1");
    static_assert(Bits <= 8 * sizeof(R), "TS uimsbf: Result type not large enough");
};


// Store signed integer:
// tcimsbf is MPEG-TS mnemonic for "two's complement integer, msb (sign) bit first".

template <int Bits, typename R>
struct tcimsbf {
    using type = R;
    static constexpr int bit_size = Bits;

    R  value;

    static_assert(2 <= Bits, "TS tcimsbf: Bits must at least be 2");  // Needs one bit more for sign.
    static_assert(Bits <= 8 * sizeof(R), "TS tcimsbf: Result type not large enough");
};


// Extract & store bits from a bit source.

namespace impl {

template <typename Dest, typename Source>
inline void assignMaybeCast(Dest &dst, Source &src) { dst = static_cast<Dest>(src); }

template <typename Dest, typename Source, typename = std::enable_if_t<typeid(Dest) == typeid(Source)>>
inline void assignMaybeCast(Dest &dst, Source &src) { dst = src; }

template <typename T,
          typename Tmp = typename T::type,
          bool SignExtend = std::numeric_limits<Tmp>::is_signed>
inline BitStream &doInputFromBitStream(BitStream &bitSource, T &outT)
{
    Tmp tmp = 0;

    bool aligned = bitSource.isByteAligned();
    if (T::bit_size == 8 && aligned) {
        tmp = bitSource.takeByteAligned();
    }
    else if (T::bit_size == 16 && aligned) {
        tmp = (static_cast<quint16>(bitSource.takeByteAligned()) << 8) |
               static_cast<quint16>(bitSource.takeByteAligned());
    }
    else {
        // General case.
        for (int bitsLeft = T::bit_size; bitsLeft > 0; bitsLeft--) {
            if (SignExtend && bitsLeft == T::bit_size) {
                // Do sign extension.
                tmp = bitSource.takeBit() ? -1 : 0;
                continue;
            }

            tmp = (tmp << 1) | (bitSource.takeBit() ? 1 : 0);
        }
    }

    assignMaybeCast(outT.value, tmp);
    return bitSource;
}

}  // namespace impl

template <int Bits, typename R, typename = std::enable_if_t<Bits <= 8>>
inline BitStream &operator>>(BitStream &bitSource, bslbf<Bits, R> &outBSLBF)
{
    return impl::doInputFromBitStream<bslbf<Bits, R>, quint8, false>(bitSource, outBSLBF);
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
template <int Bits, typename R>
inline BitStream &operator>>(BitStream &bitSource, uimsbf<Bits, R> &outUIMSBF)
{
    return impl::doInputFromBitStream<uimsbf<Bits, R>, R, false>(bitSource, outUIMSBF);
}

// tcimsbf is like uimsbf, but interprets first (sign) bit specially.
template <int Bits, typename R>
inline BitStream &operator>>(BitStream &bitSource, tcimsbf<Bits, R> &outTCIMSBF)
{
    return impl::doInputFromBitStream<tcimsbf<Bits, R>, R, true>(bitSource, outTCIMSBF);
}


}  // namespace TS

#endif // TSPARSER_H
