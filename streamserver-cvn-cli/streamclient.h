#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

#include <QPointer>
#include <QList>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QElapsedTimer>

#include "http/httpserver.h"

#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif

// TODO: Wrap into namespace SSCvn ourselves; then, remove this:
using namespace SSCvn;

class StreamServer;

class StreamClient : public QObject
{
    Q_OBJECT

    quint64                      _id;
    QString                      _logPrefix;
    QDateTime                    _createdTimestamp;
    QElapsedTimer                _createdElapsed;
    QPointer<HTTP::ServerContext>  _httpServerContext;
    bool                         _forwardPackets = false;
    bool                         _tsStripAdditionalInfo = true;
#ifndef TS_PACKET_V2
    QList<TSPacket>              _queue;
#else
    TS::PacketV2Generator        _tsGenerator;
    QList<QSharedPointer<ConversionNode<TS::PacketV2>>>  _queue;
#endif

public:
    explicit StreamClient(HTTP::ServerContext *httpServerContext, quint64 id = 0, QObject *parent = 0);

    StreamServer *parentServer() const;
    quint64 id() const;
    const QString &logPrefix() const;
    QDateTime createdTimestamp() const;
    const QElapsedTimer &createdElapsed() const;

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

signals:

private slots:
    bool handleGenerateResponseBody(QByteArray &buf);
    void handleHTTPServerContextDestroyed(QObject *obj);

public slots:
    void processRequest(HTTP::ServerContext *ctx);
    void close();
};

#endif // STREAMCLIENT_H
