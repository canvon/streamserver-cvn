#ifndef TSPACKET_H
#define TSPACKET_H

#include <QObject>

#include <QByteArray>

class TSPacket : public QObject
{
    Q_OBJECT

    QByteArray  _bytes;

public:
    explicit TSPacket(const QByteArray &bytes, QObject *parent = 0);

    static const char syncByte = '\x47';

signals:

public slots:
};

#endif // TSPACKET_H
