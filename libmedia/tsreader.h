#ifndef TSREADER_H
#define TSREADER_H

#include <QObject>

#include "conversionstore.h"
#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif
#include <memory>
#include <QIODevice>

namespace TS {

namespace impl {
class ReaderImpl;
}

class Reader : public QObject
{
    Q_OBJECT
    std::unique_ptr<impl::ReaderImpl>  _implPtr;

public:
    enum class ErrorKind {
        IO,
        TS,
    };
    Q_ENUM(ErrorKind)

    explicit Reader(QIODevice *dev, QObject *parent = 0);
    ~Reader();

    qint64 tsPacketSize() const;
    void setTSPacketSize(qint64 size);
    qint64 tsPacketOffset() const;
    qint64 tsPacketCount() const;
    int discontSegment() const;
    double pcrLast() const;

signals:
    void tsPacketReady(const QSharedPointer<ConversionNode<Packet>> &packetNode);
    void discontEncountered(double pcrPrev);
    void eofEncountered();
    void errorEncountered(ErrorKind errorKind, QString errorMessage);

public slots:
    void readData();
};

}  // namespace TS

#endif // TSREADER_H
