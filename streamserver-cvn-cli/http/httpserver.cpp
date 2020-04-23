#include "httpserver.h"

#include "log.h"
#include "humanreadable.h"
#include "httputil.h"
#include "httprequest_netside.h"
#include "httpresponse.h"

#include <string>
#include <exception>

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>

namespace SSCvn {
namespace HTTP {

using log::verbose;
using log::debug_level;


/*
 * Server
 */

class ServerPrivate
{
    QPointer<Server> q_ptr;
    Q_DECLARE_PUBLIC(Server)

    quint16      _listenPort;
    QTcpServer   _listenSocket;
    QStringList  _serverHostWhitelist;

    QSharedPointer<ServerHandler> _defaultHandler;

    quint64 _nextClientID = 1;
    QList<ServerClient*> _clients;

    explicit ServerPrivate(quint16 listenPort, Server *q);
};

ServerPrivate::ServerPrivate(quint16 listenPort, Server *q) : q_ptr(q),
    _listenPort(listenPort)
{
    if (!q_ptr)
        throw std::runtime_error("HTTP server hidden implementation ctor: Back-pointer must not be null");
}


Server::Server(quint16 listenPort, QObject *parent) : QObject(parent),
    d_ptr(new ServerPrivate(listenPort, this))
{
    Q_D(Server);

    connect(&d->_listenSocket, &QTcpServer::newConnection, this, &Server::handleClientConnected);

    if (verbose >= -1)
        qInfo() << "Listening on port" << d->_listenPort << "...";
    if (!d->_listenSocket.listen(QHostAddress::Any, d->_listenPort)) {
        qCritical() << "Error listening on port" << d->_listenPort
                    << "due to" << d->_listenSocket.errorString();
        throw std::runtime_error("HTTP server ctor: Listening on network port failed");
    }
}

Server::~Server()
{

}

quint16 Server::listenPort() const
{
    const Q_D(Server);
    return d->_listenPort;
}

const QStringList &Server::serverHostWhitelist() const
{
    const Q_D(Server);
    return d->_serverHostWhitelist;
}

void Server::setServerHostWhitelist(const QStringList &whitelist)
{
    Q_D(Server);
    if (verbose >= 1)
        qInfo() << "HTTP server: Changing server host white-list from" << d->_serverHostWhitelist << "to" << whitelist;
    d->_serverHostWhitelist = whitelist;
}

QSharedPointer<ServerHandler> Server::defaultHandler() const
{
    const Q_D(Server);
    return d->_defaultHandler;
}

void Server::setDefaultHandler(QSharedPointer<ServerHandler> handler)
{
    Q_D(Server);

    bool hadPrevious = d->_defaultHandler;
    const QString prevName = hadPrevious ? d->_defaultHandler->name() : QString::null;

    bool haveNext = handler;
    const QString nextName = haveNext ? handler->name() : QString::null;

    if (verbose >= 0) {
        if (hadPrevious && haveNext)
            qInfo() << "HTTP server: Replacing default handler" << prevName << "with" << nextName;
        else if (hadPrevious)
            qInfo() << "HTTP server: Revoking default handler" << prevName;
        else
            qInfo() << "HTTP server: Setting default handler" << nextName;
    }

    d->_defaultHandler = handler;
}

const QList<ServerClient*> &Server::clients() const
{
    const Q_D(Server);
    return d->_clients;
}

void Server::handleClientConnected()
{
    Q_D(Server);

    QTcpSocket *socket_ptr = d->_listenSocket.nextPendingConnection();
    if (!socket_ptr) {
        qDebug() << "HTTP server: No next pending connection";
        return;
    }
    if (verbose >= -1) {
        qInfo() << "HTTP server: HTTP client" << d->_nextClientID << "connected:"
                << "From" << socket_ptr->peerAddress()
                << "port" << socket_ptr->peerPort();
    }

    // Set up client object and signal mapping.
    auto *client = new ServerClient(socket_ptr, d->_nextClientID++, this);
    connect(client, &ServerClient::requestReady, this, &Server::processRequest);
    connect(client, &QObject::destroyed, this, &Server::handleClientDestroyed);

    // Store client object in list.
    d->_clients.append(client);

    if (verbose >= 0)
        qInfo() << "HTTP server: HTTP client count:" << d->_clients.length();

    emit clientConnected(client);
}

void Server::handleClientDestroyed(QObject *obj)
{
    if (!obj)
        return;
    Q_D(Server);

    // As it seems that the object's dtor has already run,
    // and in any case we can't cast down to ServerClient*:
    // Cast existing ServerClient* up to QObject* for comparison with obj.
    for (QMutableListIterator<ServerClient*> iter(d->_clients);
         iter.hasNext(); )
    {
        if (static_cast<QObject*>(iter.next()) != obj)
            continue;

        // Update the clients list to no longer point to the destroyed object.
        iter.remove();
        break;
    }

    if (verbose >= 0)
        qInfo() << "HTTP server: Client count:" << d->_clients.length();

    emit clientDestroyed(obj);
}

void Server::processRequest(ServerContext *ctx)
{
    Q_D(Server);

    const QString &ctxLogPrefix(ctx->logPrefix());
    const RequestNetside &request(ctx->request());

    if (verbose >= 0) {
        qInfo().nospace()
                << qPrintable(ctxLogPrefix) << " Processing HTTP client request: "
                << "Method " << request.method() << ", "
                << "Path "   << request.path()   << ", "
                << "HTTP version " << request.httpVersion()
                << "...";

        const HeaderNetside &header(request.header());
        qInfo() << qPrintable(ctxLogPrefix) << "Headers extract:"
                << "Host:"       << header.fieldValues("Host")
                << "User-Agent:" << header.fieldValues("User-Agent");
    }
    if (verbose >= 1) {
        qInfo() << qPrintable(ctxLogPrefix) << "HTTP header:";
        for (const HeaderNetside::Field &headerField : request.header().fields())
            qInfo() << qPrintable(ctxLogPrefix) << headerField;
    }

    const QByteArray &httpVersion(request.httpVersion());
    if (!(httpVersion == "HTTP/1.0" || httpVersion == "HTTP/1.1")) {
        if (verbose >= 0)
            qInfo() << qPrintable(ctxLogPrefix) << "HTTP version not recognized:" << httpVersion;
        ctx->setResponseError(SC_400_BadRequest, "HTTP version not recognized.\n");
        return;
    }

    // TODO: Determine host according to RFC2616 5.2
    QByteArray host;
    const QList<HeaderNetside::Field> hostHeaders = request.header().fields("Host");
    if (hostHeaders.length() > 1) {
        if (verbose >= 0) {
            QDebug info = qInfo();
            info << qPrintable(ctxLogPrefix) << "Multiple HTTP Host headers:";
            for (const HeaderNetside::Field &hostHeader : hostHeaders)
                info << hostHeader.fieldValue;
        }
        ctx->setResponseError(SC_400_BadRequest, "Multiple HTTP Host headers in request.\n");
        return;
    }
    else if (hostHeaders.length() == 1) {
        host = hostHeaders.first().fieldValue;
    }
    else {
        // TODO: For HTTP/1.1 requests, give 400 Bad Request according to RFC2616 14.23
    }

    // Check HTTP host of request.
    const auto &hostWhitelist(d->_serverHostWhitelist);
    if (!hostWhitelist.isEmpty()) {
        QString compareHost = host.toLower();
        if (!compareHost.contains(':'))
            compareHost.append(":80");
        else if (compareHost.endsWith(':'))
            compareHost.append("80");

        bool found = false;
        for (QString hostWhite : hostWhitelist) {
            QString compareHostWhite = hostWhite.toLower();
            if (!compareHostWhite.contains(':'))
                compareHostWhite.append(":80");
            else if (compareHostWhite.endsWith(':'))
                compareHostWhite.append("80");

            if (compareHost == compareHostWhite) {
                found = true;
                break;
            }
        }

        if (!found) {
            if (verbose >= 0)
                qInfo() << qPrintable(ctxLogPrefix) << "HTTP host invalid for this server:" << host;
            ctx->setResponseError(SC_400_BadRequest, "HTTP host invalid for this server\n");
            return;
        }
    }

    const QByteArray &method(request.method());
    if (!(method == "GET" || method == "HEAD")) {
        if (verbose >= 0)
            qInfo() << qPrintable(ctxLogPrefix) << "HTTP method not supported:" << method;
        ctx->setResponseError(SC_400_BadRequest, "HTTP method not supported.\n");
        return;
    }

    // TODO: Decide which handler to set, based on previously registered/configured paths.
    auto handler = ctx->handler();
    if (!handler && d->_defaultHandler)
        handler = d->_defaultHandler;
    if (!handler) {
        if (verbose >= 0)
            qCritical() << qPrintable(ctxLogPrefix) << "No handler set for request!";
        ctx->setResponseError(SC_500_InternalServerError, "No handler set for request.\n");
        return;
    }
    handler->handleRequest(ctx);
}

void Server::closeListeningSocket()
{
    Q_D(Server);
    d->_listenSocket.close();
}


/*
 * ServerClient
 */

class ServerClientPrivate {
    QPointer<ServerClient> q_ptr;
    Q_DECLARE_PUBLIC(ServerClient)

