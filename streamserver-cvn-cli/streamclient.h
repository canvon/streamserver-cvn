#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

#include <memory>
#include <QList>
#include <QTcpSocket>

#include "httprequest.h"
#include "tspacket.h"

class StreamClient : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTcpSocket>  _socketPtr;
    HTTPRequest                  _httpRequest;
    bool                         _headerSent = false;
    QList<TSPacket>              _queue;

public:
    explicit StreamClient(std::unique_ptr<QTcpSocket> &&socketPtr, QObject *parent = 0);

    QTcpSocket &socket();
    const QTcpSocket &socket() const;
    const HTTPRequest &httpRequest() const;

    void queuePacket(const TSPacket &packet);
    void sendData();

signals:

public slots:
    void receiveData();
};

#endif // STREAMCLIENT_H
