#ifndef TSPACKET_H
#define TSPACKET_H

#include <QObject>

class TSPacket : public QObject
{
    Q_OBJECT
public:
    explicit TSPacket(QObject *parent = 0);

signals:

public slots:
};

#endif // TSPACKET_H
