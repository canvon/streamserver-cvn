#include "httprequest_netside.h"

#include "humanreadable.h"

#include <stdexcept>

HTTPRequest::HTTPRequest()
{

}

QByteArray HTTPRequest::simplifiedLinearWhiteSpace(const QByteArray &bytes)
{
    QByteArray ret;
    QByteArray lws;
    enum { Null, Ret, LWS } dir;
    for (const char &c : bytes) {
        dir = Null;
        switch (c) {
        case '\r':  // CR. (LWS optionally starts with CR-LF.)
            if (lws.isEmpty())
                dir = LWS;
            else
                dir = Ret;
            break;
        case '\n':  // LF. (LWS optionally starts with CR-LF.)
            if (lws == "\r")
                dir = LWS;
            else
                dir = Ret;
            break;
        case ' ':  // SP (space)
        case '\t':  // HT (horizontal-tab)
            dir = LWS;
            break;
        default:
            dir = Ret;
            break;
        }

        switch (dir) {
        case Null:
        case Ret:
            if (ret.isEmpty()) {
                // Any leading LWS just gets removed.
            }
            else {
                if (!lws.isEmpty())
                    // Transform a sequence of LWS to a single SP.
                    ret.append(' ');
            }
            lws.clear();

            ret.append(c);
            break;
        case LWS:
            lws.append(c);
            break;
        }
    }

    // Any trailing LWS just gets removed.
    // (By ignoring the final value of the lws variable.)

    return ret;
}

qint64 HTTPRequest::byteCount() const
{
    return _byteCount;
}

qint64 HTTPRequest::byteCountMax() const
{
    return _byteCountMax;
}

void HTTPRequest::setByteCountMax(qint64 max)
{
    if (!(max >= 0))
        throw std::invalid_argument("HTTP request: Set byte count maximum: Invalid maximum " +
                                    std::to_string(max));

    _byteCountMax = max;
}

const QByteArray &HTTPRequest::buf() const
{
    return _buf;
}

const QByteArray &HTTPRequest::headerLinesBuf() const
{
    return _headerLinesBuf;
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

const HTTPHeaderParser &HTTPRequest::header() const
{
    return _header;
}

void HTTPRequest::processChunk(const QByteArray &in)
{
    if (_receiveState >= ReceiveState::Ready)
        throw std::runtime_error("HTTP request: Can't process chunk, as request is already ready");

    _buf.append(in);
    _byteCount += in.length();
    if (!(_byteCount <= _byteCountMax))
        throw std::runtime_error("HTTP request: Byte count maximum exceeded (" +
                                 std::to_string(_byteCount) + " bytes = " +
                                 HumanReadable::byteCount(_byteCount).toStdString() + ")");

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
            while (true) {
                int iLineSep = _buf.indexOf(lineSep);
                if (iLineSep < 0) {
                    // Not completely received, yet.
                    return;
                }
                else if (iLineSep == 0) {
                    // Empty line that terminates the header.
                    _buf.remove(0, lineSep.length());

                    // One last couple of header lines?
                    if (!_headerLinesBuf.isEmpty()) {
                        _headerLinesBuf.chop(lineSep.length());
                        _header.append(_headerLinesBuf);
                        _headerLinesBuf.clear();
                    }

                    _receiveState = ReceiveState::Body;
                    if (_method == "GET" || _method == "HEAD")
                        _receiveState = ReceiveState::Ready;
                    break;
                }

                QByteArray line = _buf.left(iLineSep + lineSep.length());
                _buf.remove(0, iLineSep + lineSep.length());
                if (line.startsWith(' ') || line.startsWith('\t')) {
                    // Linear white-space (LWS).
                    _headerLinesBuf.append(line);
                }
                else {
                    // Has there been unprocessed header lines before?
                    if (!_headerLinesBuf.isEmpty()) {
                        _headerLinesBuf.chop(lineSep.length());
                        _header.append(_headerLinesBuf);
                    }

                    _headerLinesBuf = line;
                }
            }
            break;
        case ReceiveState::Body:
            throw std::runtime_error("HTTP request: Request body not supported, yet");
        case ReceiveState::Ready:
            throw std::runtime_error("HTTP request: Trailing data");
        }
    }
}
