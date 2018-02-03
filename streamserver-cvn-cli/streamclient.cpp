#include "streamclient.h"

extern int verbose;

StreamClient::StreamClient(std::unique_ptr<QTcpSocket> &&socketPtr, QObject *parent) :
    QObject(parent)
{
    _socketPtr = std::move(socketPtr);

    connect(_socketPtr.get(), &QTcpSocket::readyRead, this, &StreamClient::receiveData);
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

void StreamClient::queuePacket(const TSPacket &packet)
{
    _queue.append(packet);
}

void StreamClient::receiveData()
{
    qDebug() << "Begin receive data";

    QByteArray buf;
    while (!(buf = _socketPtr->read(1024)).isEmpty()) {
        if (_httpRequest.receiveState() == HTTPRequest::ReceiveState::Ready) {
            qInfo() << "Unrecognized data after client request header, excepting.";
            if (verbose >= 3)
                qInfo() << "Unrecognized data was:" << buf;

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
        qInfo() << "Received client request header:"
                << "Method" << _httpRequest.method()
                << "Path"   << _httpRequest.path()
                << "HTTP version" << _httpRequest.httpVersion();
    }

    qDebug() << "Finish receive data";
}
