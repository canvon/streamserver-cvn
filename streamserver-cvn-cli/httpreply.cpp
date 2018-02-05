#include "httpreply.h"

#include <stdexcept>
#include <QBuffer>
#include <QTextStream>

HTTPReply::HTTPReply(int statusCode, const QString &statusMsg, const QString &httpVersion)
{
    setHttpVersion(httpVersion);
    setStatusCode(statusCode);
    setStatusMsg(statusMsg);
}

const QString &HTTPReply::httpVersion() const
{
    return _httpVersion;
}

void HTTPReply::setHttpVersion(const QString &version)
{
    if (version.isEmpty())
        throw std::invalid_argument("HTTP reply: HTTP version can't be empty");

    if (version.contains(' ') || version.contains('\r') || version.contains('\n'))
        throw std::invalid_argument("HTTP reply: Invalid characters found in to-be-set HTTP version");

    _httpVersion = version;
}

int HTTPReply::statusCode() const
{
    return _statusCode;
}

void HTTPReply::setStatusCode(int status)
{
    if (status < 100 || status > 999)
        throw std::invalid_argument("HTTP reply: Refusing to set invalid (non 3-digit) status code " +
                                    std::to_string(status));

    _statusCode = status;
}

const QString &HTTPReply::statusMsg() const
{
    return _statusMsg;
}

void HTTPReply::setStatusMsg(const QString &msg)
{
    if (msg.contains('\r') || msg.contains('\n'))
        throw std::invalid_argument("HTTP reply: Invalid characters found in to-be-set status message");

    _statusMsg = msg;
}

const HTTPReply::header_type &HTTPReply::header() const
{
    return _header;
}

void HTTPReply::setHeader(const QString &fieldName, const QString &fieldValue)
{
    // TODO: Update existing header fields?
    _header.append(std::make_pair(fieldName, fieldValue));
}

const QByteArray &HTTPReply::body() const
{
    return _body;
}

void HTTPReply::setBody(const QByteArray &body)
{
    _body = body;
    setHeader("Content-Length", QString::number(_body.length()));
}

QByteArray HTTPReply::toBytes() const
{
    QByteArray bufBytes;
    QBuffer buf(&bufBytes); buf.open(QIODevice::WriteOnly);
    QTextStream bufOut(&buf);

    // HTTP reply status line
    bufOut << _httpVersion << fieldSepStatusLine
           << _statusCode  << fieldSepStatusLine
           << _statusMsg   << lineSep;

    // Header
    for (const std::pair<QString, QString> field : _header)
        bufOut << field.first << fieldSepHeader << field.second << lineSep;
    bufOut << lineSep;

    // Body
    if (!_body.isEmpty()) {
        buf.write(_body);
    }

    return bufBytes;
}
