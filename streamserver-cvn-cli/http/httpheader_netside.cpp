#include "httpheader_netside.h"

#include "httprequest_netside.h"
#include "humanreadable.h"

#include <stdexcept>
#include <QDebug>
#include <QMultiMap>

namespace HTTP {

const QByteArray HeaderNetside::fieldSep = ":";

namespace impl {

class HeaderNetsideImpl {
    QList<HeaderNetside::Field>  _fields;
    QMultiMap<QByteArray, int>  _fieldNameIndices;
    friend HeaderNetside;

public:
    void append(const HeaderNetside::Field &theField)
    {
        _fields.append(theField);
        _fieldNameIndices.insert(
            theField.fieldName.toLower(),
            _fields.length() - 1);
    }

    void regenerateIndices()
    {
        _fieldNameIndices.clear();
        for (int i = 0; i < _fields.length(); i++) {
            const HeaderNetside::Field &theField(_fields.at(i));
            _fieldNameIndices.insert(theField.fieldName.toLower(), i);
        }
    }
};

}  // namespace HTTP::impl

HeaderNetside::HeaderNetside() :
    _implPtr(std::make_unique<impl::HeaderNetsideImpl>())
{

}

HeaderNetside::~HeaderNetside()
{

}

QList<HeaderNetside::Field> HeaderNetside::fields() const
{
    return _implPtr->_fields;
}

QList<HeaderNetside::Field> HeaderNetside::fields(const QByteArray &fieldName) const
{
    QList<Field> ret;
    QList<int> indicesReverse = _implPtr->_fieldNameIndices.values(fieldName.toLower());
    for (auto i = indicesReverse.crbegin(); i != indicesReverse.crend(); i++)
        ret.append(_implPtr->_fields.at(*i));

    return ret;
}

QList<QByteArray> HeaderNetside::fieldValues(const QByteArray &fieldName) const
{
    QList<QByteArray> ret;
    for (const Field &theField : fields(fieldName))
        ret.append(theField.fieldValue);

    return ret;
}

void HeaderNetside::setField(const QByteArray &fieldName, const QByteArray &fieldValue)
{
    if (fieldName.isEmpty())
        throw std::invalid_argument("HTTP header netside: Set field: Field name can't be empty");

    Field theField;
    theField.fieldName = fieldName;
    theField.fieldValue = fieldValue;

    QList<int> indicesReverse = _implPtr->_fieldNameIndices.values(fieldName.toLower());
    if (indicesReverse.isEmpty()) {
        _implPtr->append(theField);
    }
    else {
        // Remove every occurence except first.
        while (indicesReverse.length() > 1)
            _implPtr->_fields.removeAt(indicesReverse.takeFirst());

        _implPtr->_fields.replace(indicesReverse.last(), theField);
        _implPtr->regenerateIndices();
    }
}

void HeaderNetside::append(const QByteArray &fieldBytes)
{
    int i = fieldBytes.indexOf(fieldSep);
    if (!(i >= 0)) {
        QString exMsg;
        QDebug(&exMsg).nospace()
            << "HTTP header netside: Field bytes are missing the field separator "
            << fieldSep << ": " << HumanReadable::Hexdump { fieldBytes, true, true };
        throw std::runtime_error(exMsg.toStdString());
    }

    Field theField { fieldBytes, fieldBytes.mid(0, i), fieldBytes.mid(i + 1), QByteArray() };
    if (theField.fieldName.isEmpty()) {
        QString exMsg;
        QDebug(&exMsg)
            << "HTTP header netside: Empty field name in field bytes"
            << HumanReadable::Hexdump { fieldBytes, true, true };
        throw std::runtime_error(exMsg.toStdString());
    }

    // Simplify linear white-space to single SPs,
    // with LWS at start and end trimmed.
    theField.fieldValue = HTTPRequest::simplifiedLinearWhiteSpace(theField.fieldValueRaw);

    _implPtr->append(theField);
}

QDebug operator<<(QDebug debug, const HeaderNetside::Field &field)
{
    QDebugStateSaver saver(debug);
    debug.nospace();

    debug << "HTTP::HeaderNetside::Field(";
    debug << "fieldName="  << field.fieldName << " ";
    debug << "fieldValue=" << field.fieldValue << ")";

    return debug;
}

}  // namespace HTTP
