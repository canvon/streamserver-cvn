#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

#include <ctime>
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

    bool                    _isShuttingDown = false;
    quint16                 _listenPort;
    QTcpServer              _listenSocket;
    std::unique_ptr<QFile>  _inputFilePtr;
    QString                 _inputFileName;
    bool                    _inputFileOpenNonblocking = true;
    std::unique_ptr<QSocketNotifier>  _inputFileNotifierPtr;
    int                     _inputFileReopenTimeoutMillisec = 1000;
    int                     _inputConsecutiveErrorCount = 0;
    qint64                  _tsPacketSize = 0;  // Request immediate automatic detection.
    bool                    _tsPacketAutosize = true;
    bool                    _tsStripAdditionalInfoDefault = true;
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

    quint64                               _nextClientID = 1;
    QList<std::shared_ptr<StreamClient>>  _clients;
    QSignalMapper                         _clientDisconnectedMapper;

public:
    explicit StreamServer(std::unique_ptr<QFile> inputFilePtr, quint16 listenPort = 8000, QObject *parent = 0);

    bool         isShuttingDown() const;
    quint16      listenPort() const;
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

public slots:
    void initInputSlot();
    void processInput();
    void shutdown(int sigNum = 0, const QString &sigStr = QString());

private slots:
    void clientConnected();
    void clientDisconnected(QObject *objPtr);
};

#endif // STREAMSERVER_H
