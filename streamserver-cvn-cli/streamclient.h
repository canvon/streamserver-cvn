#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

#include <memory>
#include <functional>
#include <QList>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QElapsedTimer>
#include <QTcpSocket>

#include "http/httprequest_netside.h"
#include "http/httpreply.h"
#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif

class StreamServer;

class StreamClient : public QObject
{
    Q_OBJECT

public:
    typedef std::unique_ptr<QTcpSocket, std::function<void(QTcpSocket *)>>  socketPtr_type;
private:
    quint64                      _id;
    QString                      _logPrefix;
    QDateTime                    _createdTimestamp;
    QElapsedTimer                _createdElapsed;
    socketPtr_type               _socketPtr;
    quint64                      _socketBytesReceived = 0;
    quint64                      _socketBytesSent = 0;
    bool                         _isReceiving = true;
    HTTPRequest                  _httpRequest;
    std::unique_ptr<HTTPReply>   _httpReplyPtr;
    bool                         _replyHeaderSent = false;
    bool                         _forwardPackets = false;
    bool                         _tsStripAdditionalInfo = true;
#ifndef TS_PACKET_V2
    QList<TSPacket>              _queue;
#else
    TS::PacketV2Generator        _tsGenerator;
    QList<QSharedPointer<ConversionNode<TS::PacketV2>>>  _queue;
#endif
    QByteArray                   _sendBuf;

public:
    explicit StreamClient(socketPtr_type socketPtr, quint64 id = 0, QObject *parent = 0);

    StreamServer *parentServer() const;
    quint64 id() const;
    const QString &logPrefix() const;
    QDateTime createdTimestamp() const;
    const QElapsedTimer &createdElapsed() const;

    QTcpSocket &socket();
    const QTcpSocket &socket() const;
    quint64 socketBytesReceived() const;
    quint64 socketBytesSent() const;
    const HTTPRequest &httpRequest() const;
    const HTTPReply   *httpReply()   const;

    bool tsStripAdditionalInfo() const;
    void setTSStripAdditionalInfo(bool strip);
#ifdef TS_PACKET_V2
    TS::PacketV2Generator &tsGenerator();
    const TS::PacketV2Generator &tsGenerator() const;
#endif

#ifndef TS_PACKET_V2
    void queuePacket(const TSPacket &packet);
#else
    void queuePacket(const QSharedPointer<ConversionNode<TS::PacketV2>> &packetNode);
#endif
    void sendData();
    void processRequest();

    void close();

signals:

public slots:
    void receiveData();
};

#endif // STREAMCLIENT_H
