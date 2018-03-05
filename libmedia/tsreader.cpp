#include "tsreader.h"

#include "tspacket.h"

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
    qint64                            _tsPacketSize = TSPacket::lengthBasic;
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

    bool checkIsDiscontinuity(const TSPacket &packet);
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

qint64 Reader::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void Reader::setTSPacketSize(qint64 size)
{
    if (!(size >= TSPacket::lengthBasic))
        throw std::invalid_argument("TS reader: Set TS packet size: Invalid size " + std::to_string(size));

    _implPtr->_tsPacketSize = size;
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
        TSPacket packet(buf);
        buf.clear();
        _implPtr->_tsPacketCount++;

        if (_implPtr->checkIsDiscontinuity(packet)) {
            _implPtr->_discontSegment++;

            emit discontEncountered();
        }

        const QString errMsg = packet.errorMessage();
        if (!errMsg.isNull()) {
            emit errorEncountered(ErrorKind::TS, errMsg);
        }

        emit tsPacketReady(packet);

        _implPtr->_tsPacketOffset += packet.bytes().length();
    } while (true);
}

bool impl::ReaderImpl::checkIsDiscontinuity(const TSPacket &packet)
{
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
