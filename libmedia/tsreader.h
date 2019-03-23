#ifndef TSREADER_H
#define TSREADER_H

#include <QObject>

#include <memory>
#include <QByteArray>
#include <QIODevice>

class TSPacket;

namespace TS {

class PacketV2;


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
class PacketReaderBaseImpl;
class PacketReaderImpl;
class PacketV2ReaderImpl;
}

class PacketReaderBase : public QObject
{
    Q_OBJECT

protected:
    std::unique_ptr<impl::PacketReaderBaseImpl>  _implPtr;

public:
    enum class ErrorKind {
        IO,
        TS,
    };
    Q_ENUM(ErrorKind)

protected:
    explicit PacketReaderBase(impl::PacketReaderBaseImpl &impl, QObject *parent = nullptr);
public:
    virtual ~PacketReaderBase() override;

    BytesReader *bytesReader() const;
    qint64 tsPacketOffset() const;
    qint64 tsPacketCount() const;
    int discontSegment() const;
    double pcrLast() const;

signals:
    void discontEncountered(double pcrPrev);
    void eofEncountered();
    void errorEncountered(ErrorKind errorKind, QString errorMessage);

public slots:
    virtual void handleTSBytes(const QByteArray &bytes) = 0;
    void handleEOF();
    void handleError(const QString &errorMessage);
};

class PacketReader : public PacketReaderBase
{
    Q_OBJECT

protected:
    impl::PacketReaderImpl *_impl();
    const impl::PacketReaderImpl *_impl() const;

public:
    explicit PacketReader(QObject *parent = nullptr);
    explicit PacketReader(QIODevice *dev, QObject *parent = nullptr);
    virtual ~PacketReader() override;

signals:
    void tsPacketReady(const TSPacket &packet);

public slots:
    virtual void handleTSBytes(const QByteArray &bytes) override;
};

class PacketV2Reader : public PacketReaderBase
{
    Q_OBJECT

protected:
    impl::PacketV2ReaderImpl *_impl();
    const impl::PacketV2ReaderImpl *_impl() const;

public:
    explicit PacketV2Reader(QObject *parent = nullptr);
    explicit PacketV2Reader(QIODevice *dev, QObject *parent = nullptr);
    virtual ~PacketV2Reader();

signals:
    void tsPacketV2Ready(const TS::PacketV2 &packet);

public slots:
    virtual void handleTSBytes(const QByteArray &bytes) override;
};


// Compatibility.
using Reader = PacketReader;


}  // namespace TS

#endif // TSREADER_H
