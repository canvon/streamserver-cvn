#ifndef TSWRITER_H
#define TSWRITER_H

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
class WriterImpl;
}

class Writer : public QObject
{
    Q_OBJECT
    std::unique_ptr<impl::WriterImpl>  _implPtr;

public:
    explicit Writer(QIODevice *dev, QObject *parent = 0);
    ~Writer();

    bool tsStripAdditionalInfo() const;
    void setTSStripAdditionalInfo(bool strip);

signals:
    void errorEncountered(QString errorMessage);

public slots:
    void queueTSPacket(const Upconvert<QByteArray, Packet> &packetUpconvert);
    void queueTSPacket(const ConversionNode<Packet> &packetNode);
    void queueTSPacket(const Packet &packet);
    void writeData();
};

}  // namespace TS

#endif // TSWRITER_H
