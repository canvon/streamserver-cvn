#include "streamclient.h"

extern int verbose;

StreamClient::StreamClient(std::unique_ptr<QTcpSocket> &&socketPtr, QObject *parent) :
    QObject(parent)
{
    _socketPtr = std::move(socketPtr);

    connect(_socketPtr.get(), &QTcpSocket::readyRead, this, &StreamClient::receiveData);
    connect(_socketPtr.get(), &QTcpSocket::bytesWritten, this, &StreamClient::sendData);
}

QTcpSocket &StreamClient::socket()
{
    if (!_socketPtr)
        throw std::runtime_error("No socket object in stream client");

    return *_socketPtr;
}

const QTcpSocket &StreamClient::socket() const
{
    if (!_socketPtr)
        throw std::runtime_error("No socket object in stream client");

    return *_socketPtr;
}

const HTTPRequest &StreamClient::httpRequest() const
{
    return _httpRequest;
}

const HTTPReply *StreamClient::httpReply() const
{
    return _httpReplyPtr.get();
}

void StreamClient::queuePacket(const TSPacket &packet)
{
    if (verbose >= 2)
        qDebug() << "Queueing packet";
    if (verbose >= 3)
        qDebug() << "Packet data:" << packet.bytes();

    _queue.append(packet);
}

void StreamClient::sendData()
{
    if (verbose >= 2)
        qDebug() << "Begin send data";

    if (!_httpReplyPtr)
        // No reply generated, yet.
        return;

    if (!_replyHeaderSent) {
        // Initially fill send buffer with HTTP reply.
        _sendBuf.append(_httpReplyPtr->toBytes());
        _replyHeaderSent = true;
    }

    while (!_sendBuf.isEmpty() || !_queue.isEmpty()) {
        // Fill send buffer up to 1KiB.
        while (!_queue.isEmpty() && _sendBuf.length() + _queue.front().bytes().length() <= 1024) {
            if (verbose >= 2)
                qDebug() << "Filling send buffer with" << _queue.front().bytes().length() << "bytes";

            _sendBuf.append(_queue.front().bytes());
            _queue.pop_front();
        }

        // Try to send.
        qint64 count = _socketPtr->write(_sendBuf);
        if (count < 0) {
            qInfo() << "Write error:" << _socketPtr->errorString()
                    << ", aborting connection";
            _socketPtr->abort();
            break;
        }

        if (verbose >= 2)
            qDebug() << "Sent" << count << "bytes";

        // No more send is possible.
        if (count == 0)
            break;

        // Remove sent data from the send buffer.
        _sendBuf.remove(0, count);
    }

    if (verbose >= 2)
        qDebug() << "Finish send data";
}

void StreamClient::processRequest()
{
    qInfo() << "Processing client request:"
            << "Method" << _httpRequest.method()
            << "Path"   << _httpRequest.path()
            << "HTTP version" << _httpRequest.httpVersion()
            << "...";

    const QByteArray httpVersion = _httpRequest.httpVersion();
    if (!(httpVersion == "HTTP/1.0" || httpVersion == "HTTP/1.1")) {
        qInfo() << "HTTP version not recognized:" << httpVersion;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP version not recognized.");
        return;
    }

    const QByteArray method = _httpRequest.method();
    if (!(method == "GET" || method == "HEAD")) {
        qInfo() << "HTTP method not supported:" << method;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP method not supported.");
        return;
    }

    const QByteArray path = _httpRequest.path();
    if (!(path == "/" || path == "/stream.m2ts" || path == "/live.m2ts")) {
        qInfo() << "Path not found:" << path;
        _httpReplyPtr = std::make_unique<HTTPReply>(404, "Not Found");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("Path not found.");
        return;
    }

    qInfo() << "Request OK";
    _httpReplyPtr = std::make_unique<HTTPReply>(200, "OK");
    _httpReplyPtr->setHeader("Content-Type", "video/mp2t");
}

void StreamClient::receiveData()
{
    qDebug() << "Begin receive data";

    QByteArray buf;
    while (!(buf = _socketPtr->read(1024)).isEmpty()) {
        if (_httpRequest.receiveState() == HTTPRequest::ReceiveState::Ready) {
            qInfo() << "Unrecognized data after client request header, excepting.";
            if (verbose >= 3)
                qInfo() << "Unrecognized data was:" << buf;

            throw std::runtime_error("Stream client: Unrecognized data after client request header");
        }

        _httpRequest.processChunk(buf);
    }

    // TODO: How to handle error?
    //if (buf.isNull())
    //    _socketPtr->disconnected();

    // TODO: How to handle EOF?
    //if (buf.isEmpty())
    //    ...;

    if (_httpRequest.receiveState() == HTTPRequest::ReceiveState::Ready) {
        qDebug() << "Received request; processing...";
        processRequest();
    }

    qDebug() << "Finish receive data";
}
