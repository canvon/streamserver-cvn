#ifndef HTTPHEADER_H
#define HTTPHEADER_H

#include <memory>
#include <QByteArray>
#include <QList>

namespace HTTP {

namespace impl {
class HeaderParserImpl;
}

class HeaderParser
{
    std::unique_ptr<impl::HeaderParserImpl>  _implPtr;

public:
    struct Field {
        QByteArray  bytes;
        QByteArray  fieldName;
        QByteArray  fieldValueRaw;
        QByteArray  fieldValue;
    };

    explicit HeaderParser();
    ~HeaderParser();

    static const QByteArray fieldSep;

    QList<Field> fields() const;
    QList<Field> fields(const QByteArray &fieldName) const;
    void setField(const QByteArray &fieldName, const QByteArray &fieldValue);

    void append(const QByteArray &fieldBytes);

    // Reverse direction
    //QByteArray toBytes() const;
};

}  // namespace HTTP

#endif // HTTPHEADER_H
