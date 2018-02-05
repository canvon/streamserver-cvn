#include "streamserver.h"

#include <stdexcept>
#include <functional>
#include <QDebug>
#include <QCoreApplication>

#include "tspacket.h"
#include "humanreadable.h"

extern int verbose;

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

    if (verbose >= -1) {
        qInfo() << "Client" << clientPtr->id() << "disconnected:"
                << "From" << socket.peerAddress()
                << "port" << socket.peerPort();

        qint64 elapsed = clientPtr->createdElapsed().elapsed();
        qInfo() << "Client was connected for" << elapsed << "ms"
                << qPrintable("(" + HumanReadable::timeDuration(elapsed) + "),")
                << "since" << clientPtr->createdTimestamp();

        quint64 received = clientPtr->socketBytesReceived(), sent = clientPtr->socketBytesSent();
        qInfo() << "Client transfer statistics:"
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
}

void StreamServer::initInput()
{
    if (verbose >= 1)
        qInfo() << "Initializing input";

    if (!_inputFilePtr->isOpen()) {
        const QString fileName = _inputFilePtr->fileName();

        if (verbose >= -1)
            qInfo() << "Opening input file" << fileName << "...";

        if (!_inputFilePtr->open(QFile::ReadOnly)) {
            const QString err = _inputFilePtr->errorString();
            qCritical() << "Can't open input file" << fileName
                        << "due to" << err
                        << ", excepting.";
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
    QByteArray packetBytes = _inputFilePtr->read(_tsPacketSize);
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

    if (packetBytes.length() != _tsPacketSize) {
        qWarning() << "Desync: Read packet should be size" << _tsPacketSize
                   << ", but was" << packetBytes.length();
        return;
    }

    // Actually process the read data.
    try {
        TSPacket packet(packetBytes);
        for (auto client : _clients) {
            try {
                client->queuePacket(packet);
                client->sendData();
            }
            catch (std::exception &ex) {
                qWarning() << "Error sending TS packet to client" << client->id() << ":" << QString(ex.what());
                continue;
            }
        }
    }
    catch (std::exception &ex) {
        qWarning() << "Error processing input bytes as TS packet & sending to clients:" << QString(ex.what());
        return;
    }
}
