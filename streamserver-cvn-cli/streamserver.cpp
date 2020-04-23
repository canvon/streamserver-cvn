#include "streamserver.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>
#include <system_error>
#include <functional>
#include <QDebug>
#include <QCoreApplication>
#include <QTcpServer>

// (Note: As of 2019-04-17, we need both old and new packet defined...)
#include "tspacket.h"
#include "tspacketv2.h"
#include "humanreadable.h"
#include "log.h"
#include "http/httprequest_netside.h"

using SSCvn::log::verbose;

namespace {

double timenow()
{
    double now;
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
        throw std::system_error(errno, std::generic_category(),
                                "Can't get time for monotonic clock");
    now = t.tv_sec;
    now += static_cast<double>(t.tv_nsec)/static_cast<double>(1000000000);
    return now;
}

}  // namespace


/*
 * StreamHandler
 */

class StreamHandlerPrivate {
    StreamHandler *q_ptr;
    Q_DECLARE_PUBLIC(StreamHandler)

    StreamServer *_streamServer;

    explicit StreamHandlerPrivate(StreamServer *streamServer, StreamHandler *q);
};

StreamHandlerPrivate::StreamHandlerPrivate(StreamServer *streamServer, StreamHandler *q) : q_ptr(q),
    _streamServer(streamServer)
{
    const std::string prefix = "StreamHandler hidden implementation ctor: ";

    if (!q_ptr)
        throw std::invalid_argument(prefix + "Back-pointer must not be null");
    if (!_streamServer)
        throw std::invalid_argument(prefix + "StreamServer must not be null");
}


StreamHandler::StreamHandler(StreamServer *streamServer) :
    d_ptr(new StreamHandlerPrivate(streamServer, this))
{

}

QString StreamHandler::name() const
{
    return "StreamServer client producer";
}

void StreamHandler::handleRequest(HTTP::ServerContext *ctx)
{
    Q_D(StreamHandler);

    auto client_ptr = d->_streamServer->client(ctx);
    if (!client_ptr) {
        if (verbose >= -1)
            qCritical() << qPrintable(name() + ":") << "StreamServer returned no stream client";
        return;
    }

    client_ptr->processRequest(ctx);
}


/*
 * StreamServer
 */

StreamServer::StreamServer(std::unique_ptr<QFile> inputFilePtr, HTTP::Server *httpServer, QObject *parent) :
    QObject(parent),
    _httpServer(httpServer),
    _httpServerHandler(new StreamHandler(this)),
    _inputFilePtr(std::move(inputFilePtr))
{
    if (!_httpServer)
        throw std::runtime_error("StreamServer ctor: HTTP server must not be null");

    // TODO: Be more specific.
    _httpServer->setDefaultHandler(_httpServerHandler);
}

bool StreamServer::isShuttingDown() const
{
    return _isShuttingDown;
}

HTTP::Server *StreamServer::httpServer() const
{
    return _httpServer;
}

StreamClient *StreamServer::client(HTTP::ServerContext *ctx)
{
    if (verbose >= -1) {
        qInfo() << "StreamServer: Creating stream client" << _nextClientID
                << "from HTTP context" << ctx->id()
                << "of HTTP client" << ctx->client()->id()
                << "from" << ctx->client()->peerAddress()
                << "port" << ctx->client()->peerPort()
                << "requesting" << ctx->request().path();
    }

    // Set up client object and signal mapping.
    auto *client_ptr = new StreamClient(ctx, _nextClientID++, this);
    connect(client_ptr, &QObject::destroyed, this, &StreamServer::handleStreamClientDestroyed);

    // Pass on current settings.
    client_ptr->setTSStripAdditionalInfo(_tsStripAdditionalInfoDefault);

    // Store client object in list.
    _clients.append(client_ptr);

    if (verbose >= 0)
        qInfo() << "Stream client count:" << _clients.length();

    return client_ptr;
}

