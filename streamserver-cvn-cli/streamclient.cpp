#include "streamclient.h"

#include "log.h"

StreamClient::StreamClient(socketPtr_type socketPtr, quint64 id, QObject *parent) :
    QObject(parent), _id(id), _createdTimestamp(QDateTime::currentDateTime()),
    _socketPtr(std::move(socketPtr))
{
    _createdElapsed.start();
    _logPrefix = "{Client " + QString::number(_id) + "}";
    _socketPtr->setParent(this);
    connect(_socketPtr.get(), &QTcpSocket::readyRead, this, &StreamClient::receiveData);
    connect(_socketPtr.get(), &QTcpSocket::bytesWritten, this, &StreamClient::sendData);
}

quint64 StreamClient::id() const
{
    return _id;
}

const QString &StreamClient::logPrefix() const
{
    return _logPrefix;
}

QDateTime StreamClient::createdTimestamp() const
{
    return _createdTimestamp;
}

const QElapsedTimer &StreamClient::createdElapsed() const
{
    return _createdElapsed;
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

quint64 StreamClient::socketBytesReceived() const
{
    return _socketBytesReceived;
}

quint64 StreamClient::socketBytesSent() const
{
    return _socketBytesSent;
}

const HTTPRequest &StreamClient::httpRequest() const
{
    return _httpRequest;
}

const HTTPReply *StreamClient::httpReply() const
{
    return _httpReplyPtr.get();
}

bool StreamClient::tsStripAdditionalInfo() const
{
    return _tsStripAdditionalInfo;
}

void StreamClient::setTSStripAdditionalInfo(bool strip)
{
    if (verbose >= 2)
        qInfo() << qPrintable(_logPrefix) << "Changing TS strip additional info from" << _tsStripAdditionalInfo << "to" << strip;
    _tsStripAdditionalInfo = strip;
}

void StreamClient::queuePacket(const TSPacket &packet)
{
    if (!_forwardPackets) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Not queueing packet. Not set to forward packets (yet?)";

        return;
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Queueing packet";
    if (verbose >= 3)
        qDebug() << qPrintable(_logPrefix) << "Packet data:" << packet.bytes();

    _queue.append(packet);
}

void StreamClient::sendData()
{
    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Begin send data";

    if (_socketPtr->state() == QTcpSocket::ClosingState) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Socket in closing state, leaving send data early";
        return;
    }

    if (!_httpReplyPtr) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "No reply generated, yet, leaving send data early";
        return;
    }

    if (!_replyHeaderSent) {
        // Initially fill send buffer with HTTP reply.

        if (verbose >= 0) {
            qInfo() << qPrintable(_logPrefix) << "Sending server reply:"
                    << "HTTP version"   << _httpReplyPtr->httpVersion()
                    << "Status code"    << _httpReplyPtr->statusCode()
                    << "Status message" << _httpReplyPtr->statusMsg();
        }

        const QByteArray reply = _httpReplyPtr->toBytes();
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Filling send buffer with reply data:" << reply;

        _sendBuf.append(reply);
        _replyHeaderSent = true;
    }

    while (!_sendBuf.isEmpty() || !_queue.isEmpty()) {
        // Fill send buffer up to 1KiB.
        while (!_queue.isEmpty()) {
            const TSPacket &packet(_queue.front());
            const QByteArray bytes = _tsStripAdditionalInfo ?
                packet.toBasicPacketBytes() :
                packet.bytes();
            if (!(_sendBuf.length() + bytes.length() <= 1024))
                break;

            if (verbose >= 2)
                qDebug() << qPrintable(_logPrefix) << "Filling send buffer with" << bytes.length() << "bytes";
            if (verbose >= 3)
                qDebug() << qPrintable(_logPrefix) << "Filling with data:" << bytes;

            _sendBuf.append(bytes);
            _queue.pop_front();
        }

        // Try to send.
        qint64 count = _socketPtr->write(_sendBuf);
        if (count < 0) {
            qInfo() << qPrintable(_logPrefix) << "Write error:" << _socketPtr->errorString()
                    << ", aborting connection";
            _socketPtr->abort();
            break;
        }

        _socketBytesSent += count;
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Sent" << count << "bytes,"
                     << "total sent" << _socketBytesSent;
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Sent data:" << _sendBuf.left(count);

        // No more send is possible.
        if (count == 0)
            break;

        // Remove sent data from the send buffer.
        _sendBuf.remove(0, count);
    }

    if (_sendBuf.isEmpty() && !_forwardPackets) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Closing client connection after HTTP reply";
        _socketPtr->close();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish send data";
}

