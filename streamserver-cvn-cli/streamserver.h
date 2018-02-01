#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

#include <memory>
#include <QFile>
#include <QSocketNotifier>

class StreamServer : public QObject
{
    Q_OBJECT

    quint16                 _listenPort;
    std::unique_ptr<QFile>  _inputFilePtr;
    std::unique_ptr<QSocketNotifier>  _inputFileNotifierPtr;
    qint64                  _tsPacketSize = 188;

public:
    explicit StreamServer(std::unique_ptr<QFile> &&inputFilePtr, quint16 listenPort = 8000, QObject *parent = 0);

    quint16      listenPort() const;
    QFile       &inputFile();
    const QFile &inputFile() const;

    void initInput();

signals:

public slots:
    void processInput();
};

#endif // STREAMSERVER_H
