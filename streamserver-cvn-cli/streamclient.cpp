#include "streamclient.h"

#include "log.h"
#include "streamserver.h"
#include "http/httputil.h"
#include "http/httprequest_netside.h"
#include "http/httpresponse.h"
#include "humanreadable.h"

using SSCvn::log::verbose;

StreamClient::StreamClient(HTTP::ServerContext *httpServerContext, quint64 id, QObject *parent) :
    QObject(parent), _id(id), _createdTimestamp(QDateTime::currentDateTime()),
    _httpServerContext(httpServerContext)
{
    _createdElapsed.start();
    _logPrefix = "{Stream client " + QString::number(_id) +
        ", HTTP context " + QString::number(httpServerContext->id()) +
        ", HTTP client " + QString::number(httpServerContext->client()->id()) + "}";

    connect(httpServerContext, &QObject::destroyed, this, &StreamClient::handleHTTPServerContextDestroyed);
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

bool StreamClient::handleGenerateResponseBody(QByteArray &buf)
{
    if (!_forwardPackets)
        return false;

    // Need to wrap entire function into try-catch block, as we might be called via Qt event loop, and Qt doesn't like / recover from exceptions.
    try {

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
            if (!(buf.length() + bytes.length() <= 1024))
                break;

            if (verbose >= 2)
                qDebug() << qPrintable(_logPrefix) << "Filling send buffer with" << bytes.length() << "bytes";
            if (verbose >= 3)
                qDebug() << qPrintable(_logPrefix) << "Filling with data:" << bytes;

            buf.append(bytes);
            _queue.pop_front();
        }

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

    return true;
}

void StreamClient::handleHTTPServerContextDestroyed(QObject *obj)
{
    if (!obj)
        return;

    auto *ctx = qobject_cast<HTTP::ServerContext*>(obj);
    if (!ctx)
        return;

    if (ctx != _httpServerContext)
        return;

    _queue.clear();
    deleteLater();
}

void StreamClient::processRequest(HTTP::ServerContext *ctx)
{
    // TODO: Trust path matching done in HTTP::Server when that has been implemented!
    const QByteArray &path(ctx->request().path());
    if (!(path == "/" || path == "/stream.m2ts" || path == "/live.m2ts")) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Path not found:" << path;
        ctx->setResponseError(HTTP::SC_404_NotFound, "Path not found.\n");
        return;
    }

    QScopedPointer<HTTP::Response> response_ptr(new HTTP::Response(HTTP::SC_200_OK, "OK"));
    response_ptr->setHeader("Content-Type", "video/mp2t");
    ctx->setResponse(response_ptr.take());

    if (ctx->request().method() == "HEAD") {
        if (verbose >= -1)
            qInfo() << qPrintable(_logPrefix) << "Request OK, HEAD only";
    }
    else {
        if (verbose >= -1)
            qInfo() << qPrintable(_logPrefix) << "Request OK, start forwarding TS packets";
        _forwardPackets = true;
        connect(ctx, &HTTP::ServerContext::generateResponseBody, this, &StreamClient::handleGenerateResponseBody);
        ctx->setGenerateResponseBody(true);
    }
}

void StreamClient::close()
{
    // TODO: Respect current connection state,
    //       and ensure that all proper requests
    //       receive a proper response...

    if (verbose >= 0)
        qInfo() << qPrintable(_logPrefix) << "Closing down... (programmatic request)";
    if (_httpServerContext)
        _httpServerContext->client()->close();
}
