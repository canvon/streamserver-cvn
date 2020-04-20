#include "streamclient.h"

#include "log.h"
#include "streamserver.h"
#include "http/httputil.h"
#include "humanreadable.h"

using SSCvn::log::verbose;

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

StreamServer *StreamClient::parentServer() const
{
    QObject *theParent = parent();
    if (!theParent) {
        if (verbose >= 2)
            qWarning() << qPrintable(_logPrefix) << "Parent server: Parent not set";
        return nullptr;
    }

    auto theParentServer = dynamic_cast<StreamServer *>(theParent);
    if (!theParentServer) {
        if (verbose >= 2)
            qWarning() << qPrintable(_logPrefix) << "Parent server: Is not a StreamServer";
        return nullptr;
    }

    return theParentServer;
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

const HTTP::RequestNetside &StreamClient::httpRequest() const
{
    return _httpRequest;
}

const HTTP::Response *StreamClient::httpResponse() const
{
    return _httpResponsePtr.get();
}

void StreamClient::_setHttpResponseError(HTTP::StatusCode statusCode, const QByteArray &body)
{
    _httpResponsePtr = std::make_unique<HTTP::Response>(statusCode, HTTP::statusMsgFromStatusCode(statusCode));
    _httpResponsePtr->setHeader("Content-Type", "text/plain");
    _httpResponsePtr->setBody(body);
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
#ifdef TS_PACKET_V2
    if (_tsStripAdditionalInfo) {
        _tsGenerator.setPrefixLength(0);
    }
    else {
        const StreamServer *const server = parentServer();
        if (!server)
            throw std::runtime_error("StreamClient set TS strip additional info: Can't get TS packet size from StreamServer: Server missing");

        const qint64 tsPacketSize = server->tsPacketSize();
        if (tsPacketSize == 0)
            _tsGenerator.setPrefixLength(0);
        else
            _tsGenerator.setPrefixLength(tsPacketSize - TS::PacketV2::sizeBasic);
    }
#endif
}

#ifdef TS_PACKET_V2
TS::PacketV2Generator &StreamClient::tsGenerator()
{
    return _tsGenerator;
}

const TS::PacketV2Generator &StreamClient::tsGenerator() const
{
    return _tsGenerator;
}
#endif

#ifndef TS_PACKET_V2
void StreamClient::queuePacket(const TSPacket &packet)
#else
void StreamClient::queuePacket(const QSharedPointer<ConversionNode<TS::PacketV2>> &packetNode)
#endif
{
    if (!_forwardPackets) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Not queueing packet. Not set to forward packets (yet?)";

        return;
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Queueing packet";
    if (verbose >= 3)
        qDebug() << qPrintable(_logPrefix) << "Packet data:"
#ifndef TS_PACKET_V2
                 << packet.bytes();

    _queue.append(packet);
#else
                 << packetNode->data;

    _queue.append(packetNode);
#endif
}

void StreamClient::sendData()
{
    // Need to wrap entire function into try-catch block, as we might be called via Qt event loop, and Qt doesn't like / recover from exceptions.
    try {

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Begin send data";

    if (_socketPtr->state() == QTcpSocket::ClosingState) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Socket in closing state, leaving send data early";
        return;
    }

    if (!_httpResponsePtr) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "No response generated, yet, leaving send data early";
        return;
    }

    if (!_responseHeaderSent) {
        // Initially fill send buffer with HTTP response.

        if (verbose >= 0) {
            qInfo() << qPrintable(_logPrefix) << "Sending server response:"
                    << "HTTP version"   << _httpResponsePtr->httpVersion()
                    << "Status code"    << _httpResponsePtr->statusCode()
                    << "Status message" << _httpResponsePtr->statusMsg();
        }

        const QByteArray response = _httpResponsePtr->toBytes();
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Filling send buffer with response data:" << response;

        _sendBuf.append(response);
        _responseHeaderSent = true;
    }

    while (!_sendBuf.isEmpty() || !_queue.isEmpty()) {
        // Fill send buffer up to 1KiB.
        while (!_queue.isEmpty()) {
#ifndef TS_PACKET_V2
            const TSPacket &packet(_queue.front());
            const QByteArray bytes = _tsStripAdditionalInfo ?
                packet.toBasicPacketBytes() :
                packet.bytes();
#else
            QSharedPointer<ConversionNode<TS::PacketV2>> packetNode = _queue.front();
            QSharedPointer<ConversionNode<QByteArray>> bytesNode;
            QString errMsg;
            if (!_tsGenerator.generate(packetNode, &bytesNode, &errMsg)) {
                if (verbose >= 1)
                    qInfo() << qPrintable(_logPrefix) << "Packet generation error, discarding packet:" << errMsg;

                _queue.pop_front();
                continue;
            }
            const QByteArray &bytes(bytesNode->data);
#endif
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
            qInfo() << qPrintable(_logPrefix) << "Closing client connection after HTTP response";
        _socketPtr->close();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish send data";

    // End of try block.
    }
    catch (const std::exception &ex) {
        if (verbose >= 0) {
            qWarning().nospace()
                << qPrintable(_logPrefix) << " "
                << "Sending data: Got exception: " << ex.what();
        }

        // Otherwise, ignore... (Or what else could we do? Drop the client? Drop the first packet in the queue?)

        // Let's try to drop the first packet. (So hopefully we won't loop on this forever.)
        if (!_queue.isEmpty()) {
            if (verbose >= 1)
                qInfo() << qPrintable(_logPrefix) << "Sending data: Dropping one outgoing packet...";
            _queue.removeFirst();
        }
    }
}

void StreamClient::processRequest()
{
    if (verbose >= 0) {
        qInfo().nospace()
                << qPrintable(_logPrefix) << " Processing client request: "
                << "Method " << _httpRequest.method() << ", "
                << "Path "   << _httpRequest.path()   << ", "
                << "HTTP version " << _httpRequest.httpVersion()
                << "...";

        const HTTP::HeaderNetside &header(_httpRequest.header());
        qInfo() << qPrintable(_logPrefix) << "Headers extract:"
                << "Host:"       << header.fieldValues("Host")
                << "User-Agent:" << header.fieldValues("User-Agent");
    }
    if (verbose >= 1) {
        qInfo() << qPrintable(_logPrefix) << "HTTP header:";
        for (const HTTP::HeaderNetside::Field &headerField : _httpRequest.header().fields())
            qInfo() << qPrintable(_logPrefix) << headerField;
    }

    const QByteArray httpVersion = _httpRequest.httpVersion();
    if (!(httpVersion == "HTTP/1.0" || httpVersion == "HTTP/1.1")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "HTTP version not recognized:" << httpVersion;
        _setHttpResponseError(HTTP::SC_400_BadRequest, "HTTP version not recognized.\n");
        return;
    }

    // TODO: Determine host according to RFC2616 5.2
    QByteArray host;
    const QList<HTTP::HeaderNetside::Field> hostHeaders = _httpRequest.header().fields("Host");
    if (hostHeaders.length() > 1) {
        if (verbose >= 0) {
            QDebug info = qInfo();
            info << qPrintable(_logPrefix) << "Multiple HTTP Host headers:";
            for (const HTTP::HeaderNetside::Field &hostHeader : hostHeaders)
                info << hostHeader.fieldValue;
        }
        _setHttpResponseError(HTTP::SC_400_BadRequest, "Multiple HTTP Host headers in request.\n");
        return;
    }
    else if (hostHeaders.length() == 1) {
        host = hostHeaders.first().fieldValue;
    }
    else {
        // TODO: For HTTP/1.1 requests, give 400 Bad Request according to RFC2616 14.23
    }

    StreamServer *theParentServer = parentServer();
    if (!theParentServer) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Missing parent server object, can't check HTTP host of request" << host;
    }
    else {
        const auto hostWhitelist = parentServer()->serverHostWhitelist();
        if (!hostWhitelist.isEmpty()) {
            QString compareHost = host.toLower();
            if (!compareHost.contains(':'))
                compareHost.append(":80");
            else if (compareHost.endsWith(':'))
                compareHost.append("80");

            bool found = false;
            for (QString hostWhite : hostWhitelist) {
                QString compareHostWhite = hostWhite.toLower();
                if (!compareHostWhite.contains(':'))
                    compareHostWhite.append(":80");
                else if (compareHostWhite.endsWith(':'))
                    compareHostWhite.append("80");

                if (compareHost == compareHostWhite) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (verbose >= 0)
                    qInfo() << qPrintable(_logPrefix) << "HTTP host invalid for this server:" << host;
                _setHttpResponseError(HTTP::SC_400_BadRequest, "HTTP host invalid for this server\n");
                return;
            }
        }
    }

    const QByteArray method = _httpRequest.method();
    if (!(method == "GET" || method == "HEAD")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "HTTP method not supported:" << method;
        _setHttpResponseError(HTTP::SC_400_BadRequest, "HTTP method not supported.\n");
        return;
    }

    const QByteArray path = _httpRequest.path();
    if (!(path == "/" || path == "/stream.m2ts" || path == "/live.m2ts")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Path not found:" << path;
        _setHttpResponseError(HTTP::SC_404_NotFound, "Path not found.\n");
        return;
    }

    _httpResponsePtr = std::make_unique<HTTP::Response>(HTTP::SC_200_OK, "OK");
    _httpResponsePtr->setHeader("Content-Type", "video/mp2t");
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
    //       receive a proper response...

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
            if (verbose >= 0) {
                qInfo() << qPrintable(_logPrefix) << "Unable to parse network bytes as HTTP request:" << ex.what();
                qInfo() << qPrintable(_logPrefix) << "Buffer was"
                        << HumanReadable::Hexdump { _httpRequest.buf(), true, true, true };
                qInfo() << qPrintable(_logPrefix) << "Header lines buffer was"
                        << HumanReadable::Hexdump { _httpRequest.headerLinesBuf(), true, true, true };
                qInfo() << qPrintable(_logPrefix) << "Rejected chunk was"
                        << HumanReadable::Hexdump { buf, true, true, true };
            }
            _setHttpResponseError(HTTP::SC_400_BadRequest, "Unable to parse HTTP request.\n");
            return;
        }
    }

    // TODO: How to handle error?
    //if (buf.isNull())
    //    _socketPtr->disconnected();

    // TODO: How to handle EOF?
    //if (buf.isEmpty())
    //    ...;

    if (_httpRequest.receiveState() == HTTP::RequestNetside::ReceiveState::Ready) {
        _isReceiving = false;
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Received request; processing...";
        processRequest();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish receive data";
}
