#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

#include <memory>
#include <QFile>

class StreamServer : public QObject
{
    Q_OBJECT

    quint16                 _listenPort;
    std::unique_ptr<QFile>  _inputFilePtr;

public:
    explicit StreamServer(std::unique_ptr<QFile> &&inputFilePtr, quint16 listenPort = 8000, QObject *parent = 0);

    quint16      listenPort() const;
    QFile       &inputFile();
    const QFile &inputFile() const;

    void initInput();

signals:

public slots:
};

#endif // STREAMSERVER_H
