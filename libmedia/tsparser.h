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

template <int Bits, typename R>
BitStream &operator>>(BitStream &bits, bslbf<Bits, R> &outBSLBF)
{
    throw std::runtime_error("TS bit stream output to bslbf: General case not implemented");
}

// Specialization for single-bit (e.g., bit flag).
template <>
BitStream &operator>> <1, bool>(BitStream &bits, bslbf1 &outBSLBF)
{
    outBSLBF.value = bits.takeBit();
    return bits;
}


}  // namespace TS

#endif // TSPARSER_H
