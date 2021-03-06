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
#ifndef TS_PACKET_V2
    bool                              _tsStripAdditionalInfo = false;
#else
    PacketV2Generator                 _tsGenerator;
#endif
    qint64                            _tsPacketOffset = 0;
    qint64                            _tsPacketCount  = 0;
    friend Writer;

public:
    explicit WriterImpl(QIODevice *dev) : _devPtr(dev)
    {

    }

    QString positionString() const;
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

const QString Writer::positionString() const
{
    return _implPtr->positionString();
}

#ifndef TS_PACKET_V2
bool Writer::tsStripAdditionalInfo() const
{
    return _implPtr->_tsStripAdditionalInfo;
}

void Writer::setTSStripAdditionalInfo(bool strip)
{
    _implPtr->_tsStripAdditionalInfo = strip;
}
#else
PacketV2Generator &Writer::tsGenerator() const
{
    return _implPtr->_tsGenerator;
}

bool Writer::tsStripAdditionalInfo() const
{
    return _implPtr->_tsGenerator.prefixLength() == 0;
}

void Writer::setTSStripAdditionalInfo(bool strip)
{
    if (strip)
        _implPtr->_tsGenerator.setPrefixLength(0);
}
#endif

int Writer::queueTSPacket(const QSharedPointer<ConversionNode<Packet>> &packetNode)
{
#ifndef TS_PACKET_V2
    const auto bytesNodeElements = packetNode->findOtherFormat<QByteArray>();
    for (const auto &bytesNodeElement : bytesNodeElements) {
        if (!_implPtr->_tsStripAdditionalInfo || bytesNodeElement.node->data.length() == TSPacket::lengthBasic)
        {
            _implPtr->_tsPacketCount++;
            return _implPtr->queueBytes(bytesNodeElement.node->data);
        }
    }

    // No optimization found, generate from meaning-accessible representation.
    return queueTSPacket(packetNode->data);
#else
    PacketV2Generator &generator(_implPtr->_tsGenerator);
    QSharedPointer<ConversionNode<QByteArray>> bytesNode;
    QString errMsg;
    if (!generator.generate(packetNode, &bytesNode, &errMsg))
        throw static_cast<std::runtime_error>(ExceptionBuilder() << "TS writer: Error converting packet to bytes:" << errMsg);

    _implPtr->_tsPacketCount++;
    return _implPtr->queueBytes(bytesNode->data);
#endif
}

int Writer::queueTSPacket(const Packet &packet)
{
#ifndef TS_PACKET_V2
    _implPtr->_tsPacketCount++;
    return _implPtr->queueBytes(_implPtr->_tsStripAdditionalInfo ?
        packet.toBasicPacketBytes() :
        packet.bytes()
    );
#else
    PacketV2Generator &generator(_implPtr->_tsGenerator);
    QByteArray bytes;
    QString errMsg;
    if (generator.generate(packet, &bytes, &errMsg)) {
        _implPtr->_tsPacketCount++;
        return _implPtr->queueBytes(bytes);
    }
    else {
        throw std::runtime_error(std::string("TS writer: Error converting packet to bytes: ") + errMsg.toStdString());
    }
#endif
}

QString impl::WriterImpl::positionString() const
{
    QString pos;
    {
        QDebug debug(&pos);
        debug.nospace();
        debug << "[offset=" << _tsPacketOffset;

        if (_tsPacketCount >= 1)
            debug << ", pkg=" << _tsPacketCount;
        else
            debug << ", pkg=(not_started)";

        debug << "]";
    }
    return pos;
}

int impl::WriterImpl::queueBytes(const QByteArray &bytes)
{
    _buf.append(bytes);
    const int bytesQueued = bytes.length();
    _tsPacketOffset += bytesQueued;

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
