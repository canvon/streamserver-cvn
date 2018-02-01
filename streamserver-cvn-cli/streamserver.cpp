#include "streamserver.h"

#include <stdexcept>
#include <QDebug>
#include <QCoreApplication>

StreamServer::StreamServer(std::unique_ptr<QFile> &&inputFilePtr, quint16 listenPort, QObject *parent) :
    QObject(parent), _listenPort(listenPort)
{
    _inputFilePtr = std::move(inputFilePtr);
}

quint16 StreamServer::listenPort() const
{
    return _listenPort;
}

QFile &StreamServer::inputFile()
{
    if (!_inputFilePtr)
        throw std::runtime_error("No input file object in stream server");

    return *_inputFilePtr;
}

const QFile &StreamServer::inputFile() const
{
    if (!_inputFilePtr)
        throw std::runtime_error("No input file object in stream server");

    return *_inputFilePtr;
}

void StreamServer::initInput()
{
    if (!_inputFilePtr->isOpen()) {
        const QString fileName = _inputFilePtr->fileName();

        qInfo() << Q_FUNC_INFO
                << "Opening input file" << fileName << "...";

        if (!_inputFilePtr->open(QFile::ReadOnly)) {
            const QString err = _inputFilePtr->errorString();
            qCritical() << Q_FUNC_INFO
                        << "Can't open input file" << fileName
                        << "due to" << err
                        << ", excepting.";
            //qApp->exit(1);  // We can't always do this, as the main loop maybe has not started yet, so exiting it will have no effect!
            throw std::runtime_error("Can't open input file \"" + fileName.toStdString() + "\": " + err.toStdString());
        }
    }
}
