#ifndef HTTPHEADER_NETSIDE_H
#define HTTPHEADER_NETSIDE_H

#include <memory>
#include <QByteArray>
#include <QList>
#include <QDebug>

namespace SSCvn {
namespace HTTP {  // namespace SSCvn::HTTP

namespace impl {
class HeaderNetsideImpl;
}

class HeaderNetside
{
    std::unique_ptr<impl::HeaderNetsideImpl>  _implPtr;

public:
    struct Field {
        QByteArray  bytes;
        QByteArray  fieldName;
        QByteArray  fieldValueRaw;
        QByteArray  fieldValue;
    };

    explicit HeaderNetside();
    ~HeaderNetside();

    QList<Field> fields() const;
    QList<Field> fields(const QByteArray &fieldName) const;
    QList<QByteArray> fieldValues(const QByteArray &fieldName) const;
    void setField(const QByteArray &fieldName, const QByteArray &fieldValue);

    void append(const QByteArray &fieldBytes);

    // Reverse direction
    //QByteArray toBytes() const;
};

QDebug operator<<(QDebug debug, const HeaderNetside::Field &field);

}  // namespace SSCvn::HTTP
}  // namespace SSCvn

#endif // HTTPHEADER_NETSIDE_H