QFile &StreamServer::inputFile()
{
    if (!_inputFilePtr)
        throw std::runtime_error("No input file object in stream server");

    return *_inputFilePtr;
}

const QFile &StreamServer::inputFile() const
{
    if (!_inputFilePtr)
        throw std::runtime_error("No input file object in stream server");

    return *_inputFilePtr;
}

bool StreamServer::inputFileOpenNonblocking() const
{
    return _inputFileOpenNonblocking;
}

void StreamServer::setInputFileOpenNonblocking(bool nonblock)
{
    if (verbose >= 1)
        qInfo() << "Changing input file open non-blocking from" << _inputFileOpenNonblocking << "to" << nonblock;
    _inputFileOpenNonblocking = nonblock;
}

int StreamServer::inputFileReopenTimeoutMillisec() const
{
    return _inputFileReopenTimeoutMillisec;
}

void StreamServer::setInputFileReopenTimeoutMillisec(int timeoutMillisec)
{
    if (verbose >= 1)
        qInfo() << "Changing input file reopen timeout from" << _inputFileReopenTimeoutMillisec << "ms to" << timeoutMillisec << "ms";
    _inputFileReopenTimeoutMillisec = timeoutMillisec;
}

qint64 StreamServer::tsPacketSize() const
{
    return _tsPacketSize;
}

void StreamServer::setTSPacketSize(qint64 size)
{
    if (!(TSPacket::lengthBasic <= size && size <= TSPacket::lengthBasic * 2))
        throw std::runtime_error("Stream server: Can't set TS packet size to invalid value " + std::to_string(size));

    if (verbose >= 1)
        qInfo() << "Changing TS packet size from" << _tsPacketSize << "to" << size;
    _tsPacketSize = size;
}

bool StreamServer::tsPacketAutosize() const
{
    return _tsPacketAutosize;
}

void StreamServer::setTSPacketAutosize(bool autosize)
{
    if (verbose >= 1)
        qInfo() << "Changing TS packet autosize from" << _tsPacketAutosize << "to" << autosize;
    _tsPacketAutosize = autosize;
}

bool StreamServer::tsStripAdditionalInfoDefault() const
{
    return _tsStripAdditionalInfoDefault;
}

void StreamServer::setTSStripAdditionalInfoDefault(bool strip)
{
    if (verbose >= 1)
        qInfo() << "Changing TS strip additional info default from" << _tsStripAdditionalInfoDefault << "to" << strip;
    _tsStripAdditionalInfoDefault = strip;
}

StreamServer::BrakeType StreamServer::brakeType() const
{
    return _brakeType;
}

void StreamServer::setBrakeType(StreamServer::BrakeType type)
{
    if (verbose >= 1)
        qInfo() << "Changing brake type from" << _brakeType << "to" << type;
    _brakeType = type;
}

void StreamServer::handleStreamClientDestroyed(QObject *obj)
{
    auto *streamClient = qobject_cast<StreamClient*>(obj);
    if (!streamClient) {
        if (verbose >= 0) {
            qWarning() << "StreamServer: Can't handle stream client destroyed:"
                       << "Passed object is not a StreamClient!";
        }
        return;
    }

    _clients.removeOne(streamClient);
}

void StreamServer::handleHTTPServerClientDestroyed(HTTP::ServerClient * /* httpServerClient */)
{
    if (_isShuttingDown && _clients.length() == 0) {
        if (verbose >= -1)
            qInfo() << "Shutdown: Client count reached zero, exiting event loop";
        if (qApp)
            qApp->exit();
        else
            qFatal("Shutdown: Can't access application object to exit event loop");
    }
}

