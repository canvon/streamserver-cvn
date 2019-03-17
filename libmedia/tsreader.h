#ifndef TSREADER_H
#define TSREADER_H

#include <QObject>

#include <memory>
#include <QByteArray>
#include <QIODevice>

class TSPacket;

namespace TS {


namespace impl {
class BytesReaderImpl;
}

class BytesReader : public QObject
{
    Q_OBJECT
    std::unique_ptr<impl::BytesReaderImpl>  _implPtr;

public:
    explicit BytesReader(QIODevice *dev, QObject *parent = nullptr);
    ~BytesReader();

    qint64 tsPacketSize() const;
    void setTSPacketSize(qint64 size);

signals:
    void tsBytesReady(const QByteArray &bytes);
    void eofEncountered();
    void errorEncountered(const QString &errorMessage);

public slots:
    void readData();
};


namespace impl {
class PacketReaderImpl;
}

class PacketReader : public QObject
{
    Q_OBJECT
    std::unique_ptr<impl::PacketReaderImpl>  _implPtr;

public:
    enum class ErrorKind {
        IO,
        TS,
    };
    Q_ENUM(ErrorKind)

    explicit PacketReader(QObject *parent = nullptr);
    explicit PacketReader(QIODevice *dev, QObject *parent = nullptr);
    ~PacketReader();

    BytesReader *bytesReader() const;
    qint64 tsPacketOffset() const;
    qint64 tsPacketCount() const;
    int discontSegment() const;
    double pcrLast() const;

signals:
    void tsPacketReady(const TSPacket &packet);
    void discontEncountered(double pcrPrev);
    void eofEncountered();
    void errorEncountered(ErrorKind errorKind, QString errorMessage);

public slots:
    void handleTSBytes(const QByteArray &bytes);
    void handleEOF();
    void handleError(const QString &errorMessage);
};


// Compatibility.
using Reader = PacketReader;


}  // namespace TS

#endif // TSREADER_H
