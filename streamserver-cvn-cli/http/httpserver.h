#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>

#include "httputil.h"

#include <memory>
#include <QScopedPointer>
#include <QSharedPointer>

class QStringList;
class QByteArray;
class QHostAddress;
class QTcpSocket;
class QDateTime;
class QElapsedTimer;

namespace SSCvn {
namespace HTTP {


class ServerClient;
class ServerContext;
class ServerHandler;
class ServerPrivate;

class Server : public QObject
{
    Q_OBJECT

    QScopedPointer<ServerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Server)

public:
    explicit Server(quint16 listenPort = listenPort_default, QObject *parent = nullptr);
    ~Server();

    static const quint16 listenPort_default = 8000;

    quint16 listenPort() const;

    const QStringList &serverHostWhitelist() const;
    void setServerHostWhitelist(const QStringList &whitelist);

    QSharedPointer<ServerHandler> defaultHandler() const;
    void setDefaultHandler(QSharedPointer<ServerHandler> handler);

    const QList<ServerClient*> &clients() const;

signals:
    void clientConnected(ServerClient *client);
    void clientDestroyed(QObject *obj);

private slots:
    void handleClientConnected();
    void handleClientDestroyed(QObject *obj);

public slots:
    void processRequest(ServerContext *ctx);
    void closeListeningSocket();
};


class ServerClientPrivate;

class ServerClient : public QObject
{
    Q_OBJECT

    QScopedPointer<ServerClientPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ServerClient)

public:
    explicit ServerClient(QTcpSocket *socket, quint64 id = 0, QObject *parent = nullptr);
    ~ServerClient();

    Server *parentServer() const;

    quint64 id() const;
    const QString &logPrefix() const;
    QDateTime createdTimestamp() const;
    const QElapsedTimer &createdElapsed() const;

    quint64 socketBytesReceived() const;
    quint64 socketBytesSent() const;
    QHostAddress peerAddress() const;
    quint16 peerPort() const;

signals:
    void requestReady(ServerContext *ctx);

private slots:
    void handleDisconnected();

public slots:
    void receiveData();
    void sendData();

    void close();
};


class RequestNetside;
class Response;
class ServerContextPrivate;

class ServerContext : public QObject
{
    Q_OBJECT

    QScopedPointer<ServerContextPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ServerContext)

public:
    explicit ServerContext(ServerClient *client, quint64 id = 0, QObject *parent = nullptr);
    ~ServerContext();

    quint64 id() const;
    const QString &logPrefix() const;
    QDateTime createdTimestamp() const;
    const QElapsedTimer &createdElapsed() const;

    ServerClient *client() const;

    QSharedPointer<ServerHandler> handler() const;
    void setHandler(QSharedPointer<ServerHandler> handler);

    RequestNetside &request();
    const RequestNetside &request() const;

    Response *response() const;
    void setResponse(Response *response);
    void setResponseError(StatusCode statusCode, const QByteArray &body);

    bool bufferResponse(QByteArray &buf);
    bool isGenerateResponseBody() const;
    void setGenerateResponseBody(bool generate);

signals:
    // Return value is: "Continue being called?"
    bool generateResponseBody(QByteArray &buf);

public slots:
};


class ServerHandler {
public:
    virtual QString name() const = 0;
    virtual void handleRequest(ServerContext *ctx) = 0;
};


}  // namespace SSCvn::HTTP
}  // namespace SSCvn

#endif // HTTPSERVER_H
