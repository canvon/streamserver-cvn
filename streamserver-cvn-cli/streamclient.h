#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

#include <memory>
#include <QTcpSocket>

class StreamClient : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTcpSocket>  _socketPtr;

public:
    explicit StreamClient(std::unique_ptr<QTcpSocket> &&socketPtr, QObject *parent = 0);

    QTcpSocket &socket();
    const QTcpSocket &socket() const;

signals:

public slots:
};

#endif // STREAMCLIENT_H
