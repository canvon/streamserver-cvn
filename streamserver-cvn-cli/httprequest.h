#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include "httpheader.h"

#include <QByteArray>

using HTTPHeaderParser = HTTP::HeaderParser;

// An HTTP request from the wire.
class HTTPRequest
{
    QByteArray  _buf;
    QByteArray  _headerLinesBuf;
public:
    enum class ReceiveState {
        RequestLine,
        Header,
        Body,
        Ready,
    };
private:
    ReceiveState  _receiveState = ReceiveState::RequestLine;
    QByteArray    _requestLine;
    QByteArray    _method;
    QByteArray    _path;
    QByteArray    _httpVersion;
    HTTPHeaderParser  _header;
    QByteArray    _body;

public:
    explicit HTTPRequest();

    const QByteArray lineSep = "\r\n";
    const QByteArray fieldSepRequestLine = " ";

    ReceiveState receiveState() const;
    const QByteArray &requestLine() const;
    const QByteArray &method() const;
    const QByteArray &path() const;
    const QByteArray &httpVersion() const;
    const HTTPHeaderParser &header() const;

    void processChunk(const QByteArray &in);
};

#endif // HTTPREQUEST_H
