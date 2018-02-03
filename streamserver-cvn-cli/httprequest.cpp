#include "httprequest.h"

#include <stdexcept>

HTTPRequest::HTTPRequest()
{

}

HTTPRequest::ReceiveState HTTPRequest::receiveState() const
{
    return _receiveState;
}

const QByteArray &HTTPRequest::requestLine() const
{
    if (_receiveState <= ReceiveState::RequestLine)
        throw std::runtime_error("HTTP request: Request line is not available, yet");

    return _requestLine;
}

const QByteArray &HTTPRequest::method() const
{
    if (_receiveState <= ReceiveState::RequestLine)
        throw std::runtime_error("HTTP request: Request method is not available, yet");

    return _method;
}

const QByteArray &HTTPRequest::path() const
{
    if (_receiveState <= ReceiveState::RequestLine)
        throw std::runtime_error("HTTP request: Request path is not available, yet");

    return _path;
}

const QByteArray &HTTPRequest::httpVersion() const
{
    if (_receiveState <= ReceiveState::RequestLine)
        throw std::runtime_error("HTTP request: HTTP version is not available, yet");

    return _httpVersion;
}

void HTTPRequest::processChunk(const QByteArray &in)
{
    if (_receiveState >= ReceiveState::Ready)
        throw std::runtime_error("HTTP request: Can't process chunk, as request is already ready");

    _buf.append(in);
    while (_buf.length() > 0) {
        switch (_receiveState) {
        case ReceiveState::RequestLine:
        {
            int iLineSep = _buf.indexOf(lineSep);
            if (iLineSep < 0)
                // Not completely received, yet.
                return;
            _requestLine = _buf.left(iLineSep);

            int iFrom = 0, iFieldSep;

            iFieldSep = _requestLine.indexOf(fieldSepRequestLine, iFrom);
            if (iFieldSep < 0)
                throw std::runtime_error("HTTP request: No field separator after HTTP method");
            _method = _requestLine.mid(iFrom, iFieldSep - iFrom);
            if (_method.isEmpty())
                throw std::runtime_error("HTTP request: HTTP method is missing");
            iFrom = iFieldSep + fieldSepRequestLine.length();

            iFieldSep = _requestLine.indexOf(fieldSepRequestLine, iFrom);
            if (iFieldSep < 0)
                throw std::runtime_error("HTTP request: No field separator after request path");
            _path = _requestLine.mid(iFrom, iFieldSep - iFrom);
            if (_path.isEmpty())
                throw std::runtime_error("HTTP request: Request path is missing");
            iFrom = iFieldSep + fieldSepRequestLine.length();

            _httpVersion = _requestLine.mid(iFrom);
            if (_httpVersion.isEmpty())
                throw std::runtime_error("HTTP request: Request version is missing");

            _buf.remove(0, iLineSep + lineSep.length());
            _receiveState = ReceiveState::Header;
            break;
        }
        case ReceiveState::Header:
        {
            QByteArray lineSep2 = lineSep + lineSep;
            int iLineSep2 = _buf.indexOf(lineSep2);
            if (iLineSep2 < 0)
                // Not completely received, yet.
                return;
            _header = _buf.left(iLineSep2 + lineSep.length());

            _buf.remove(0, iLineSep2 + lineSep2.length());
            _receiveState = ReceiveState::Body;
            if (_method == "GET" || _method == "HEAD")
                _receiveState = ReceiveState::Ready;
            break;
        }
        case ReceiveState::Body:
            throw std::runtime_error("HTTP request: Request body not supported, yet");
        case ReceiveState::Ready:
            throw std::runtime_error("HTTP request: Trailing data");
        }
    }
}