    quint64               _id = 0;
    QString               _logPrefix;
    QDateTime             _createdTimestamp;
    QElapsedTimer         _createdElapsed;

    QPointer<QTcpSocket>  _socket_ptr;
    quint64               _socketBytesReceived = 0;
    quint64               _socketBytesSent = 0;
    bool _isReceiving = true;
    bool _isBufferSendDone = false;
    QByteArray _sendBuf;

    quint64 _nextContextID = 1;
    ServerContext *_currentContext;

    explicit ServerClientPrivate(QTcpSocket *socket, quint64 id, ServerClient *q);

    void _handleDisconnected();
    void _createContext();
    void _receiveData();
    void _sendData();
};

ServerClientPrivate::ServerClientPrivate(QTcpSocket *socket, quint64 id, ServerClient *q) : q_ptr(q),
    _id(id), _createdTimestamp(QDateTime::currentDateTime()),
    _socket_ptr(socket)
{
    const std::string prefix = "HTTP server client hidden implementation ctor: ";

    if (!q_ptr)
        throw std::invalid_argument(prefix + "Back-pointer must not be null");
    if (!_socket_ptr)
        throw std::invalid_argument(prefix + "Socket must not be null");

    _createdElapsed.start();
    _logPrefix = "{HTTPClient" + QString::number(_id) + "}";
}

void ServerClientPrivate::_handleDisconnected()
{
    Q_Q(ServerClient);
    q->deleteLater();

    if (_currentContext) {
        const RequestNetside &request(_currentContext->request());
        if (request.receiveState() != RequestNetside::ReceiveState::Ready) {
            if (verbose >= 0) {
                qInfo() << qPrintable(_logPrefix) << "No valid HTTP request before disconnect!";
                qInfo() << qPrintable(_logPrefix) << "Buffer was"
                        << HumanReadable::Hexdump { request.buf(), true, true, true };
                qInfo() << qPrintable(_logPrefix) << "Header lines buffer was"
                        << HumanReadable::Hexdump { request.headerLinesBuf(), true, true, true };
            }
        }
    }

    if (verbose >= -1) {
        qInfo() << qPrintable(_logPrefix)
                << "Client disconnected:"
                << "From" << _socket_ptr->peerAddress()
                << "port" << _socket_ptr->peerPort();

        qint64 elapsed = _createdElapsed.elapsed();
        qInfo() << qPrintable(_logPrefix)
                << "Client was connected for" << elapsed << "ms"
                << qPrintable("(" + HumanReadable::timeDuration(elapsed) + "),")
                << "since" << _createdTimestamp;

        qInfo() << qPrintable(_logPrefix)
                << "Client transfer statistics:"
                << "Received from client" << _socketBytesReceived << "bytes"
                << qPrintable("(" + HumanReadable::byteCount(_socketBytesReceived) + "),")
                << "sent to client" << _socketBytesSent << "bytes"
                << qPrintable("(" + HumanReadable::byteCount(_socketBytesSent) + ")");
    }
}

void ServerClientPrivate::_createContext()
{
    Q_Q(ServerClient);

    if (verbose >= 0) {
        qInfo() << qPrintable(_logPrefix)
                << "Creating HTTP context" << _nextContextID;
    }

    _currentContext = new ServerContext(q, _nextContextID++);
    // N.B.: Don't pass default handler on here! Will be fallback in Server::processRequest().
}

void ServerClientPrivate::_receiveData()
{
    Q_Q(ServerClient);

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Begin receive data";

    if (!_currentContext)
        _createContext();

    QByteArray buf;
    while (!(buf = _socket_ptr->read(1024)).isEmpty()) {
        _socketBytesReceived += buf.length();
        if (verbose >= 2)
            qInfo() << qPrintable(_logPrefix) << "Received" << buf.length() << "bytes of data,"
                    << "total received" << _socketBytesReceived;
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Received data:" << buf;

        if (!_isReceiving) {
            if (verbose >= 0)
                qInfo() << qPrintable(_logPrefix) << "Unrecognized client data, aborting connection.";
            if (verbose >= 3)
                qInfo() << qPrintable(_logPrefix) << "Unrecognized client data was:" << buf;

            _socket_ptr->abort();
            return;
        }

        RequestNetside &request(_currentContext->request());
        try {
            request.processChunk(buf);
        }
        catch (const std::exception &ex) {
            _isReceiving = false;
            if (verbose >= 0) {
                qInfo() << qPrintable(_logPrefix) << "Unable to parse network bytes as HTTP request:" << ex.what();
                qInfo() << qPrintable(_logPrefix) << "Buffer was"
                        << HumanReadable::Hexdump { request.buf(), true, true, true };
                qInfo() << qPrintable(_logPrefix) << "Header lines buffer was"
                        << HumanReadable::Hexdump { request.headerLinesBuf(), true, true, true };
                qInfo() << qPrintable(_logPrefix) << "Rejected chunk was"
                        << HumanReadable::Hexdump { buf, true, true, true };
            }
            _currentContext->setResponseError(HTTP::SC_400_BadRequest, "Unable to parse HTTP request.\n");
            return;
        }
    }

    // TODO: How to handle error?
    //if (buf.isNull())
    //    _socket_ptr->disconnected();

    // TODO: How to handle EOF?
    //if (buf.isEmpty())
    //    ...;

    if (_currentContext->request().receiveState() == HTTP::RequestNetside::ReceiveState::Ready) {
        _isReceiving = false;
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Received request; raising ready signal...";
        emit q->requestReady(_currentContext);

        if (!_currentContext->response()) {
            if (verbose >= 0)
                qCritical() << qPrintable(_logPrefix) << "No response was produced!";
            _currentContext->setResponseError(SC_500_InternalServerError, "No response was produced.\n");
        }

        // Start the process of delivering the response to the client.
        // Without starting it here, the event loop won't be enough...
        _sendData();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish receive data";
}

void ServerClientPrivate::_sendData()
{
    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Begin send data";

    if (_socket_ptr->state() == QTcpSocket::ClosingState) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Socket in closing state, leaving send data early";
        return;
    }

    if (!_currentContext) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Current context missing, leaving send data early";
        return;
    }