void StreamServer::initInput()
{
    if (verbose >= 1)
        qInfo() << "Initializing input";

    if (!_inputFilePtr->isOpen()) {
        if (_inputFileName.isNull()) {
            _inputFileName = _inputFilePtr->fileName();
            if (verbose >= 1)
                qInfo() << "Initialized input file name from passed-in input file:"
                        << _inputFileName;
        }
        const QString fileName = _inputFileName;

        if (_tsPacketAutosize)
            _tsPacketSize = 0;  // Request immediate re-detection.
        _openRealTimeValid = false;
        _openRealTime = 0;

        bool openSucceeded = false;
        QString errMsgInfix;
        if (_inputFileOpenNonblocking) {
            if (verbose >= -1)
                qInfo() << "Opening input file" << fileName << "in non-blocking mode...";

            int fd = open(QFile::encodeName(fileName).constData(), O_RDONLY | O_NONBLOCK);
            if (fd < 0)
                throw std::system_error(errno, std::generic_category(),
                                        "Can't open input file \"" + fileName.toStdString() + "\"");
            openSucceeded = _inputFilePtr->open(fd, QFile::ReadOnly, QFile::AutoCloseHandle);
            if (!openSucceeded) {
                errMsgInfix = " from fd " + QString::number(fd);
                close(fd);
            }
        }
        else {
            if (verbose >= -1)
                qInfo() << "Opening input file" << fileName << "in normal (blocking) mode...";

            openSucceeded = _inputFilePtr->open(QFile::ReadOnly);
        }

        if (!openSucceeded) {
            const QString err = _inputFilePtr->errorString();
            qCritical().nospace()
                << "Can't open input file " << fileName
                << qPrintable(errMsgInfix) << ": " << qPrintable(err);
            //qApp->exit(1);  // We can't always do this, as the main loop maybe has not started yet, so exiting it will have no effect!
            throw std::runtime_error("Can't open input file \"" + fileName.toStdString() + "\"" +
                                     errMsgInfix.toStdString() + ": " + err.toStdString());
        }
    }

    // Prepare a new notifier.
    int inputFileHandle = _inputFilePtr->handle();
    if (inputFileHandle < 0)
        throw std::runtime_error("Can't get handle for input file");
    _inputFileNotifierPtr = std::make_unique<QSocketNotifier>(
        inputFileHandle, QSocketNotifier::Read, this);
    connect(_inputFileNotifierPtr.get(), &QSocketNotifier::activated, this, &StreamServer::processInput);

    if (verbose >= 1)
        qInfo() << "Successfully initialized input";
}

void StreamServer::finalizeInput()
{
    if (verbose >= 1)
        qInfo() << "Finalizing input";

    if (verbose >= -1)
        qInfo() << "Closing input...";
    _inputFilePtr->close();

    // Stop notifier gracefully, otherwise it outputs error messages from the event loop.
    if (_inputFileNotifierPtr) {
        _inputFileNotifierPtr->setEnabled(false);
        _inputFileNotifierPtr.reset();
    }

    if (verbose >= 1)
        qInfo() << "Successfully finalized input";
}

void StreamServer::initInputSlot()
{
    bool succeeded = false;
    try {
        initInput();
        succeeded = true;
    }
    catch (std::exception &ex) {
        qCritical() << "Error (re)initializing input:" << ex.what();
    }
    catch (...) {
        qCritical() << "Unknown error (re)initializing input";
    }

    if (!succeeded) {
        shutdown();
    }
}