void StreamClient::processRequest()
{
    if (verbose >= 0) {
        qInfo() << qPrintable(_logPrefix) << "Processing client request:"
                << "Method" << _httpRequest.method()
                << "Path"   << _httpRequest.path()
                << "HTTP version" << _httpRequest.httpVersion()
                << "...";
    }

    const QByteArray httpVersion = _httpRequest.httpVersion();
    if (!(httpVersion == "HTTP/1.0" || httpVersion == "HTTP/1.1")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "HTTP version not recognized:" << httpVersion;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP version not recognized.\n");
        return;
    }

    const QByteArray method = _httpRequest.method();
    if (!(method == "GET" || method == "HEAD")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "HTTP method not supported:" << method;
        _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("HTTP method not supported.\n");
        return;
    }

    const QByteArray path = _httpRequest.path();
    if (!(path == "/" || path == "/stream.m2ts" || path == "/live.m2ts")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Path not found:" << path;
        _httpReplyPtr = std::make_unique<HTTPReply>(404, "Not Found");
        _httpReplyPtr->setHeader("Content-Type", "text/plain");
        _httpReplyPtr->setBody("Path not found.\n");
        return;
    }

    _httpReplyPtr = std::make_unique<HTTPReply>(200, "OK");
    _httpReplyPtr->setHeader("Content-Type", "video/mp2t");
    if (_httpRequest.method() == "HEAD") {
        if (verbose >= -1)
            qInfo() << qPrintable(_logPrefix) << "Request OK, HEAD only";
    }
    else {
        if (verbose >= -1)
            qInfo() << qPrintable(_logPrefix) << "Request OK, start forwarding TS packets";
        _forwardPackets = true;
    }
}

void StreamClient::close()
{
    // TODO: Respect current connection state,
    //       and ensure that all proper requests
    //       receive a proper reply...

    if (verbose >= 0)
        qInfo() << qPrintable(_logPrefix) << "Closing down... (programmatic request)";
    _socketPtr->close();
}

void StreamClient::receiveData()
{
    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Begin receive data";

    QByteArray buf;
    while (!(buf = _socketPtr->read(1024)).isEmpty()) {
        _socketBytesReceived += buf.length();
        if (verbose >= 2)
            qInfo() << qPrintable(_logPrefix) << "Received" << buf.length() << "bytes of data,"
                    << "total received" << _socketBytesReceived;
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Received data:" << buf;

        if (!_isReceiving) {
            if (verbose >= 0)
                qInfo() << qPrintable(_logPrefix) << "Unrecognized client data, aborting connection.";
            if (verbose >= 3)
                qInfo() << qPrintable(_logPrefix) << "Unrecognized client data was:" << buf;

            _socketPtr->abort();
            return;
        }

        try {
            _httpRequest.processChunk(buf);
        }
        catch (std::exception &ex) {
            _isReceiving = false;
            if (verbose >= 0)
                qInfo() << qPrintable(_logPrefix) << "Unable to parse network bytes as HTTP request:" << QString(ex.what());
            _httpReplyPtr = std::make_unique<HTTPReply>(400, "Bad Request");
            _httpReplyPtr->setHeader("Content-Type", "text/plain");
            _httpReplyPtr->setBody("Unable to parse HTTP request.\n");
            return;
        }
    }

    // TODO: How to handle error?
    //if (buf.isNull())
    //    _socketPtr->disconnected();

    // TODO: How to handle EOF?
    //if (buf.isEmpty())
    //    ...;

    if (_httpRequest.receiveState() == HTTPRequest::ReceiveState::Ready) {
        _isReceiving = false;
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Received request; processing...";
        processRequest();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish receive data";
}
