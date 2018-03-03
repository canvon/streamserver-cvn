#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include "httpheader.h"

#include <QByteArray>

using HTTPHeaderParser = HTTP::HeaderParser;

// An HTTP request from the wire.
class HTTPRequest
{
    qint64      _byteCount = 0;
    qint64      _byteCountMax = 10 * 1024;  // 10 KiB
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

    static QByteArray simplifiedLinearWhiteSpace(const QByteArray &bytes);

    qint64 byteCount() const;
    qint64 byteCountMax() const;
    void setByteCountMax(qint64 max);
    const QByteArray &buf() const;
    const QByteArray &headerLinesBuf() const;

    ReceiveState receiveState() const;
    const QByteArray &requestLine() const;
    const QByteArray &method() const;
    const QByteArray &path() const;
    const QByteArray &httpVersion() const;
    const HTTPHeaderParser &header() const;

    void processChunk(const QByteArray &in);
};

#endif // HTTPREQUEST_H
