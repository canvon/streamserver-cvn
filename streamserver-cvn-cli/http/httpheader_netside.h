#ifndef HTTPHEADER_NETSIDE_H
#define HTTPHEADER_NETSIDE_H

#include <memory>
#include <QByteArray>
#include <QList>
#include <QDebug>

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
    QList<QByteArray> fieldValues(const QByteArray &fieldName) const;
    void setField(const QByteArray &fieldName, const QByteArray &fieldValue);

    void append(const QByteArray &fieldBytes);

    // Reverse direction
    //QByteArray toBytes() const;
};

QDebug operator<<(QDebug debug, const HeaderParser::Field &field);

}  // namespace HTTP

#endif // HTTPHEADER_NETSIDE_H