    if (!_isBufferSendDone) {
        if (!_currentContext->bufferResponse(_sendBuf))
            _isBufferSendDone = true;
    }

    while (!_sendBuf.isEmpty()) {
        // Try to send.
        qint64 count = _socket_ptr->write(_sendBuf);
        if (count < 0) {
            qInfo() << qPrintable(_logPrefix) << "Write error:" << _socket_ptr->errorString()
                    << ", aborting connection";
            _socket_ptr->abort();
            break;
        }

        _socketBytesSent += count;
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "Sent" << count << "bytes,"
                     << "total sent" << _socketBytesSent;
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Sent data:" << _sendBuf.left(count);

        // No more send is possible.
        if (count == 0)
            break;

        // Remove sent data from the send buffer.
        _sendBuf.remove(0, count);
    }

    if (_sendBuf.isEmpty() && _isBufferSendDone) {
        if (verbose >= 0)
            qInfo() << qPrintable(_logPrefix) << "Closing client connection after HTTP response";
        _socket_ptr->close();
    }

    if (verbose >= 2)
        qDebug() << qPrintable(_logPrefix) << "Finish send data";
}


ServerClient::ServerClient(QTcpSocket *socket, quint64 id, QObject *parent) : QObject(parent),
    d_ptr(new ServerClientPrivate(socket, id, this))
{
    socket->setParent(this);
    connect(socket, &QTcpSocket::disconnected, this, &ServerClient::handleDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ServerClient::receiveData);
    connect(socket, &QTcpSocket::bytesWritten, this, &ServerClient::sendData);
}

ServerClient::~ServerClient()
{

}

Server *ServerClient::parentServer() const
{
    auto *server = qobject_cast<Server*>(parent());
    return server;
}

quint64 ServerClient::id() const
{
    const Q_D(ServerClient);
    return d->_id;
}

const QString &ServerClient::logPrefix() const
{
    const Q_D(ServerClient);
    return d->_logPrefix;
}

QDateTime ServerClient::createdTimestamp() const
{
    const Q_D(ServerClient);
    return d->_createdTimestamp;
}

const QElapsedTimer &ServerClient::createdElapsed() const
{
    const Q_D(ServerClient);
    return d->_createdElapsed;
}

quint64 ServerClient::socketBytesReceived() const
{
    const Q_D(ServerClient);
    return d->_socketBytesReceived;
}

quint64 ServerClient::socketBytesSent() const
{
    const Q_D(ServerClient);
    return d->_socketBytesSent;
}

QHostAddress ServerClient::peerAddress() const
{
    const Q_D(ServerClient);
    return d->_socket_ptr->peerAddress();
}

quint16 ServerClient::peerPort() const
{
    const Q_D(ServerClient);
    return d->_socket_ptr->peerPort();
}

void ServerClient::handleDisconnected()
{
    Q_D(ServerClient);
    d->_handleDisconnected();
}

void ServerClient::receiveData()
{
    Q_D(ServerClient);
    d->_receiveData();
}

void ServerClient::sendData()
{
    Q_D(ServerClient);
    d->_sendData();
}

void ServerClient::close()
{
    Q_D(ServerClient);
    d->_socket_ptr->close();
}


/*
 * ServerContext
 */

class ServerContextPrivate {
    QPointer<ServerContext> q_ptr;
    Q_DECLARE_PUBLIC(ServerContext)

