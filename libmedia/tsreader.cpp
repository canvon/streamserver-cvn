#include "tsreader.h"

#include "tspacket.h"

#include <cmath>
#include <stdexcept>
#include <QByteArray>
#include <QPointer>
#include <QFile>
#include <QSocketNotifier>

namespace TS {


//
// TS::BytesReader
//

namespace impl {
class BytesReaderImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    qint64                            _tsPacketSize = TSPacket::lengthBasic;

public:
    explicit BytesReaderImpl(QIODevice *dev) : _devPtr(dev)
    {

    }

    friend BytesReader;
};
}  // namespace TS::impl

BytesReader::BytesReader(QIODevice *dev, QObject *parent) :
    QObject(parent),
    _implPtr(std::make_unique<impl::BytesReaderImpl>(dev))
{
    if (!dev)
        throw std::invalid_argument("TS bytes reader ctor: Device can't be null");

    // Set up signals.
    auto filePtr = dynamic_cast<QFile *>(dev);
    if (filePtr) {
        _implPtr->_notifierPtr = std::make_unique<QSocketNotifier>(filePtr->handle(), QSocketNotifier::Read, this);
        connect(_implPtr->_notifierPtr.get(), &QSocketNotifier::activated, this, &BytesReader::readData);
    }
    else {
        connect(dev, &QIODevice::readyRead, this, &BytesReader::readData);
    }
}

BytesReader::~BytesReader()
{

}

qint64 BytesReader::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void BytesReader::setTSPacketSize(qint64 size)
{
    if (!(size >= TSPacket::lengthBasic))
        throw std::invalid_argument("TS bytes reader: Set TS packet size: Invalid size " + std::to_string(size));

    _implPtr->_tsPacketSize = size;
}

void BytesReader::readData()
{
    if (!_implPtr->_devPtr) {
        emit errorEncountered("Device is gone");
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
                emit errorEncountered(dev.errorString());
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

        emit tsBytesReady(buf);
        buf.clear();
    } while (true);
}


//
// TS::PacketReader
//

namespace impl {
class PacketReaderImpl {
    QPointer<BytesReader>             _bytesReader;
    qint64                            _tsPacketOffset = 0;
    qint64                            _tsPacketCount  = 0;
    int                               _discontSegment = 1;
    bool                              _discontLastPCRValid = false;
    double                            _discontLastPCR;
    friend PacketReader;

public:
    bool checkIsDiscontinuity(const TSPacket &packet);
};
}  // namespace TS::impl

PacketReader::PacketReader(QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<impl::PacketReaderImpl>())
{

}

PacketReader::PacketReader(QIODevice *dev, QObject *parent) :
    PacketReader(parent)
{
    if (!dev)
        throw std::invalid_argument("TS packet reader: dev can't be null");

    auto &bytesReader(_implPtr->_bytesReader);
    bytesReader = new BytesReader(dev, this);

    // Setup signals.
    connect(bytesReader, &BytesReader::tsBytesReady, this, &PacketReader::handleTSBytes);
    connect(bytesReader, &BytesReader::eofEncountered, this, &PacketReader::handleEOF);
    connect(bytesReader, &BytesReader::errorEncountered, this, &PacketReader::handleError);
}

PacketReader::~PacketReader()
{

}

BytesReader *PacketReader::bytesReader() const
{
    return _implPtr->_bytesReader;
}

qint64 PacketReader::tsPacketOffset() const
{
    return _implPtr->_tsPacketOffset;
}

qint64 PacketReader::tsPacketCount() const
{
    return _implPtr->_tsPacketCount;
}

int PacketReader::discontSegment() const
{
    return _implPtr->_discontSegment;
}

double PacketReader::pcrLast() const
{
    return _implPtr->_discontLastPCRValid ? _implPtr->_discontLastPCR : NAN;
}

void PacketReader::handleTSBytes(const QByteArray &bytes)
{
    // Try to interpret as TS packet.
    TSPacket packet(bytes);
    _implPtr->_tsPacketCount++;

    double pcrPrev = pcrLast();
    if (_implPtr->checkIsDiscontinuity(packet)) {
        _implPtr->_discontSegment++;

        emit discontEncountered(pcrPrev);
    }

    const QString errMsg = packet.errorMessage();
    if (!errMsg.isNull()) {
        emit errorEncountered(ErrorKind::TS, errMsg);
    }

    emit tsPacketReady(packet);

    _implPtr->_tsPacketOffset += packet.bytes().length();
}

bool impl::PacketReaderImpl::checkIsDiscontinuity(const TSPacket &packet)
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

void PacketReader::handleEOF()
{
    emit eofEncountered();
}

void PacketReader::handleError(const QString &errorMessage)
{
    emit errorEncountered(ErrorKind::IO, errorMessage);
}


}  // namespace TS
