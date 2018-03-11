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
    int bitsLeft() const;
    bool atEnd() const;

    bool   takeBit();
    quint8 takeByteAligned();
};


// Store finite number of bits:
// bslbf is MPEG-TS mnemonic for "bit string, left bit first".

template <int Bits, typename R>
struct bslbf_base {
    using type = R;
    R  value;
    static_assert(1 <= Bits, "TS bslbf: Bits must at least be 1");
    static_assert(Bits <= 8 * sizeof(R), "TS bslbf: Result type not large enough");
};

template <int Bits, typename R>
struct bslbf : public bslbf_base<Bits, R> { };

// Specialization for single-bit (e.g., bit flag):
// Allow easy access to value.
template<>
struct bslbf<1, bool> : bslbf_base<1, bool> {
    operator bool()
    {
        return value;
    }
};

using bslbf1 = bslbf<1, bool>;    // Single-bit (e.g., bit flag).
using bslbf8 = bslbf<8, quint8>;  // 8 bits, aka a byte.


// Extract & store bits from a bit source.

template <int Bits, typename R, typename = std::enable_if_t<Bits <= 8>>
BitStream &operator>>(BitStream &bitSource, bslbf<Bits, R> &outBSLBF)
{
    quint8 tmp = 0;
    for (int bitsLeft = Bits; bitsLeft > 0; bitsLeft--) {
        tmp = (tmp << 1) | (bitSource.takeBit() ? 1 : 0);
    }
    outBSLBF.value = static_cast<R>(tmp);
    return bitSource;
}

// Specialization for single-bit (e.g., bit flag).
template <>
BitStream &operator>> <1, bool>(BitStream &bitSource, bslbf1 &outBSLBF)
{
    outBSLBF.value = bitSource.takeBit();
    return bitSource;
}


}  // namespace TS

#endif // TSPARSER_H
