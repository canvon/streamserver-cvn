#include "log.h"
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

using SSCvn::log::verbose;

namespace TS {

namespace impl {
class ReaderImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    QString                           _logPrefix = "{TS::Reader}";
    bool                              _tsPacketAutoSize = true;
    qint64                            _tsPacketSize = 0;
#ifdef TS_PACKET_V2
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

    QString positionString() const;
    qint64 tsPacketSizeEffective() const;
#ifdef TS_PACKET_V2
    bool checkIsReady();
    bool checkIsReady(int bufPacketSize, int bufPrefixLength, int *storeBufPacketCount = nullptr, int *storeBufSyncByteCount = nullptr);
#endif
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

const QString &Reader::logPrefix() const
{
    return _implPtr->_logPrefix;
}

void Reader::setLogPrefix(const QString &prefix)
{
    _implPtr->_logPrefix = prefix;
}

const QString Reader::positionString() const
{
    return _implPtr->positionString();
}

#ifdef TS_PACKET_V2
PacketV2Parser &Reader::tsParser() const
{
    return _implPtr->_tsParser;
}
#endif

bool Reader::tsPacketAutoSize() const
{
    return _implPtr->_tsPacketAutoSize;
}

void Reader::setTSPacketAutoSize(bool autoSize)
{
    _implPtr->_tsPacketAutoSize = autoSize;
}

qint64 Reader::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void Reader::setTSPacketSize(qint64 size)
{
    const QString theLogPrefix = logPrefix(), thePositionString = positionString();

#ifndef TS_PACKET_V2
    if (!(size >= TSPacket::lengthBasic))
#else
    if (!(size >= PacketV2::sizeBasic))
#endif
        throw std::invalid_argument("TS reader: Set TS packet size: Invalid size " + std::to_string(size));

    if (verbose >= 1)
        qInfo() << theLogPrefix << thePositionString << "Setting fixed packet size of" << size << "bytes.";
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

    // Wrap potentially event-driven (Qt event loop-called) code into try-catch block.
    try {

    QIODevice &dev(*_implPtr->_devPtr);
    QByteArray &buf(_implPtr->_buf);

    do {
        const int prePacketSize = _implPtr->tsPacketSizeEffective();
        int bufLenPrev   = buf.length();
        int bufLenTarget = std::max(prePacketSize, bufLenPrev + 4);
        if (bufLenPrev < bufLenTarget) {
            if (verbose >= 3) {
                qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                        << "Trying to read from" << bufLenPrev << "bytes to" << bufLenTarget << "bytes,"
                        << "that is" << (bufLenTarget - bufLenPrev) << "bytes...";
            }
            buf.resize(bufLenTarget);
            qint64 readResult = dev.read(buf.data() + bufLenPrev, bufLenTarget - bufLenPrev);
            if (readResult < 0) {
                buf.resize(bufLenPrev);
                const QString errMsg = dev.errorString();
                if (verbose >= 3) {
                    qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                            << "Got error:" << errMsg;
                }
                emit errorEncountered(ErrorKind::IO, errMsg);
                return;
            }
            else if (readResult == 0) {
                buf.resize(bufLenPrev);
                if (verbose >= 3) {
                    qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                            << "Got end-of-file (EOF).";
                }
                emit eofEncountered();
                return;
            }
            else if (bufLenPrev + readResult < bufLenTarget) {
                // Short read. Guess all we can do is return...
                buf.resize(bufLenPrev + readResult);
                if (verbose >= 3) {
                    qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                            << "Got short read of" << readResult << "bytes.";
                }
                return;
            }

            // A full read!
            if (verbose >= 3) {
                qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                        << "Got a full read.";
            }
        }

#ifdef TS_PACKET_V2
        // Try to support packet size auto-detection & resync after corruption...
        if (!_implPtr->checkIsReady()) {
            if (verbose >= 3) {
                qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                        << "Buffer can't be processed, yet. Continuing read data loop...";
            }
            continue;
        }
#endif

        if (verbose >= 3) {
            qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                    << "Draining buffer...";
        }

        while (drainBuffer());

        if (verbose >= 3) {
            qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                    << "Finished draining buffer.";
        }
    } while (true);

    // End of try block.
    }
    catch (const std::exception &ex) {
        emit errorEncountered(ErrorKind::Unknown, QString("Exception in TS::Reader::readData(): ") + ex.what());
        return;
    }
}

bool Reader::drainBuffer()
{
    QByteArray &buf(_implPtr->_buf);

    const int packetSize = _implPtr->tsPacketSizeEffective();
    if (buf.length() < packetSize) {
        if (verbose >= 3) {
            qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                    << "Drain buffer: Buffer length" << buf.length() << "is smaller than packet size" << packetSize;
        }
        return false;
    }

    // Try to interpret as TS packet.
    if (verbose >= 3) {
        qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                << "Extracting packet size" << packetSize << "bytes from buffer...";
    }
    auto bytesNode_ptr = QSharedPointer<ConversionNode<QByteArray>>::create(buf.left(packetSize));
    buf.remove(0, packetSize);
#ifndef TS_PACKET_V2
    if (verbose >= 3) {
        qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                << "Parsing as TSPacket (V1)...";
    }
    auto packetNode_ptr = QSharedPointer<ConversionNode<TSPacket>>::create(bytesNode_ptr->data);
    const QString errMsg = packetNode_ptr->data.errorMessage();
    const bool success = errMsg.isNull();
    conversionNodeAddEdge(bytesNode_ptr, packetNode_ptr);
#else
    if (verbose >= 3) {
        qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                << "Parsing as TS::PacketV2...";
    }
    QSharedPointer<ConversionNode<PacketV2>> packetNode_ptr;
    QString errMsg;
    const bool success = _implPtr->_tsParser.parse(bytesNode_ptr, &packetNode_ptr, &errMsg);
#endif
    _implPtr->_tsPacketCount++;
    if (verbose >= 3) {
        qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                << (success ? "Successfully" : "Failedly") << "parsed packet.";
    }

    double pcrPrev = pcrLast();
    if (packetNode_ptr && _implPtr->checkIsDiscontinuity(packetNode_ptr->data)) {
        _implPtr->_discontSegment++;
        if (verbose >= 2) {
            qInfo() << qPrintable(_implPtr->_logPrefix) << qPrintable(positionString())
                    << "Detected discontinuity!";
        }

        emit discontEncountered(pcrPrev);
    }

    if (!success) {
        emit errorEncountered(ErrorKind::TS, errMsg);
    }

    if (packetNode_ptr)
        emit tsPacketReady(packetNode_ptr);

    _implPtr->_tsPacketOffset += bytesNode_ptr->data.length();
    return true;
}

QString impl::ReaderImpl::positionString() const
{
    QString pos;
    {
        QDebug debug(&pos);
        debug.nospace();
        debug << "[offset=" << _tsPacketOffset;

        const auto packetCount = _tsPacketCount;
        if (packetCount >= 1)
            debug << ", pkg=" << packetCount;
        else
            debug << ", pkg=(not_started)";

        debug << ", seg="   << _discontSegment;
        debug << "]";
    }
    return pos;
}

qint64 impl::ReaderImpl::tsPacketSizeEffective() const
{
    const qint64 basicPacketSize =
#ifndef TS_PACKET_V2
        TSPacket::lengthBasic;
#else
        PacketV2::sizeBasic;
#endif
    int packetSize = _tsPacketSize;
    if (!(packetSize >= basicPacketSize))
        packetSize = basicPacketSize;
    return packetSize;
}

#ifdef TS_PACKET_V2
bool impl::ReaderImpl::checkIsReady()
{
    int bufPacketSize = tsPacketSizeEffective();
    int bufPrefixLength = _tsParser.prefixLength();

    while (_buf.length() >= bufPacketSize) {
        // Already running at some packet size(, or fixed)?
        if (_tsPacketSize != 0) {
            int bufPacketCount = 0;
            int bufSyncByteCount = 0;
            bool isReadyOldSize = checkIsReady(bufPacketSize, bufPrefixLength, &bufPacketCount, &bufSyncByteCount);

            if (verbose >= 3) {
                qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                        << "Check is ready: Already running at TS packet size" << _tsPacketSize
                        << "with" << bufSyncByteCount << "of" << bufPacketCount << "packets in the buffer"
                        << "starting with sync byte.";
            }

            // While we're not exceeding some arbitrary limit:
            const int limitPacketCount = 16;
            if (bufPacketCount <= limitPacketCount)
                return isReadyOldSize;

            if (verbose >= 2) {
                qWarning() << qPrintable(_logPrefix) << qPrintable(positionString())
                           << "Check is ready: Exceeded packets-in-buffer limit!"
                           << bufPacketCount << "vs" << limitPacketCount;
            }

            // If enabled, maybe just do packet size auto-detection instead of resync.
            if (_tsPacketAutoSize) {
                if (verbose >= 2) {
                    qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                            << "Check is ready: Resetting TS packet size to 0, thus forcing auto-detection...";
                }
                _tsPacketSize = 0;
            }
        }

        // Packet size auto-detection?
        if (_tsPacketSize == 0) {
            if (!_tsPacketAutoSize)
                throw std::runtime_error("TS packet auto-size disabled, but no packet size set!");

            // First, fill buffer up until we should have some packets to look at.
            if (_buf.length() < 16 * TS::PacketV2::sizeBasic)
                return false;

            // Now, look through all the (theoretic) possibilities:
            QList<int> candidatePrefixLengths { 0, 4 };
            QList<int> candidateSuffixLengths { 0, 16, 20 };

            double bestScore = 0.0;
            for (const int testPrefixLength : candidatePrefixLengths) {
                for (const int testSuffixLength : candidateSuffixLengths) {
                    const int testPacketSize = testPrefixLength + TS::PacketV2::sizeBasic + testSuffixLength;

                    int bufPacketCount = 0;
                    int bufSyncByteCount = 0;
                    checkIsReady(testPacketSize, testPrefixLength, &bufPacketCount, &bufSyncByteCount);
                    double score = static_cast<double>(bufSyncByteCount) / static_cast<double>(bufPacketCount);
                    if (verbose >= 3) {
                        qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                                << "TS packet size auto-detection: Score" << score << "for"
                                << "test packet size" << testPacketSize << "with"
                                << "test prefix length" << testPrefixLength << "and"
                                << "test suffix length" << testSuffixLength;
                    }
                    if (score > bestScore) {
                        if (verbose >= 3) {
                            qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                                    << "TS packet size auto-detection: Remembering as best score for now...";
                        }

                        bufPacketSize = testPacketSize;
                        bufPrefixLength = testPrefixLength;
                        bestScore = score;
                    }
                }
            }

            // Do we have a winner? Stick to that packet size for a while
            // and indicate buffer can now be processed.
            if (bestScore >= 0.5) {
                if (verbose >= 1) {
                    qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                            << "TS packet size auto-detection: Final best score is" << bestScore << "and"
                            << "packet size gets set permanently to" << bufPacketSize << "with"
                            << bufPrefixLength << "prefix bytes!";
                }

                _tsPacketSize = bufPacketSize;
                _tsParser.setPrefixLength(bufPrefixLength);
                return true;
            }

            // Otherwise, assume basic case and fall-through to general resync.
            bufPacketSize = TS::PacketV2::sizeBasic;
            bufPrefixLength = 0;
            // *Don't* set the memorized packet size, so that auto-detection is resumed after resync.
            //_tsPacketSize = bufPacketSize;
            //_tsParser.setPrefixLength(bufPrefixLength);

            if (verbose >= 2) {
                qWarning() << qPrintable(_logPrefix) << qPrintable(positionString())
                        << "TS packet size auto-detection failed:"
                        << "Final best score of" << bestScore << "is not enough; reset to"
                        << "packet size" << bufPacketSize << "with"
                        << bufPrefixLength << "prefix length, for the moment.";
            }
        }


        // Need resync. If possible, we should drop bytes until we have a sync byte at the correct position.

        if (verbose >= 2) {
            qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                    << "Trying resync...";
        }

        int syncBytePos1 = _buf.indexOf(TS::PacketV2::syncByteFixedValue);
        if (!(syncBytePos1 >= 0)) {
            // If no sync byte can be found at all, indicate buffer
            // should be processed (with every "packet" parsed being invalid).
            if (verbose >= 1) {
                qWarning() << qPrintable(_logPrefix) << qPrintable(positionString())
                           << "Resync: No first sync byte found, allowing to process buffer as invalid packets...";
            }
            return true;
        }

        int syncBytePos1PlusPacket = _buf.indexOf(TS::PacketV2::syncByteFixedValue, syncBytePos1 + TS::PacketV2::sizeBasic);
        if (!(syncBytePos1PlusPacket >= 0)) {
            // No sync byte belonging to another packet following first
            // sync byte found, can't do any sensible adjustment based on that.
            // Process as invalid packets...
            if (verbose >= 1) {
                qWarning() << qPrintable(_logPrefix) << qPrintable(positionString())
                           << "Resync: No sync byte belonging to another packet following first sync byte found,"
                           << "allowing to process buffer as invalid packets...";
            }
            return true;
        }

        const int syncBytePosDiff = syncBytePos1PlusPacket - syncBytePos1;
        if (syncBytePosDiff == TS::PacketV2::sizeBasic) {
            // Assume we found two consecutive valid packets, and
            // remove garbage before the first one.
            _buf.remove(0, syncBytePos1);
            if (verbose >= 0) {
                qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                        << "Resync: Found two consecutive sync bytes with distance" << syncBytePosDiff
                        << "which is one basic TS packet size!"
                        << "Removed" << syncBytePos1 << "bytes of garbage.";
            }
        }
        else if (syncBytePosDiff == TS::PacketV2::sizeBasic + 4) {
            // Looks like prefix bytes.
            if (syncBytePos1 >= 4) {
                // Remove garbage.
                _buf.remove(0, syncBytePos1 - 4);
                if (verbose >= 0) {
                    qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                            << "Resync: Found two consecutive sync bytes with distance" << syncBytePosDiff
                            << "which is a timecode prefix plus basic TS packet size!"
                            << "Removed" << (syncBytePos1 - 4) << "bytes of garbage.";
                }
            }
            else {
                // This is special. We got a TS with prefix bytes, but
                // not before our first packet. We could either drop
                // the whole packet, or somehow fill up/in (potentially)
                // invalid prefix bytes. ...
                _buf.insert(0, 4 - syncBytePos1, 0x00);
                if (verbose >= 0) {
                    qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                            << "Resync: Found two consecutive sync bytes with distance" << syncBytePosDiff
                            << "which is a timecode prefix plus basic TS packet size!"
                            << "Inserted" << (4 - syncBytePos1) << "bytes of zeroes.";
                }
            }
        }
        else if (syncBytePosDiff == TS::PacketV2::sizeBasic + 16 ||
                 syncBytePosDiff == TS::PacketV2::sizeBasic + 20)
        {
            // These should be forward-error-correction codes. (?)
            // Anyhow, they should just be trailing bytes.
            // Remove garbage before first detected packet...
            _buf.remove(0, syncBytePos1);
            if (verbose >= 0) {
                qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                        << "Resync: Found two consecutive sync bytes with distance" << syncBytePosDiff
                        << "which is a basic TS packet with forward-error-correction size!"
                        << "Removed" << syncBytePos1 << "bytes of garbage.";
            }
        }
        else {
            // Does not look sensible. Maybe it's not a sync byte at all.
            // We could try to randomly drop some packets, but at the lack
            // of clear information, process as invalid packets...
            if (verbose >= 1) {
                qWarning() << qPrintable(_logPrefix) << qPrintable(positionString())
                           << "Resync: Two sync bytes found, but distance" << syncBytePosDiff << "doesn't make sense,"
                           << "allowing to process buffer as invalid packets...";
            }
            return true;
        }

        // Go on with the same process again, until we run out of buffer bytes...
        if (verbose >= 3) {
            qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                    << "Check is ready: Going for a next round...";
        }
    }

    // Ran out of buffer bytes. Indicate buffer can't be processed, yet.
    if (verbose >= 3) {
        qInfo() << qPrintable(_logPrefix) << qPrintable(positionString())
                << "Check is ready: Ran out of buffer bytes.";
    }
    return false;
}

bool impl::ReaderImpl::checkIsReady(int bufPacketSize, int bufPrefixLength, int *storeBufPacketCount, int *storeBufSyncByteCount)
{
    // Exactly one packet read?
    if (_buf.length() == bufPacketSize) {
        if (storeBufPacketCount)
            *storeBufPacketCount = 1;

        // With sync byte at correct position, processing the buffer is ok.
        // Otherwise, delay processing buffer until the missing sync byte can be viewed in context.
        bool hasSyncByte = _buf.at(bufPrefixLength) == TS::PacketV2::syncByteFixedValue;
        if (storeBufSyncByteCount)
            *storeBufSyncByteCount = hasSyncByte ? 1 : 0;
        return hasSyncByte;
    }

    // Otherwise, this might just be one (or a few) corrupted packet(s).
    // When we got more packets that are ok, allow parsing the buffer.
    int bufPacketCount = 0;
    int bufSyncByteCount = 0;
    int bufOffset = 0;
    while (_buf.length() - bufOffset >= bufPacketSize) {
        ++bufPacketCount;
        if (_buf.at(bufOffset + bufPrefixLength) == TS::PacketV2::syncByteFixedValue)
            ++bufSyncByteCount;
        bufOffset += bufPacketSize;
    }

    // Store the computed values if requested by caller.
    if (storeBufPacketCount)
        *storeBufPacketCount = bufPacketCount;
    if (storeBufSyncByteCount)
        *storeBufSyncByteCount = bufSyncByteCount;

    // Out of a whim, let's say that 60% should need to be okay.
    // PercentValue/BaseValue >= 60/100  | *BaseValue, *100
    if (bufSyncByteCount * 100 >= bufPacketCount * 60)
        return true;

    // Otherwise, delay processing buffer until there are enough correct packets.
    return false;
}
#endif

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
