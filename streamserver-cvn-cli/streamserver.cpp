#include "streamserver.h"

#include <stdexcept>
#include <functional>
#include <QDebug>
#include <QCoreApplication>

#include "tspacket.h"

extern int verbose;

StreamServer::StreamServer(std::unique_ptr<QFile> inputFilePtr, quint16 listenPort, QObject *parent) :
    QObject(parent),
    _listenPort(listenPort), _listenSocket(this),
    _inputFilePtr(std::move(inputFilePtr)), _inputFileReopenTimer(this),
    _clientDisconnectedMapper(this)
{
    connect(&_listenSocket, &QTcpServer::newConnection, this, &StreamServer::clientConnected);

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
    qInfo() << "Client" << _nextClientID << "connected:"
            << "From" << socketPtr->peerAddress()
            << "port" << socketPtr->peerPort();

    // Set up client object and signal mapping.
    std::shared_ptr<StreamClient> clientPtr(
        new StreamClient(std::move(socketPtr), _nextClientID++, this),
        std::mem_fn(&StreamClient::deleteLater));
    _clientDisconnectedMapper.setMapping(&clientPtr->socket(), clientPtr.get());
    connect(&clientPtr->socket(), &QTcpSocket::disconnected,
            &_clientDisconnectedMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map));

    // Store client object in list.
    _clients.push_back(clientPtr);

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
    qInfo() << "Client" << clientPtr->id() << "disconnected:"
            << "From" << socket.peerAddress()
            << "port" << socket.peerPort();

    // Clean up resources.
    _clientDisconnectedMapper.removeMappings(&socket);

    // Remove client object from list.
    _clients.removeOne(std::shared_ptr<StreamClient>(clientPtr, std::mem_fn(&StreamClient::deleteLater)));

    qInfo() << "Client count:" << _clients.length();
}

void StreamServer::initInput()
{
    qDebug() << "Initializing input";

    if (!_inputFilePtr->isOpen()) {
        const QString fileName = _inputFilePtr->fileName();

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

    qDebug() << "Successfully initialized input";
}

void StreamServer::finalizeInput()
{
    qDebug() << "Finalizing input";

    qInfo() << "Closing input...";
    _inputFilePtr->close();

    // Stop notifier gracefully, otherwise it outputs error messages from the event loop.
    if (_inputFileNotifierPtr) {
        _inputFileNotifierPtr->setEnabled(false);
        _inputFileNotifierPtr.reset();
    }

    qDebug() << "Successfully finalized input";
}

void StreamServer::processInput()
{
    QByteArray packetBytes = _inputFilePtr->read(_tsPacketSize);
    if (packetBytes.isNull()) {
        qInfo() << "EOF on input, finalizing...";
        finalizeInput();

        qInfo() << "Setting up timer to open input again";
        _inputFileReopenTimer.singleShot(_inputFileReopenTimeoutMillisec,
            this, &StreamServer::initInput);

        return;
    }
    if (verbose >= 3)
        qDebug() << "Read data:" << packetBytes;

    if (packetBytes.length() != _tsPacketSize)
        throw std::runtime_error("Desync: Read packet should be size " + std::to_string(_tsPacketSize) +
                                 ", but was " + std::to_string(packetBytes.length()));

    // Actually process the read data.
    TSPacket packet(packetBytes);
    for (auto client : _clients) {
        client->queuePacket(packet);
        client->sendData();
    }
}
