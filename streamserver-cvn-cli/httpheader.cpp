#include "httpheader.h"

#include "humanreadable.h"

#include <stdexcept>
#include <QDebug>
#include <QMultiMap>

namespace HTTP {

const QByteArray HeaderParser::fieldSep = ":";

namespace impl {

class HeaderParserImpl {
    QList<HeaderParser::Field>  _fields;
    QMultiMap<QByteArray, int>  _fieldNameIndices;
    friend HeaderParser;

public:
    void append(const HeaderParser::Field &theField)
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
            const HeaderParser::Field &theField(_fields.at(i));
            _fieldNameIndices.insert(theField.fieldName.toLower(), i);
        }
    }
};

}  // namespace HTTP::impl

HeaderParser::HeaderParser() :
    _implPtr(std::make_unique<impl::HeaderParserImpl>())
{

}

HeaderParser::~HeaderParser()
{

}

QList<HeaderParser::Field> HeaderParser::fields() const
{
    return _implPtr->_fields;
}

QList<HeaderParser::Field> HeaderParser::fields(const QByteArray &fieldName) const
{
    QList<Field> ret;
    QList<int> indicesReverse = _implPtr->_fieldNameIndices.values(fieldName.toLower());
    for (auto i = indicesReverse.crbegin(); i != indicesReverse.crend(); i++)
        ret.append(_implPtr->_fields.at(*i));

    return ret;
}

void HeaderParser::setField(const QByteArray &fieldName, const QByteArray &fieldValue)
{
    if (fieldName.isEmpty())
        throw std::invalid_argument("HTTP header parser: Set field: Field name can't be empty");

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

void HeaderParser::append(const QByteArray &fieldBytes)
{
    int i = fieldBytes.indexOf(fieldSep);
    if (!(i >= 0)) {
        QString exMsg;
        QDebug(&exMsg).nospace()
            << "HTTP header parser: Field bytes are missing the field separator "
            << fieldSep << ": " << HumanReadable::Hexdump { fieldBytes, true, true };
        throw std::runtime_error(exMsg.toStdString());
    }

    Field theField { fieldBytes, fieldBytes.mid(0, i), fieldBytes.mid(i + 1), QByteArray() };
    if (theField.fieldName.isEmpty()) {
        QString exMsg;
        QDebug(&exMsg)
            << "HTTP header parser: Empty field name in field bytes"
            << HumanReadable::Hexdump { fieldBytes, true, true };
        throw std::runtime_error(exMsg.toStdString());
    }

    // TODO: Remove linear white-space.
    theField.fieldValue = theField.fieldValueRaw.trimmed();

    _implPtr->append(theField);
}

}  // namespace HTTP
