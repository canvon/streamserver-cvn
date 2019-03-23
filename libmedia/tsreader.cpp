#include "tsreader.h"

#include "tspacket.h"
#include "tspacketv2.h"

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
// TS::PacketReaderBase
//

namespace impl {

class PacketReaderBaseImpl
{
protected:
    QPointer<PacketReaderBase>        _apiPtr;
    QPointer<BytesReader>             _bytesReader;
    qint64                            _tsPacketOffset = 0;
    qint64                            _tsPacketCount  = 0;
    int                               _discontSegment = 1;
    bool                              _discontLastPCRValid = false;
    double                            _discontLastPCR;

    PacketReaderBaseImpl(PacketReaderBase *api = nullptr);
    PacketReaderBaseImpl(QIODevice *dev, PacketReaderBase *api = nullptr);
public:
    virtual ~PacketReaderBaseImpl();

public:
    double pcrLast() const;

    friend PacketReaderBase;
};

PacketReaderBaseImpl::PacketReaderBaseImpl(PacketReaderBase *api) :
    _apiPtr(api)
{

}

PacketReaderBaseImpl::PacketReaderBaseImpl(QIODevice *dev, PacketReaderBase *api) :
    PacketReaderBaseImpl(api)
{
    if (!dev)
        throw std::invalid_argument("TS packet reader base: dev can't be null");

    auto *const q = _apiPtr.data();
    if (!q)
        throw std::runtime_error("TS packet reader base: Missing API back-pointer");

    _bytesReader = new BytesReader(dev, q);

    // Setup signals.
    q->connect(_bytesReader, &BytesReader::tsBytesReady, q, &PacketReaderBase::handleTSBytes);
    q->connect(_bytesReader, &BytesReader::eofEncountered, q, &PacketReaderBase::handleEOF);
    q->connect(_bytesReader, &BytesReader::errorEncountered, q, &PacketReaderBase::handleError);
}

PacketReaderBaseImpl::~PacketReaderBaseImpl()
{

}

double PacketReaderBaseImpl::pcrLast() const
{
    return _discontLastPCRValid ? _discontLastPCR : NAN;
}

}  // namespace TS::impl

PacketReaderBase::PacketReaderBase(impl::PacketReaderBaseImpl &impl, QObject *parent) :
    QObject(parent), _implPtr(&impl)
{

}

PacketReaderBase::~PacketReaderBase()
{

}

BytesReader *PacketReaderBase::bytesReader() const
{
    return _implPtr->_bytesReader;
}

qint64 PacketReaderBase::tsPacketOffset() const
{
    return _implPtr->_tsPacketOffset;
}

qint64 PacketReaderBase::tsPacketCount() const
{
    return _implPtr->_tsPacketCount;
}

int PacketReaderBase::discontSegment() const
{
    return _implPtr->_discontSegment;
}

double PacketReaderBase::pcrLast() const
{
    return _implPtr->pcrLast();
}

void PacketReaderBase::handleEOF()
{
    emit eofEncountered();
}

void PacketReaderBase::handleError(const QString &errorMessage)
{
    emit errorEncountered(ErrorKind::IO, errorMessage);
}


//
// TS::PacketReader
//

namespace impl {

class PacketReaderImpl : public PacketReaderBaseImpl
{
protected:
    inline auto _api()       { return dynamic_cast<      PacketReader *>(_apiPtr.data()); }
    inline auto _api() const { return dynamic_cast<const PacketReader *>(_apiPtr.data()); }

    using PacketReaderBaseImpl::PacketReaderBaseImpl;

public:
    void handleTSBytes(const QByteArray &bytes);
    bool checkIsDiscontinuity(const TSPacket &packet);

    friend PacketReader;
};

void PacketReaderImpl::handleTSBytes(const QByteArray &bytes)
{
    auto *const q = _api();

    // Try to interpret as TS packet.
    TSPacket packet(bytes);
    _tsPacketCount++;

    double pcrPrev = pcrLast();
    if (checkIsDiscontinuity(packet)) {
        _discontSegment++;

        if (q)
            emit q->discontEncountered(pcrPrev);
    }

    const QString errMsg = packet.errorMessage();
    if (!errMsg.isNull()) {
        if (q)
            emit q->errorEncountered(PacketReader::ErrorKind::TS, errMsg);
    }

    if (q)
        emit q->tsPacketReady(packet);

    _tsPacketOffset += packet.bytes().length();
}

bool PacketReaderImpl::checkIsDiscontinuity(const TSPacket &packet)
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

}  // namespace TS::impl

impl::PacketReaderImpl *PacketReader::_impl()
{
    return dynamic_cast<impl::PacketReaderImpl *>(_implPtr.get());
}