void StreamServer::processInput()
{
    qint64 readSize = _tsPacketSize;
    if (readSize == 0) {
        if (!_tsPacketAutosize)
            qFatal("TS packet autosize turned off but no fixed packet size set!");

        if (verbose >= 1)
            qInfo() << "Input TS packet size set to" << _tsPacketSize << "/ immediate automatic detection."
                    << "Starting with basic length" << TSPacket::lengthBasic;
        readSize = TSPacket::lengthBasic;
    }

    QByteArray packetBytes = _inputFilePtr->read(readSize);
    if (_tsPacketSize == 0 && !packetBytes.isNull() && packetBytes.length() == readSize) {
        if (packetBytes.startsWith(TSPacket::syncByte)) {
            // If additional data is already available, try to detect formats with suffix after basic packet.
            const QByteArray nextPacketBytes = _inputFilePtr->peek(readSize);
            if (nextPacketBytes.startsWith(TSPacket::syncByte)) {
                if (verbose >= 1)
                    qInfo() << "Good; sync byte found in this and next packet";
            }
            else {
                if (nextPacketBytes.length() <= 20) {
                    if (verbose >= 1)
                        qInfo() << "Sync byte found, but not enough further data available to detect packet length";
                }
                else {
                    if (verbose >= 1)
                        qInfo() << "Next packet does not start with sync byte";
                    if (nextPacketBytes.length() > 16 && nextPacketBytes.at(16) == TSPacket::syncByte) {
                        if (verbose >= 1)
                            qInfo() << "Next packet offset 16 contains sync byte, assuming 16-byte suffix";
                        packetBytes.append(_inputFilePtr->read(16));
                        readSize += 16;
                    }
                    else if (nextPacketBytes.length() > 20 && nextPacketBytes.at(20) == TSPacket::syncByte) {
                        if (verbose >= 1)
                            qInfo() << "Next packet offset 20 contains sync byte, assuming 20-byte suffix";
                        packetBytes.append(_inputFilePtr->read(20));
                        readSize += 20;
                    }
                    else {
                        // TODO: Do something other than forcibly end? E.g., ignore some packets?
                        qFatal("TS packet sync byte not found in next packet of input");
                    }
                }
            }
        }
        else {
            if (verbose >= 1)
                qInfo() << "Initial packet does not start with sync byte";
            if (packetBytes.at(4) == TSPacket::syncByte) {
                if (verbose >= 1)
                    qInfo() << "Offset 4 contains sync byte, assuming 4-byte TimeCode prefix";
                packetBytes.append(_inputFilePtr->read(4));
                readSize += 4;
            }
            else {
                // TODO: Do something other than forcibly end? E.g., ignore some packets?
                qFatal("TS packet sync byte not found in input");
            }
        }
    }
    if (packetBytes.isNull()) {
        if (verbose >= 0)
            qInfo() << "EOF on input, finalizing...";
        finalizeInput();

        if (verbose >= 1)
            qInfo() << "Setting up timer to open input again after" << _inputFileReopenTimeoutMillisec << "ms";
        QTimer::singleShot(_inputFileReopenTimeoutMillisec,
            this, &StreamServer::initInputSlot);

        return;
    }
    if (verbose >= 3)
        qDebug() << "Read data:" << packetBytes;

    if (packetBytes.length() != readSize) {
        qWarning().nospace()
            << "Desync: Read packet should be size " << readSize
            << ", but was " << packetBytes.length();
        // TODO: Try a resync via TS packet sync byte?
        return;
    }

    // Actually process the read data.
    try {
#ifndef TS_PACKET_V2
        TSPacket packet(packetBytes);
        const QString &errmsg(packet.errorMessage());
        const bool success = errmsg.isNull();
#else
        auto packetBytesNode = QSharedPointer<ConversionNode<QByteArray>>::create(packetBytes);
        QSharedPointer<ConversionNode<TS::PacketV2>> packetNode;
        QString errmsg;
        if (readSize > 0)
            _tsParser.setPrefixLength(readSize - TS::PacketV2::sizeBasic);
        const bool success = _tsParser.parse(packetBytesNode, &packetNode, &errmsg);
        if (!packetNode) {
            if (verbose >= 0)
                qWarning() << "TS packet parsing didn't yield a packet node, skipping bytes...";
            return;
        }
        TS::PacketV2 &packet(packetNode->data);
#endif
        if (verbose >= 3)
            qInfo() << "TS packet contents:" << packet;
        if (verbose >= 0 && !success)
            qWarning() << "TS packet error:" << qPrintable(errmsg);
        if (!success) {
            if (++_inputConsecutiveErrorCount >= 16 && _tsPacketAutosize) {
                if (_tsPacketSize > 0) {
                    qWarning() << "Got" << _inputConsecutiveErrorCount << "consecutive errors, trying to re-sync and re-detect TS packet size...";

                    int iSyncByte, pass = 0;
                    while (++pass <= TSPacket::lengthBasic + 20 &&
                           (iSyncByte = packetBytes.indexOf(TSPacket::syncByte)) > 0)
                    {
                        if (verbose >= 1)
                            qInfo() << "Throwing away" << iSyncByte << "bytes";
                        packetBytes.remove(0, iSyncByte);

                        qint64 fillUp = TSPacket::lengthBasic - packetBytes.length();
                        if (verbose >= 1)
                            qInfo() << "Reading in" << fillUp << "additional bytes to fill up buffer...";
                        packetBytes.append(_inputFilePtr->read(fillUp));
                        if (packetBytes.length() < TSPacket::lengthBasic)
                            qFatal("Not enough data available for read during re-sync");

                        const QByteArray followingBytes = _inputFilePtr->peek(21);
                        if (followingBytes.isEmpty()) {
                            if (verbose >= 1)
                                qInfo() << "Re-sync: Sync byte found, but not enough further data available. The synchronization is just a guess";
                            break;
                        }
                        else if (followingBytes.startsWith(TSPacket::syncByte)) {
                            if (verbose >= 1)
                                qInfo() << "Re-sync: Good; sync byte found in this and next packet";
                            break;
                        }
                        else if (followingBytes.length() > 4 && followingBytes.at(4) == TSPacket::syncByte) {
                            if (verbose >= 1)
                                qInfo() << "Re-sync: Sync byte found in this packet, and at offset 4 in following bytes;"
                                        << "next read will probably get a 4-byte TimeCode prefix style packet";
                            break;
                        }
                        else if (followingBytes.length() > 16 && followingBytes.at(16) == TSPacket::syncByte) {
                            if (verbose >= 1) {
                                qInfo() << "Re-sync: Sync byte found in this packet, and at offset 16 in following bytes";
                                qInfo() << "Need to read & discard 16 additional bytes...";
                            }
                            if (_inputFilePtr->read(16).length() != 16)
                                qFatal("Failed to read & discard 16 bytes during re-sync");
                            break;
                        }
                        else if (followingBytes.length() > 20 && followingBytes.at(20) == TSPacket::syncByte) {
                            if (verbose >= 1) {
                                qInfo() << "Re-sync: Sync byte found in this packet, and at offset 20 in following bytes";
                                qInfo() << "Need to read & discard 20 additional bytes...";
                            }
                            if (_inputFilePtr->read(16).length() != 20)
                                qFatal("Failed to read & discard 20 bytes during re-sync");
                            break;
                        }

                        if (verbose >= 1)
                            qInfo() << "Re-sync: Not good, found a sync byte but no related other sync byte in pass" << pass;
                    }
                    if (!packetBytes.startsWith(TSPacket::syncByte))
                        qFatal("Giving up resync after %d passes", pass);

                    _tsPacketSize = 0;
                }
                _inputConsecutiveErrorCount = 0;
            }
        }
        else {
            _inputConsecutiveErrorCount = 0;
            if (_tsPacketSize == 0) {
                _tsPacketSize = readSize;
                if (verbose >= 0)
                    qInfo().nospace() << "Detected TS packet size of " << _tsPacketSize << ", which is basic length plus " << (_tsPacketSize - TSPacket::lengthBasic);
            }
        }

#ifndef TS_PACKET_V2
        auto af = packet.adaptationField();
        bool afModified = false;
        if (af && af->PCRFlag() && af->PCR()) {
            double pcr = af->PCR()->toSecs();
#else
        auto &af(packet.adaptationField);
        bool afModified = false;
        if (af.pcrFlag) {
            double pcr = af.programClockReference.toSecs();
#endif
            if (!_openRealTimeValid) {
                _openRealTime = timenow() - pcr;
                _openRealTimeValid = true;
                if (verbose >= 0)
                    qDebug() << "Initialized _openRealTime to" << fixed << _openRealTime;
            }
            double now = timenow() - _openRealTime;
            double dt = (pcr - _lastPacketTime) - (now - _lastRealTime);
            if (_lastPacketTime + 1 < pcr || pcr < _lastPacketTime) {
                // Discontinuity, just keep sending.
#ifndef TS_PACKET_V2
                bool discontinuityBefore = af->discontinuityIndicator();
                af->setDiscontinuityIndicator(true);
#else
                bool discontinuityBefore = af.discontinuityIndicator.value;
                af.discontinuityIndicator.value = true;
#endif
                afModified = true;
                if (verbose >= 0) {
                    qInfo().nospace()
                        << "Discontinuity detected; Discontinuity Indicator was "
                        << discontinuityBefore << ", now set to "
#ifndef TS_PACKET_V2
                        << af->discontinuityIndicator();
#else
                        << af.discontinuityIndicator.value;
#endif
                }
                _openRealTime = timenow() - pcr;
                if (verbose >= 0)
                    qDebug() << "Reset _openRealTime to" << fixed << _openRealTime;
            }
            else if (_brakeType == BrakeType::PCRSleep) {
                if (dt > 0 && pcr >= now) {
                    if (verbose >= 1) {
                        qDebug().nospace()
                            << "Sleeping: " << pcr - now << ", dt = " << dt
                            << " = (" << pcr << " - " << _lastPacketTime
                            << ") - (" << now << " - " << _lastRealTime
                            << ")";
                    }
                    usleep((unsigned int)((pcr - now) * 1000000.));
                }
                else {
                    if (verbose >= 1)
                        qDebug() << "Passing.";
                }
            }
            _lastPacketTime = pcr;
            _lastRealTime = timenow() - _openRealTime;
        }
        if (afModified) {
#ifndef TS_PACKET_V2
            packet.updateAdaptationfieldBytes();
#else
            // Try to force a re-generation on next send.
            packetNode->clearEdges();
#endif
        }

        for (auto client : _clients) {
            try {
#ifndef TS_PACKET_V2
                client->queuePacket(packet);
#else
                client->queuePacket(packetNode);
#endif
                // FIXME: client->sendData();
            }
            catch (std::exception &ex) {
                qWarning().nospace()
                    << qPrintable(client->logPrefix()) << " "
                    << "Error sending TS packet to client " << client->id() << ": " << QString(ex.what());
                continue;
            }
        }
    }
    catch (std::exception &ex) {
        qWarning() << "Error processing input bytes as TS packet & sending to clients:" << QString(ex.what());
        return;
    }
}

void StreamServer::shutdown(int sigNum, const QString &sigStr)
{
    if (sigNum > 0) {
        if (verbose >= -1) {
            if (sigStr.isEmpty())
                qInfo() << "Got signal number" << sigNum;
            else
                qInfo() << "Got signal" << qPrintable(sigStr);
        }
    }

    if (_isShuttingDown) {
        qCritical() << "Shutdown called while already shutting down;"
                    << "immediately exiting event loop";
        if (qApp) {
            qApp->exit();
            return;
        }
        else
            qFatal("Shutdown: Can't access application object to exit event loop");
    }

    if (verbose >= 0)
        qInfo() << "Shutting down...";
    _isShuttingDown = true;

    if (verbose >= 1)
        qInfo() << "Shutdown: Closing listening socket...";
    _httpServer->closeListeningSocket();

    if (_clients.length() > 0) {
        if (verbose >= 0)
            qInfo() << "Shutdown: Closing client connections...";
        for (auto client : _clients)
            client->close();
        // Be sure to return to event loop after this!
        if (verbose >= 0)
            qInfo() << "Shutdown: Done requesting close of all client connections";
    }
    else {
        if (verbose >= -1)
            qInfo() << "Shutdown: No clients, exiting event loop";
        if (qApp)
            qApp->exit();
        else
            qFatal("Shutdown: Can't access application object to exit event loop");
    }

    if (verbose >= 1)
        qDebug() << "Shutdown: Returning to caller, expecting to ultimately return to event loop";
}
