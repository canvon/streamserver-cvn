#ifndef TSPARSER_H
#define TSPARSER_H

#include <memory>
#include <QByteArray>

namespace TS {

namespace impl {
class ParserImpl;
}

class Parser
{
    std::unique_ptr<impl::ParserImpl>  _implPtr;

public:
    explicit Parser(const QByteArray &bytes);
    ~Parser();

    const QByteArray &bytes() const;
    int offsetBytes() const;
    int bitsLeft() const;
    bool atEnd() const;

    bool   takeBit();
    quint8 takeByteAligned();
};

}  // namespace TS

#endif // TSPARSER_H
