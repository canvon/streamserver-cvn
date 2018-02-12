#include "streamserver.h"

#include <unistd.h>
#include <stdexcept>
#include <functional>
#include <QDebug>
#include <QCoreApplication>

#include "tspacket.h"
#include "humanreadable.h"

extern int verbose;

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

StreamServer::StreamServer(std::unique_ptr<QFile> inputFilePtr, quint16 listenPort, QObject *parent) :
    QObject(parent),
    _listenPort(listenPort), _listenSocket(this),
    _inputFilePtr(std::move(inputFilePtr)), _inputFileReopenTimer(this),
    _clientDisconnectedMapper(this)
{
    connect(&_listenSocket, &QTcpServer::newConnection, this, &StreamServer::clientConnected);

    if (verbose >= -1)
        qInfo() << "Listening on port" << _listenPort << "...";
    if (!_listenSocket.listen(QHostAddress::Any, _listenPort)) {
        qCritical() << "Error listening on port" << _listenPort
                    << "due to" << _listenSocket.errorString();
        throw std::runtime_error("Listening on network port failed");
    }


    connect(&_clientDisconnectedMapper, static_cast<void(QSignalMapper::*)(QObject *)>(&QSignalMapper::mapped),
            this, &StreamServer::clientDisconnected);
}

bool StreamServer::isShuttingDown() const
{
    return _isShuttingDown;
}

quint16 StreamServer::listenPort() const
{
    return _listenPort;
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

void StreamServer::clientConnected()
{
    StreamClient::socketPtr_type socketPtr(_listenSocket.nextPendingConnection(),
                                           std::mem_fn(&QTcpSocket::deleteLater));
    if (!socketPtr) {
        qDebug() << "No next pending connection";
        return;
    }
    if (verbose >= -1) {
        qInfo() << "Client" << _nextClientID << "connected:"
                << "From" << socketPtr->peerAddress()
                << "port" << socketPtr->peerPort();
    }

    // Set up client object and signal mapping.
    std::shared_ptr<StreamClient> clientPtr(
        new StreamClient(std::move(socketPtr), _nextClientID++, this),
        std::mem_fn(&StreamClient::deleteLater));
    clientPtr->setTSStripAdditionalInfo(_tsStripAdditionalInfoDefault);
    _clientDisconnectedMapper.setMapping(&clientPtr->socket(), clientPtr.get());
    connect(&clientPtr->socket(), &QTcpSocket::disconnected,
            &_clientDisconnectedMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map));

    // Store client object in list.
    _clients.push_back(clientPtr);

    if (verbose >= 0)
        qInfo() << "Client count:" << _clients.length();
}

void StreamServer::clientDisconnected(QObject *objPtr)
{
    if (!objPtr) {
        qDebug() << "No object specified";
        return;
    }

    auto *clientPtr = dynamic_cast<StreamClient *>(objPtr);
    if (!clientPtr) {
        qDebug() << "Not a StreamClient";
        return;
    }

    QTcpSocket &socket(clientPtr->socket());
    const QString logPrefix = clientPtr->logPrefix();

    if (verbose >= -1) {
        qInfo() << qPrintable(logPrefix)
                << "Client" << clientPtr->id() << "disconnected:"
                << "From" << socket.peerAddress()
                << "port" << socket.peerPort();

        qint64 elapsed = clientPtr->createdElapsed().elapsed();
        qInfo() << qPrintable(logPrefix)
                << "Client was connected for" << elapsed << "ms"
                << qPrintable("(" + HumanReadable::timeDuration(elapsed) + "),")
                << "since" << clientPtr->createdTimestamp();

        quint64 received = clientPtr->socketBytesReceived(), sent = clientPtr->socketBytesSent();
        qInfo() << qPrintable(logPrefix)
                << "Client transfer statistics:"
                << "Received from client" << received << "bytes"
                << qPrintable("(" + HumanReadable::byteCount(received) + "),")
                << "sent to client" << sent << "bytes"
                << qPrintable("(" + HumanReadable::byteCount(sent) + ")");
    }

    // Clean up resources.
    _clientDisconnectedMapper.removeMappings(&socket);

    // Remove client object from list.
    _clients.removeOne(std::shared_ptr<StreamClient>(clientPtr, std::mem_fn(&StreamClient::deleteLater)));

    if (verbose >= 0)
        qInfo() << "Client count:" << _clients.length();

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
        const QString fileName = _inputFilePtr->fileName();

        if (_tsPacketAutosize)
            _tsPacketSize = 0;  // Request immediate re-detection.
        _openRealTimeValid = false;
        _openRealTime = 0;
        if (verbose >= -1)
            qInfo().nospace() << "Opening input file " << fileName << "...";

        if (!_inputFilePtr->open(QFile::ReadOnly)) {
            const QString err = _inputFilePtr->errorString();
            qCritical().nospace()
                << "Can't open input file " << fileName
                << ": " << qPrintable(err);
            //qApp->exit(1);  // We can't always do this, as the main loop maybe has not started yet, so exiting it will have no effect!
            throw std::runtime_error("Can't open input file \"" + fileName.toStdString() + "\": " + err.toStdString());
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
            qInfo() << "Setting up timer to open input again";
        _inputFileReopenTimer.singleShot(_inputFileReopenTimeoutMillisec,
            this, &StreamServer::initInput);

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
        TSPacket packet(packetBytes);
        if (verbose >= 3)
            qInfo() << "TS packet contents:" << packet;
        const QString &errmsg(packet.errorMessage());
        if (verbose >= 0 && !errmsg.isNull())
            qWarning() << "TS packet error:" << qPrintable(errmsg);
        if (!errmsg.isNull()) {
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

        auto af = packet.adaptationField();
        bool afModified = false;
        if (af && af->PCRFlag() && af->PCR()) {
            double pcr = af->PCR()->toSecs();
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
                bool discontinuityBefore = af->discontinuityIndicator();
                af->setDiscontinuityIndicator(true);
                afModified = true;
                if (verbose >= 0) {
                    qInfo().nospace()
                        << "Discontinuity detected; Discontinuity Indicator was "
                        << discontinuityBefore << ", now set to "
                        << af->discontinuityIndicator();
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
        if (afModified)
            packet.updateAdaptationfieldBytes();

        for (auto client : _clients) {
            try {
                client->queuePacket(packet);
                client->sendData();
            }
            catch (std::exception &ex) {
                qWarning() << qPrintable(client->logPrefix())
                           << "Error sending TS packet to client" << client->id() << ":" << QString(ex.what());
                continue;
            }
        }
    }
    catch (std::exception &ex) {
        qWarning() << "Error processing input bytes as TS packet & sending to clients:" << QString(ex.what());
        return;
    }
}

void StreamServer::shutdown()
{
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
    _listenSocket.close();

    if (_clients.length() > 0) {
        if (verbose >= 0)
            qInfo() << "Shutdown: Closing client connections...";
        for (auto client : _clients)
            client->close();
        // Be sure to return to event loop after this!
    }
    else {
        if (verbose >= -1)
            qInfo() << "Shutdown: No clients, exiting event loop";
        if (qApp)
            qApp->exit();
        else
            qFatal("Shutdown: Can't access application object to exit event loop");
    }
}
