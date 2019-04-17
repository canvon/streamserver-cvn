#include "tswriter.h"

#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif

#include <stdexcept>
#include <QByteArray>
#include <QPointer>
#include <QFile>
#include <QSocketNotifier>

namespace TS {

namespace impl {
class WriterImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    bool                              _tsStripAdditionalInfo = false;
    friend Writer;

public:
    explicit WriterImpl(QIODevice *dev) : _devPtr(dev)
    {

    }

    int queueBytes(const QByteArray &bytes);
};
}  // namespace TS::impl

Writer::Writer(QIODevice *dev, QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<impl::WriterImpl>(dev))
{
    if (!dev)
        throw std::invalid_argument("TS writer ctor: Device can't be null");

    // Set up signals.
    auto filePtr = dynamic_cast<QFile *>(dev);
    if (filePtr) {
        _implPtr->_notifierPtr = std::make_unique<QSocketNotifier>(filePtr->handle(), QSocketNotifier::Write, this);
        connect(_implPtr->_notifierPtr.get(), &QSocketNotifier::activated, this, &Writer::writeData);
    }
    else {
        connect(dev, &QIODevice::bytesWritten, this, &Writer::writeData);
    }
}

Writer::~Writer()
{

}

bool Writer::tsStripAdditionalInfo() const
{
    return _implPtr->_tsStripAdditionalInfo;
}

void Writer::setTSStripAdditionalInfo(bool strip)
{
    _implPtr->_tsStripAdditionalInfo = strip;
}

int Writer::queueTSPacket(const Upconvert<QByteArray, Packet> &packetUpconvert)
{
    if (_implPtr->_tsStripAdditionalInfo && packetUpconvert.source.length()
#ifndef TS_PACKET_V2
            != TSPacket::lengthBasic)
    {
        return queueTSPacket(packetUpconvert.result);
#else
            != PacketV2::sizeBasic)
    {
        // FIXME: Implement!
        throw std::runtime_error("TS writer: Can't queue packet: Strip additional info is not implemented, yet!");
#endif
    }
    else {
        // Optimize re-generation from the parsed data away.
        return _implPtr->queueBytes(packetUpconvert.source);
    }
}

int Writer::queueTSPacket(const QSharedPointer<ConversionNode<Packet>> &packetNode)
{
    const auto bytesNodes_ptrs = packetNode->findOtherFormat<QByteArray>();
    for (const QSharedPointer<ConversionNode<QByteArray>> &bytesNode_ptr : bytesNodes_ptrs) {
        if (!_implPtr->_tsStripAdditionalInfo || bytesNode_ptr->data.length()
#ifndef TS_PACKET_V2
                == TSPacket::lengthBasic)
#else
                == PacketV2::sizeBasic)
#endif
        {
            return _implPtr->queueBytes(bytesNode_ptr->data);
        }
    }

    // No optimization found, generate from meaning-accessible representation.
    return queueTSPacket(packetNode->data);
}

int Writer::queueTSPacket(const Packet &packet)
{
#ifndef TS_PACKET_V2
    return _implPtr->queueBytes(_implPtr->_tsStripAdditionalInfo ?
        packet.toBasicPacketBytes() :
        packet.bytes()
    );
#else
    // TODO: Store the generator for a longer time
    // FIXME: Respect the _tsStripAdditionalInfo!
    PacketV2Generator generator;
    QByteArray bytes;
    QString errMsg;
    if (generator.generate(packet, &bytes, &errMsg))
        return _implPtr->queueBytes(bytes);
    else
        throw std::runtime_error(std::string("TS writer: Error converting packet to bytes: ") + errMsg.toStdString());
#endif
}

int impl::WriterImpl::queueBytes(const QByteArray &bytes)
{
    _buf.append(bytes);
    const int bytesQueued = bytes.length();

    // TODO: Have a maximum amount of data that can be queued.

    if (_notifierPtr)
        _notifierPtr->setEnabled(true);

    return bytesQueued;
}

void Writer::writeData()
{
    if (!_implPtr->_devPtr)
        std::runtime_error("TS writer: Write data: Device is gone!");
    QIODevice  &dev(*_implPtr->_devPtr);
    QByteArray &buf(_implPtr->_buf);

    while (!buf.isEmpty()) {
        // Try to send.
        qint64 writeResult = dev.write(buf);
        if (writeResult < 0) {
            emit errorEncountered(dev.errorString());
            return;
        }
        else if (writeResult == 0) {
            return;
        }
        //else if (writeResult < buf.length()) {
        //    // Short write. Guess all we can do is return... (?)
        //    buf.remove(0, writeResult);
        //    return;
        //}

        buf.remove(0, writeResult);
    }

    // Buffer now is empty.
    if (_implPtr->_notifierPtr)
        _implPtr->_notifierPtr->setEnabled(false);
}

}  // namespace TS
