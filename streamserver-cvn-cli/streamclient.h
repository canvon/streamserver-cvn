#ifndef STREAMCLIENT_H
#define STREAMCLIENT_H

#include <QObject>

class StreamClient : public QObject
{
    Q_OBJECT
public:
    explicit StreamClient(QObject *parent = 0);

signals:

public slots:
};

#endif // STREAMCLIENT_H
