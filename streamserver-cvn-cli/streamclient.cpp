#include "streamclient.h"

StreamClient::StreamClient(std::unique_ptr<QTcpSocket> &&socketPtr, QObject *parent) :
    QObject(parent)
{
    _socketPtr = std::move(socketPtr);
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
