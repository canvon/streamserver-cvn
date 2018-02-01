#ifndef STREAMSERVER_H
#define STREAMSERVER_H

#include <QObject>

class StreamServer : public QObject
{
    Q_OBJECT
public:
    explicit StreamServer(QObject *parent = 0);

signals:

public slots:
};

#endif // STREAMSERVER_H
