#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

#include <memory>
#include <QList>
#include <QFile>
#include <QSignalMapper>
#include <QSocketNotifier>
#include <QTimer>
#include <QTcpServer>

#include "streamclient.h"

class StreamServer : public QObject
{
    Q_OBJECT

    quint16                 _listenPort;
    QTcpServer              _listenSocket;
    std::unique_ptr<QFile>  _inputFilePtr;
    std::unique_ptr<QSocketNotifier>  _inputFileNotifierPtr;
    QTimer                  _inputFileReopenTimer;
    int                     _inputFileReopenTimeoutMillisec = 1000;
    qint64                  _tsPacketSize = 188;

    quint64                               _nextClientID = 1;
    QList<std::shared_ptr<StreamClient>>  _clients;
    QSignalMapper                         _clientDisconnectedMapper;

public:
    explicit StreamServer(std::unique_ptr<QFile> inputFilePtr, quint16 listenPort = 8000, QObject *parent = 0);

    quint16      listenPort() const;
    QFile       &inputFile();
    const QFile &inputFile() const;
    qint64       tsPacketSize() const;
    void         setTSPacketSize(qint64 size);

    void initInput();
    void finalizeInput();

signals:

public slots:
    void processInput();

private slots:
    void clientConnected();
    void clientDisconnected(QObject *objPtr);
};

#endif // STREAMSERVER_H
