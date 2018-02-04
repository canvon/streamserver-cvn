#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

#include <memory>
#include <functional>
#include <QList>
#include <QTcpSocket>

#include "httprequest.h"
#include "httpreply.h"
#include "tspacket.h"

class StreamClient : public QObject
{
    Q_OBJECT

public:
    typedef std::unique_ptr<QTcpSocket, std::function<void(QTcpSocket *)>>  socketPtr_type;
private:
    socketPtr_type               _socketPtr;
    HTTPRequest                  _httpRequest;
    std::unique_ptr<HTTPReply>   _httpReplyPtr;
    bool                         _replyHeaderSent = false;
    bool                         _forwardPackets = false;
    QList<TSPacket>              _queue;
    QByteArray                   _sendBuf;

public:
    explicit StreamClient(socketPtr_type socketPtr, QObject *parent = 0);

    QTcpSocket &socket();
    const QTcpSocket &socket() const;
    const HTTPRequest &httpRequest() const;
    const HTTPReply   *httpReply()   const;

    void queuePacket(const TSPacket &packet);
    void sendData();
    void processRequest();

signals:

public slots:
    void receiveData();
};

#endif // STREAMCLIENT_H
