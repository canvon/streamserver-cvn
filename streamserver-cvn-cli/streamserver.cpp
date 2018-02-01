#include "streamserver.h"

#include <stdexcept>

StreamServer::StreamServer(std::unique_ptr<QFile> &&inputFilePtr, quint16 listenPort, QObject *parent) :
    QObject(parent), _listenPort(listenPort)
{
    _inputFilePtr = std::move(inputFilePtr);
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