    quint64               _id = 0;
    QString               _logPrefix;
    QDateTime             _createdTimestamp;
    QElapsedTimer         _createdElapsed;

    QPointer<ServerClient> _client;
    QSharedPointer<ServerHandler> _handler;

    RequestNetside            _request;
    QScopedPointer<Response>  _response_ptr;
    bool                      _responseHeaderSent = false;
    bool _isGenerateResponseBody = false;

    explicit ServerContextPrivate(ServerClient *client, quint64 id, ServerContext *q);

    bool _bufferResponse(QByteArray &buf);
};

ServerContextPrivate::ServerContextPrivate(ServerClient *client, quint64 id, ServerContext *q) : q_ptr(q),
    _id(id), _createdTimestamp(QDateTime::currentDateTime()),
    _client(client)
{
    const std::string prefix = "HTTP server context hidden implementation ctor: ";

    if (!q_ptr)
        throw std::invalid_argument(prefix + "Back-pointer must not be null");
    if (!_client)
        throw std::invalid_argument(prefix + "Client must not be null");

    _createdElapsed.start();
    _logPrefix = "{HTTPClient" + QString::number(_client->id()) + "/HTTPCtx" + QString::number(_id) + "}";
}

bool ServerContextPrivate::_bufferResponse(QByteArray &buf)
{
    Q_Q(ServerContext);

    if (!_response_ptr) {
        if (verbose >= 2)
            qDebug() << qPrintable(_logPrefix) << "No response generated, yet, leaving buffer response early";
        return true;
    }

    if (!_responseHeaderSent) {
        // Initially fill send buffer with HTTP response.

        if (verbose >= 0) {
            qInfo() << qPrintable(_logPrefix) << "Sending server response:"
                    << "HTTP version"   << _response_ptr->httpVersion()
                    << "Status code"    << _response_ptr->statusCode()
                    << "Status message" << _response_ptr->statusMsg();
        }

        const QByteArray response = _response_ptr->toBytes();
        if (verbose >= 3)
            qDebug() << qPrintable(_logPrefix) << "Filling send buffer with response data:" << response;

        buf.append(response);
        _responseHeaderSent = true;
    }

    if (!_isGenerateResponseBody)
        return false;

    int bufSizePrev = -1, bufSize = 0;
    while ((bufSize = buf.size()) > bufSizePrev && bufSize < 1024) {
        bufSizePrev = bufSize;
        if (!q->generateResponseBody(buf))
            return false;
    }
    return true;
}


ServerContext::ServerContext(ServerClient *client, quint64 id, QObject *parent) : QObject(parent),
    d_ptr(new ServerContextPrivate(client, id, this))
{

}

ServerContext::~ServerContext()
{

}

quint64 ServerContext::id() const
{
    const Q_D(ServerContext);
    return d->_id;
}

const QString &ServerContext::logPrefix() const
{
    const Q_D(ServerContext);
    return d->_logPrefix;
}

QDateTime ServerContext::createdTimestamp() const
{
    const Q_D(ServerContext);
    return d->_createdTimestamp;
}

const QElapsedTimer &ServerContext::createdElapsed() const
{
    const Q_D(ServerContext);
    return d->_createdElapsed;
}

ServerClient *ServerContext::client() const
{
    const Q_D(ServerContext);
    return d->_client;
}

QSharedPointer<ServerHandler> ServerContext::handler() const
{
    const Q_D(ServerContext);
    return d->_handler;
}

void ServerContext::setHandler(QSharedPointer<ServerHandler> handler)
{
    Q_D(ServerContext);
    d->_handler = handler;
}

RequestNetside &ServerContext::request()
{
    Q_D(ServerContext);
    return d->_request;
}

const RequestNetside &ServerContext::request() const
{
    const Q_D(ServerContext);
    return d->_request;
}

Response *ServerContext::response() const
{
    const Q_D(ServerContext);
    return d->_response_ptr.data();
}

void ServerContext::setResponse(Response *response)
{
    Q_D(ServerContext);
    d->_response_ptr.reset(response);  // (Takes ownership.)
}

void ServerContext::setResponseError(StatusCode statusCode, const QByteArray &body)
{
    Q_D(ServerContext);
    QScopedPointer<Response> response_ptr(new Response(statusCode, statusMsgFromStatusCode(statusCode)));
    response_ptr->setHeader("Content-Type", "text/plain");
    response_ptr->setBody(body);
    d->_response_ptr.swap(response_ptr);
}

bool ServerContext::bufferResponse(QByteArray &buf)
{
    Q_D(ServerContext);
    return d->_bufferResponse(buf);
}

bool ServerContext::isGenerateResponseBody() const
{
    const Q_D(ServerContext);
    return d->_isGenerateResponseBody;
}

void ServerContext::setGenerateResponseBody(bool generate)
{
    Q_D(ServerContext);
    d->_isGenerateResponseBody = generate;
}


}  // namespace SSCvn::HTTP
}  // namespace SSCvn
