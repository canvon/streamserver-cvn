#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

#include <ctime>
#include <memory>
#include <QPointer>
#include <QScopedPointer>
#include <QList>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>

#include "streamclient.h"
#include "http/httpserver.h"

// FIXME: Remove after wrapping in namespace SSCvn:
using namespace SSCvn;


class StreamServer;
class StreamHandlerPrivate;

class StreamHandler : public HTTP::ServerHandler {
    QScopedPointer<StreamHandlerPrivate>  d_ptr;
    Q_DECLARE_PRIVATE(StreamHandler)

public:
    explicit StreamHandler(StreamServer *streamServer);

    QString name() const override;
    void handleRequest(HTTP::ServerContext *ctx) override;
};


class StreamServer : public QObject
{
    Q_OBJECT

    bool                    _isShuttingDown = false;
    QPointer<HTTP::Server>  _httpServer;
    QSharedPointer<StreamHandler>  _httpServerHandler;
    std::unique_ptr<QFile>  _inputFilePtr;
    QString                 _inputFileName;
    bool                    _inputFileOpenNonblocking = true;
    std::unique_ptr<QSocketNotifier>  _inputFileNotifierPtr;
    int                     _inputFileReopenTimeoutMillisec = 1000;
    int                     _inputConsecutiveErrorCount = 0;
    qint64                  _tsPacketSize = 0;  // Request immediate automatic detection.
    bool                    _tsPacketAutosize = true;
    bool                    _tsStripAdditionalInfoDefault = true;
#ifdef TS_PACKET_V2
    TS::PacketV2Parser      _tsParser;
#endif
    bool                    _openRealTimeValid = false;
    double                  _openRealTime = 0;
    double                  _lastRealTime = 0;
    double                  _lastPacketTime = 0;
public:
    enum class BrakeType {
        None,
        PCRSleep,
    };
    Q_ENUM(BrakeType)
private:
    BrakeType               _brakeType = BrakeType::PCRSleep;

    quint64                 _nextClientID = 1;
    QList<StreamClient*>    _clients;

public:
    explicit StreamServer(std::unique_ptr<QFile> inputFilePtr, HTTP::Server *httpServer, QObject *parent = nullptr);

    bool isShuttingDown() const;

    HTTP::Server *httpServer() const;
    StreamClient *client(HTTP::ServerContext *ctx);

    QFile       &inputFile();
    const QFile &inputFile() const;
    bool         inputFileOpenNonblocking() const;
    void         setInputFileOpenNonblocking(bool nonblock);
    int          inputFileReopenTimeoutMillisec() const;
    void         setInputFileReopenTimeoutMillisec(int timeoutMillisec);
    qint64       tsPacketSize() const;
    void         setTSPacketSize(qint64 size);
    bool         tsPacketAutosize() const;
    void         setTSPacketAutosize(bool autosize);
    bool         tsStripAdditionalInfoDefault() const;
    void         setTSStripAdditionalInfoDefault(bool strip);
    BrakeType    brakeType() const;
    void         setBrakeType(BrakeType type);

    void initInput();
    void finalizeInput();

signals:

private slots:
    void handleStreamClientDestroyed(QObject *obj);
    void handleHTTPServerClientDestroyed(QObject *obj);

public slots:
    void initInputSlot();
    void processInput();
    void shutdown(int sigNum = 0, const QString &sigStr = QString());
};

#endif // STREAMSERVER_H
