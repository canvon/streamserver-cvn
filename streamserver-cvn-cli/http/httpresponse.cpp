#include "httpresponse.h"

#include "httputil.h"

#include <stdexcept>
#include <QBuffer>
#include <QTextStream>

namespace SSCvn {
namespace HTTP {  // namespace SSCvn::HTTP

Response::Response(StatusCode statusCode, const QString &statusMsg, const QString &httpVersion)
{
    setHttpVersion(httpVersion);
    setStatusCode(statusCode);
    setStatusMsg(statusMsg);
}

const QString &Response::httpVersion() const
{
    return _httpVersion;
}

void Response::setHttpVersion(const QString &version)
{
    if (version.isEmpty())
        throw std::invalid_argument("HTTP response: HTTP version can't be empty");

    if (version.contains(' ') || version.contains('\r') || version.contains('\n'))
        throw std::invalid_argument("HTTP response: Invalid characters found in to-be-set HTTP version");

    _httpVersion = version;
}

StatusCode Response::statusCode() const
{
    return _statusCode;
}

void Response::setStatusCode(StatusCode status)
{
    if (status < 100 || status > 999)
        throw std::invalid_argument("HTTP response: Refusing to set invalid (non 3-digit) status code " +
                                    std::to_string(status));

    _statusCode = status;
}

const QString &Response::statusMsg() const
{
    return _statusMsg;
}

void Response::setStatusMsg(const QString &msg)
{
    if (msg.contains('\r') || msg.contains('\n'))
        throw std::invalid_argument("HTTP response: Invalid characters found in to-be-set status message");

    _statusMsg = msg;
}

const Response::header_type &Response::header() const
{
    return _header;
}

void Response::setHeader(const QString &fieldName, const QString &fieldValue)
{
    // TODO: Update existing header fields?
    _header.append(std::make_pair(fieldName, fieldValue));
}

const QByteArray &Response::body() const
{
    return _body;
}

void Response::setBody(const QByteArray &body)
{
    _body = body;
    setHeader("Content-Length", QString::number(_body.length()));
}

QByteArray Response::toBytes() const
{
    QByteArray bufBytes;
    QBuffer buf(&bufBytes); buf.open(QIODevice::WriteOnly);
    QTextStream bufOut(&buf);

    // HTTP response status line
    bufOut << _httpVersion << fieldSepStartLine
           << _statusCode  << fieldSepStartLine
           << _statusMsg   << lineSep;

    // Header
    for (const std::pair<QString, QString> field : _header)
        bufOut << field.first << fieldSepHeaderGenerate << field.second << lineSep;
    bufOut << lineSep;

    // Body
    bufOut.flush();
    if (!_body.isEmpty()) {
        buf.write(_body);
    }

    return bufBytes;
}

}  // namespace SSCvn::HTTP
}  // namespace SSCvn
