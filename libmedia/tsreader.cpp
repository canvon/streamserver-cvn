#include "tsreader.h"

#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif

#include <cmath>
#include <stdexcept>
#include <QByteArray>
#include <QPointer>
#include <QFile>
#include <QSocketNotifier>

namespace TS {

namespace impl {
class ReaderImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    qint64                            _tsPacketSize
#ifndef TS_PACKET_V2
        = TSPacket::lengthBasic;
#else
        = PacketV2::sizeBasic;
    PacketV2Parser                    _tsParser;
#endif
    qint64                            _tsPacketOffset = 0;
    qint64                            _tsPacketCount  = 0;
    int                               _discontSegment = 1;
    bool                              _discontLastPCRValid = false;
    double                            _discontLastPCR;
    friend Reader;

public:
    explicit ReaderImpl(QIODevice *dev) : _devPtr(dev)
    {

    }

    bool checkIsDiscontinuity(const Packet &packet);
};
}  // namespace TS::impl

Reader::Reader(QIODevice *dev, QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<impl::ReaderImpl>(dev))
{
    if (!dev)
        throw std::invalid_argument("TS reader ctor: Device can't be null");

    // Set up signals.
    auto filePtr = dynamic_cast<QFile *>(dev);
    if (filePtr) {
        _implPtr->_notifierPtr = std::make_unique<QSocketNotifier>(filePtr->handle(), QSocketNotifier::Read, this);
        connect(_implPtr->_notifierPtr.get(), &QSocketNotifier::activated, this, &Reader::readData);
    }
    else {
        connect(dev, &QIODevice::readyRead, this, &Reader::readData);
    }
}

Reader::~Reader()
{

}

#ifdef TS_PACKET_V2
PacketV2Parser &Reader::tsParser() const
{
    return _implPtr->_tsParser;
}
#endif

qint64 Reader::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void Reader::setTSPacketSize(qint64 size)
{
#ifndef TS_PACKET_V2
    if (!(size >= TSPacket::lengthBasic))
#else
    if (!(size >= PacketV2::sizeBasic))
#endif
        throw std::invalid_argument("TS reader: Set TS packet size: Invalid size " + std::to_string(size));

    _implPtr->_tsPacketSize = size;
#ifdef TS_PACKET_V2
    _implPtr->_tsParser.setPrefixLength(size - PacketV2::sizeBasic);
#endif
}

qint64 Reader::tsPacketOffset() const
{
    return _implPtr->_tsPacketOffset;
}

qint64 Reader::tsPacketCount() const
{
    return _implPtr->_tsPacketCount;
}

int Reader::discontSegment() const
{
    return _implPtr->_discontSegment;
}

double Reader::pcrLast() const
{
    return _implPtr->_discontLastPCRValid ? _implPtr->_discontLastPCR : NAN;
}

void Reader::readData()
{
    if (!_implPtr->_devPtr) {
        emit errorEncountered(ErrorKind::IO, "Device is gone");
        return;
    }
    QIODevice &dev(*_implPtr->_devPtr);
    QByteArray &buf(_implPtr->_buf);

    do {
        int bufLenPrev   = buf.length();
        int bufLenTarget = _implPtr->_tsPacketSize;
        if (bufLenPrev < bufLenTarget) {
            buf.resize(bufLenTarget);
            qint64 readResult = dev.read(buf.data() + bufLenPrev, bufLenTarget - bufLenPrev);
            if (readResult < 0) {
                buf.resize(bufLenPrev);
                emit errorEncountered(ErrorKind::IO, dev.errorString());
                return;
            }
            else if (readResult == 0) {
                buf.resize(bufLenPrev);
                emit eofEncountered();
                return;
            }
            else if (bufLenPrev + readResult < bufLenTarget) {
                // Short read. Guess all we can do is return...
                buf.resize(bufLenPrev + readResult);
                return;
            }

            // A full read!
        }

        // Try to interpret as TS packet.
        auto bytesNode_ptr = QSharedPointer<ConversionNode<QByteArray>>::create(buf);
#ifndef TS_PACKET_V2
        auto packetNode_ptr = QSharedPointer<ConversionNode<TSPacket>>::create(bytesNode_ptr->data);
        const QString errMsg = packetNode_ptr->data.errorMessage();
        const bool success = errMsg.isNull();
        conversionNodeAddEdge(bytesNode_ptr, std::make_tuple(packetNode_ptr));
#else
        QSharedPointer<ConversionNode<PacketV2>> packetNode_ptr;
        QString errMsg;
        const bool success = _implPtr->_tsParser.parse(bytesNode_ptr, &packetNode_ptr, &errMsg);
#endif
        buf.clear();
        _implPtr->_tsPacketCount++;

        double pcrPrev = pcrLast();
        if (packetNode_ptr && _implPtr->checkIsDiscontinuity(packetNode_ptr->data)) {
            _implPtr->_discontSegment++;

            emit discontEncountered(pcrPrev);
        }

        if (!success) {
            emit errorEncountered(ErrorKind::TS, errMsg);
        }

        if (packetNode_ptr)
            emit tsPacketReady(packetNode_ptr);

        _implPtr->_tsPacketOffset += bytesNode_ptr->data.length();
    } while (true);
}

bool impl::ReaderImpl::checkIsDiscontinuity(const Packet &packet)
{
#ifndef TS_PACKET_V2
    if (!(packet.validity() >= TSPacket::ValidityType::AdaptationField))
        return false;

    auto afControl = packet.adaptationFieldControl();
    if (!(afControl == TSPacket::AdaptationFieldControlType::AdaptationFieldOnly ||
          afControl == TSPacket::AdaptationFieldControlType::AdaptationFieldThenPayload))
        return false;

    auto afPtr = packet.adaptationField();
    if (!afPtr)
        return false;

    if (!afPtr->flagsValid())
        return false;

    if (!afPtr->PCRFlag())
        return false;

    auto pcrPtr = afPtr->PCR();
    if (!pcrPtr)
        return false;
#else
    if (packet.isNullPacket())
        return false;

    if (!packet.hasAdaptationField())
        return false;

    const auto &af(packet.adaptationField);
    if (!af.pcrFlag)
        return false;

    const auto *const pcrPtr = &af.programClockReference;
#endif

    // We got a valid PCR!
    double pcr = pcrPtr->toSecs();

    // Previous PCR available? If not, just remember the new one for later.
    if (!_discontLastPCRValid) {
        _discontLastPCR = pcr;
        _discontLastPCRValid = true;
        return false;
    }

    double lastPCR = _discontLastPCR;
    _discontLastPCR = pcr;

    if (lastPCR <= pcr && pcr <= lastPCR + 1)
        // Everything within parameters.
        return false;

    return true;
}

}  // namespace TS