const impl::PacketReaderImpl *PacketReader::_impl() const
{
    return dynamic_cast<const impl::PacketReaderImpl *>(_implPtr.get());
}

PacketReader::PacketReader(QObject *parent) :
    PacketReaderBase(*new impl::PacketReaderImpl(this), parent)
{

}

PacketReader::PacketReader(QIODevice *dev, QObject *parent) :
    PacketReaderBase(*new impl::PacketReaderImpl(dev, this), parent)
{

}

PacketReader::~PacketReader()
{

}

void PacketReader::handleTSBytes(const QByteArray &bytes)
{
    auto *const d = _impl();
    if (!d)
        return;

    d->handleTSBytes(bytes);
}


//
// TS::PacketV2Reader
//

namespace impl {

class PacketV2ReaderImpl : public PacketReaderBaseImpl
{
protected:
    std::unique_ptr<PacketV2Parser>  _parserPtr;

    inline auto _api()       { return dynamic_cast<      PacketV2Reader *>(_apiPtr.data()); }
    inline auto _api() const { return dynamic_cast<const PacketV2Reader *>(_apiPtr.data()); }

    PacketV2ReaderImpl(PacketReaderBase *api = nullptr);
    PacketV2ReaderImpl(QIODevice *dev, PacketReaderBase *api = nullptr);

public:
    void handleTSBytes(const QByteArray &bytes);
    bool checkIsDiscontinuity(const PacketV2 &packet);

    friend PacketV2Reader;
};

PacketV2ReaderImpl::PacketV2ReaderImpl(PacketReaderBase *api) :
    PacketReaderBaseImpl(api),
    _parserPtr(std::make_unique<PacketV2Parser>())
{

}

PacketV2ReaderImpl::PacketV2ReaderImpl(QIODevice *dev, PacketReaderBase *api) :
    PacketReaderBaseImpl(dev, api),
    _parserPtr(std::make_unique<PacketV2Parser>())
{

}

void PacketV2ReaderImpl::handleTSBytes(const QByteArray &bytes)
{
    auto *const q = _api();

    // Try to interpret as TS packet v2.
    PacketV2 packet;
    QString errMsg;
    const bool success = _parserPtr->parse(bytes, &packet, &errMsg);
    _tsPacketCount++;

    double pcrPrev = pcrLast();
    if (checkIsDiscontinuity(packet)) {
        _discontSegment++;

        if (q)
            emit q->discontEncountered(pcrPrev);
    }

    if (!success) {
        if (q)
            emit q->errorEncountered(PacketV2Reader::ErrorKind::TS, errMsg);
    }

    if (q)
        emit q->tsPacketV2Ready(packet);

    _tsPacketOffset += bytes.length();
}

bool PacketV2ReaderImpl::checkIsDiscontinuity(const PacketV2 &packet)
{
    if (packet.isNullPacket())
        return false;

    if (!packet.hasAdaptationField())
        return false;

    const auto &af(packet.adaptationField);
    if (!af.pcrFlag)
        return false;

    // We got a valid PCR!
    const auto &pcr(af.programClockReference);
    const double pcrSec = pcr.toSecs();

    // Previous PCR available? If not, just remember the new one for later.
    if (!_discontLastPCRValid) {
        _discontLastPCR = pcrSec;
        _discontLastPCRValid = true;
        return false;
    }

    double lastPCRSec = _discontLastPCR;
    _discontLastPCR = pcrSec;

    if (lastPCRSec <= pcrSec && pcrSec <= lastPCRSec + 1) {
        // Everything within parameters.
        return false;
    }

    // Discontinuity detected.
    return true;
}

}  // namespace TS::impl

impl::PacketV2ReaderImpl *PacketV2Reader::_impl()
{
    return dynamic_cast<impl::PacketV2ReaderImpl *>(_implPtr.get());
}

const impl::PacketV2ReaderImpl *PacketV2Reader::_impl() const
{
    return dynamic_cast<const impl::PacketV2ReaderImpl *>(_implPtr.get());
}

PacketV2Reader::PacketV2Reader(QObject *parent) :
    PacketReaderBase(*new impl::PacketV2ReaderImpl(this), parent)
{

}

PacketV2Reader::PacketV2Reader(QIODevice *dev, QObject *parent) :
    PacketReaderBase(*new impl::PacketV2ReaderImpl(dev, this), parent)
{

}

PacketV2Reader::~PacketV2Reader()
{

}

void PacketV2Reader::handleTSBytes(const QByteArray &bytes)
{
    auto *const d = _impl();
    if (!d)
        return;

    d->handleTSBytes(bytes);
}


}  // namespace TS
