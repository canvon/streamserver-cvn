#ifndef TSPARSER_H
#define TSPARSER_H

#include <memory>
#include <QByteArray>

namespace TS {

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

}  // namespace TS

#endif // TSPARSER_H
