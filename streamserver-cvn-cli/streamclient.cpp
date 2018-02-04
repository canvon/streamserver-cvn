#include "streamclient.h"

extern int verbose;

StreamClient::StreamClient(socketPtr_type socketPtr, quint64 id, QObject *parent) :
    QObject(parent), _id(id), _socketPtr(std::move(socketPtr))
{
    _logPrefix = "Client " + QString::number(_id);
    _socketPtr->setParent(this);
    connect(_socketPtr.get(), &QTcpSocket::readyRead, this, &StreamClient::receiveData);
    connect(_socketPtr.get(), &QTcpSocket::bytesWritten, this, &StreamClient::sendData);
}

quint64 StreamClient::id() const
{
    return _id;
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
    if (!_forwardPackets) {
        if (verbose >= 2)
            qDebug() << _logPrefix << "Not queueing packet. Not set to forward packets (yet?)";

        return;
    }

    if (verbose >= 2)
        qDebug() << _logPrefix << "Queueing packet";
    if (verbose >= 3)
        qDebug() << _logPrefix << "Packet data:" << packet.bytes();

    _queue.append(packet);
}

void StreamClient::sendData()
{
    if (verbose >= 2)
        qDebug() << _logPrefix << "Begin send data";

    if (_socketPtr->state() == QTcpSocket::ClosingState) {
        if (verbose >= 2)
            qDebug() << _logPrefix << "Socket in closing state, leaving send data early";
        return;
    }

    if (!_httpReplyPtr) {
        if (verbose >= 2)
            qDebug() << _logPrefix << "No reply generated, yet, leaving send data early";
        return;
    }

    if (!_replyHeaderSent) {
        // Initially fill send buffer with HTTP reply.

        qInfo() << _logPrefix << "Sending server reply:"
                << "HTTP version"   << _httpReplyPtr->httpVersion()
                << "Status code"    << _httpReplyPtr->statusCode()
                << "Status message" << _httpReplyPtr->statusMsg();

        const QByteArray reply = _httpReplyPtr->toBytes();
        if (verbose >= 3)
            qDebug() << _logPrefix << "Filling send buffer with reply data:" << reply;

        _sendBuf.append(reply);
        _replyHeaderSent = true;
    }

    while (!_sendBuf.isEmpty() || !_queue.isEmpty()) {
        // Fill send buffer up to 1KiB.
        while (!_queue.isEmpty() && _sendBuf.length() + _queue.front().bytes().length() <= 1024) {
            if (verbose >= 2)
                qDebug() << _logPrefix << "Filling send buffer with" << _queue.front().bytes().length() << "bytes";
            if (verbose >= 3)
                qDebug() << _logPrefix << "Filling with data:" << _queue.front().bytes();

            _sendBuf.append(_queue.front().bytes());
            _queue.pop_front();
        }

        // Try to send.
        qint64 count = _socketPtr->write(_sendBuf);
        if (count < 0) {
            qInfo() << _logPrefix << "Write error:" << _socketPtr->errorString()
                    << ", aborting connection";
            _socketPtr->abort();
            break;
        }

        if (verbose >= 2)
            qDebug() << _logPrefix << "Sent" << count << "bytes";
        if (verbose >= 3)
            qDebug() << _logPrefix << "Sent data:" << _sendBuf.left(count);

        // No more send is possible.
        if (count == 0)
            break;

        // Remove sent data from the send buffer.
        _sendBuf.remove(0, count);
    }

    if (_sendBuf.isEmpty() && !_forwardPackets)
    {
        qInfo() << _logPrefix << "Closing client connection after HTTP reply";
        _socketPtr->close();
    }

    if (verbose >= 2)
        qDebug() << _logPrefix << "Finish send data";
}

void StreamClient::processRequest()
{
    qInfo() << _logPrefix << "Processing client request:"
            << "Method" << _httpRequest.method()
            << "Path"   << _httpRequest.path()
            << "HTTP version" << _httpRequest.httpVersion()
            << "...";

    const QByteArray httpVersion = _httpRequest.httpVersion();
    if (!(httpVersion == "HTTP/1.0" || httpVersion == "HTTP/1.1")) {
        qInfo() << _logPrefix << "HTTP version not recognized:" << httpVersion;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP version not recognized.");
        return;
    }

    const QByteArray method = _httpRequest.method();
    if (!(method == "GET" || method == "HEAD")) {
        qInfo() << _logPrefix << "HTTP method not supported:" << method;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP method not supported.");
        return;
    }

    const QByteArray path = _httpRequest.path();
    if (!(path == "/" || path == "/stream.m2ts" || path == "/live.m2ts")) {
        qInfo() << _logPrefix << "Path not found:" << path;
        _httpReplyPtr = std::make_unique<HTTPReply>(404, "Not Found");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("Path not found.");
        return;
    }

    _httpReplyPtr = std::make_unique<HTTPReply>(200, "OK");
    _httpReplyPtr->setHeader("Content-Type", "video/mp2t");
    if (_httpRequest.method() == "HEAD") {
        qInfo() << _logPrefix << "Request OK, HEAD only";
    }
    else {
        qInfo() << _logPrefix << "Request OK, start forwarding TS packets";
        _forwardPackets = true;
    }
}

void StreamClient::receiveData()
{
    qDebug() << _logPrefix << "Begin receive data";

    QByteArray buf;
    while (!(buf = _socketPtr->read(1024)).isEmpty()) {
        if (verbose >= 2)
            qInfo() << _logPrefix << "Received" << buf.length() << "bytes of data";
        if (verbose >= 3)
            qDebug() << _logPrefix << "Received data:" << buf;

        if (_httpRequest.receiveState() == HTTPRequest::ReceiveState::Ready) {
            qInfo() << _logPrefix << "Unrecognized data after client request header, excepting.";
            if (verbose >= 3)
                qInfo() << _logPrefix << "Unrecognized data was:" << buf;

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
        qDebug() << _logPrefix << "Received request; processing...";
        processRequest();
    }

    qDebug() << _logPrefix << "Finish receive data";
}
