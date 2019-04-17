#include "tspacketv2.h"

#include "humanreadable.h"
#include "exceptionbuilder.h"
#include <stdexcept>

namespace TS {


quint64 ProgramClockReference::pcrValue() const
{
    return pcrBase.value * pcrBaseFactor + pcrExtension.value;
}

quint64 ProgramClockReference::toNanosecs() const
{
    return pcrValue() * 1000000000LL / systemClockFrequencyHz;
}

double ProgramClockReference::toSecs() const
{
    return static_cast<double>(pcrValue()) / static_cast<double>(systemClockFrequencyHz);
}

QDebug operator<<(QDebug debug, const ProgramClockReference &pcr)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TS::ProgramClockReference(";

    debug <<  "base=" << pcr.pcrBase.value;

    if (!(pcr.reserved1.value == pcr.reserved1FixedValue))
        debug << " reserved=" << pcr.reserved1.value
              << "/0x" << qPrintable(QString::number(pcr.reserved1.value, 16));

    debug << " extension=" << pcr.pcrExtension.value;

    // TODO: Use an output format of, e.g., 01:23:45.67
    debug << " computedSeconds=" << pcr.toSecs();

    debug << ")";
    return debug;
}

BitStream &operator>>(BitStream &bitSource, ProgramClockReference &pcr)
{
    bitSource >> pcr.pcrBase >> pcr.reserved1 >> pcr.pcrExtension;
    return bitSource;
}

BitStream &operator<<(BitStream &bitSink, const ProgramClockReference &pcr)
{
    bitSink << pcr.pcrBase << pcr.reserved1 << pcr.pcrExtension;
    return bitSink;
}


PacketV2::PacketV2()
{

}

bool PacketV2::isSyncByteFixedValue() const
{
    return syncByte.value == syncByteFixedValue;
}

bool PacketV2::isNullPacket() const
{
    return pid.value == pidNullPacket;
}

bool PacketV2::hasAdaptationField() const
{
    return adaptationFieldControl.value == AdaptationFieldControlType::AdaptationFieldOnly ||
           adaptationFieldControl.value == AdaptationFieldControlType::AdaptationFieldThenPayload;
}

bool PacketV2::hasPayload() const
{
    return adaptationFieldControl.value == AdaptationFieldControlType::PayloadOnly ||
           adaptationFieldControl.value == AdaptationFieldControlType::AdaptationFieldThenPayload;
}

QDebug operator<<(QDebug debug, const PacketV2 &packet)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TS::PacketV2(";

    if (packet.isSyncByteFixedValue())
        debug << "syncByte";
    else
        debug << "syncByte=" << packet.syncByte.value;

    debug << " transportErrorIndicator="   << packet.transportErrorIndicator.value;
    debug << " payloadUnitStartIndicator=" << packet.payloadUnitStartIndicator.value;
    debug << " transportPriority="         << packet.transportPriority.value;
    debug << " PID="                       << packet.pid.value;

    if (packet.isNullPacket())
        return debug << " NullPacket)";

    debug << " " << packet.transportScramblingControl.value;
    debug << " " << packet.adaptationFieldControl.value;
    debug << " continuityCounter=" << packet.continuityCounter.value;

    if (packet.hasAdaptationField())
    {
        // Dump adaptation field.
        debug << " " << packet.adaptationField;
    }

    if (packet.hasPayload())
    {
        // Dump payload data.
        debug << " payloadData=" << HumanReadable::Hexdump { packet.payloadDataBytes }.enableAll();
    }

    debug << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const PacketV2::AdaptationField &af)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TS::PacketV2::AdaptationField(";

    debug << "adaptationFieldLength=" << af.adaptationFieldLength.value;
    if (!(af.adaptationFieldLength.value > 0))
        return debug << ")";

    debug << " discontinuityIndicator="            << af.discontinuityIndicator.value;
    debug << " randomAccessIndicator="             << af.randomAccessIndicator.value;
    debug << " elementaryStreamPriorityIndicator=" << af.elementaryStreamPriorityIndicator.value;
    debug << " pcrFlag="                           << af.pcrFlag.value;
    debug << " opcrFlag="                          << af.opcrFlag.value;
    debug << " splicingPointFlag="                 << af.splicingPointFlag.value;
    debug << " transportPrivateDataFlag="          << af.transportPrivateDataFlag.value;
    debug << " adaptationFieldExtensionFlag="      << af.adaptationFieldExtensionFlag.value;

    if (af.pcrFlag)
        debug << " programClockReference=" << af.programClockReference;
    if (af.opcrFlag)
        debug << " originalProgramClockReference=" << af.originalProgramClockReference;

    if (af.splicingPointFlag)
        debug << " spliceCountdown=" << af.spliceCountdown.value;

    if (af.transportPrivateDataFlag)
        debug << " transportPrivateData=" << HumanReadable::Hexdump { af.transportPrivateDataBytes }.enableAll();

    if (af.adaptationFieldExtensionFlag)
        debug << " adaptationFieldExtension=" << HumanReadable::Hexdump { af.adaptationFieldExtensionBytes }.enableAll();

    if (!af.stuffingBytes.isEmpty())
        debug << " stuffingBytes=" << HumanReadable::Hexdump { af.stuffingBytes }.enableAll();

    debug << ")";
    return debug;
}


namespace impl {
class PacketV2ParserImpl {
    int  _prefixLength = 0;

    struct ParseState {
        BitStream  bitSource;
        PacketV2  *packetPtr;
        QString   *errorMessagePtr;
    };

    bool parsePacket(ParseState *statePtr);
    bool parseAdaptationField(ParseState *statePtr);

    friend PacketV2Parser;
};
}

bool impl::PacketV2ParserImpl::parsePacket(ParseState *statePtr)
{
    BitStream &bitSource(statePtr->bitSource);
    PacketV2 &packet(*statePtr->packetPtr);
    QString *errMsgPtr = statePtr->errorMessagePtr;

    {
        const int bytesLeft = bitSource.bytesLeft();
        if (bytesLeft != PacketV2::sizeBasic) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Not enough bytes left to parse packet: Need" << PacketV2::sizeBasic
                    << "but got" << bytesLeft;
            }
            return false;
        }
    }

    try {
        bitSource >> packet.syncByte;
        if (!packet.isSyncByteFixedValue())
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "No sync byte" << HumanReadable::Hexdump { QByteArray(1, PacketV2::syncByteFixedValue) }
                << "-- starts with" << HumanReadable::Hexdump { bitSource.bytes().left(8) }.enableAll());
    }
    catch (std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Error at sync byte:" << ex.what();
        }
        return false;
    }

    try {
        bitSource
            >> packet.transportErrorIndicator
            >> packet.payloadUnitStartIndicator
            >> packet.transportPriority
            >> packet.pid;

        if (packet.isNullPacket())
            // Stop parsing here. Rest can be arbitrarily invalid.
            return true;
    }
    catch (std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Error between transportErrorIndicator and PID:" << ex.what();
        }
        return false;
    }

    try {
        bitSource
            >> packet.transportScramblingControl
            >> packet.adaptationFieldControl
            >> packet.continuityCounter;

        if (packet.transportScramblingControl.value == PacketV2::TransportScramblingControlType::Reserved1)
            throw std::runtime_error("Field transportScramblingControl has reserved value");
        if (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::Reserved1)
            throw std::runtime_error("Field adaptationFieldControl has reserved value");
    }
    catch (std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Error between transportScramblingControl and continuityCounter:" << ex.what();
        }
        return false;
    }

    if (packet.hasAdaptationField())
    {
        try {
            // Parse adaptation field.
            if (!parseAdaptationField(statePtr))
                throw std::runtime_error(!errMsgPtr || errMsgPtr->isEmpty() ?
                    "Parse failed" : qPrintable(*errMsgPtr));
        }
        catch (std::exception &ex) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Error parsing adaptation field:" << ex.what();
            }
            return false;
        }
    }

    if (packet.hasPayload())
    {
        try {
            int N = 184 - (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload ?
                               packet.adaptationField.adaptationFieldLength.value + 1 : 0);
            packet.payloadDataBytes = bitSource.takeByteArrayAligned(N);
        }
        catch (std::exception &ex) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Error extracting payload data:" << ex.what();
            }
            return false;
        }
    }

    if (!bitSource.atEnd()) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Expected end of bit source, but"
                << bitSource.bytesLeft() << "bytes and"
                << bitSource.bitsLeft() << "bits left";
        }
        return false;
    }

    return true;
}

bool impl::PacketV2ParserImpl::parseAdaptationField(ParseState *statePtr)
{
    BitStream &bitSource(statePtr->bitSource);
    PacketV2 &packet(*statePtr->packetPtr);
    PacketV2::AdaptationField &af(packet.adaptationField);

    auto &afLen(af.adaptationFieldLength);
    bitSource >> afLen;
    if (!(afLen.value > 0))
        // (== 0 is used for a single stuffing byte, the adaptation field length)
        return true;

    const QByteArray afBytes = bitSource.takeByteArrayAligned(afLen.value);
    BitStream afBitSource(afBytes);

    afBitSource
        >> af.discontinuityIndicator
        >> af.randomAccessIndicator
        >> af.elementaryStreamPriorityIndicator
        >> af.pcrFlag
        >> af.opcrFlag
        >> af.splicingPointFlag
        >> af.transportPrivateDataFlag
        >> af.adaptationFieldExtensionFlag;

    if (af.pcrFlag)
        afBitSource >> af.programClockReference;

    if (af.opcrFlag)
        afBitSource >> af.originalProgramClockReference;

    if (af.splicingPointFlag)
        afBitSource >> af.spliceCountdown;

    if (af.transportPrivateDataFlag) {
        auto &tpdLen(af.transportPrivateDataLength);
        afBitSource >> tpdLen;
        af.transportPrivateDataBytes = afBitSource.takeByteArrayAligned(tpdLen.value);
    }

    if (af.adaptationFieldExtensionFlag) {
        auto &afeLen(af.adaptationFieldExtensionLength);
        afBitSource >> afeLen;
        af.adaptationFieldExtensionBytes = afBitSource.takeByteArrayAligned(afeLen.value);
    }

    af.stuffingBytes = afBitSource.takeByteArrayAligned(-1);

    if (!afBitSource.atEnd())
        throw static_cast<std::runtime_error>(ExceptionBuilder()
            << "Expected end of bit source, but"
            << afBitSource.bytesLeft() << "bytes and"
            << afBitSource.bitsLeft() << "bits left");

    return true;
}

PacketV2Parser::PacketV2Parser() :
    _implPtr(std::make_unique<impl::PacketV2ParserImpl>())
{

}

PacketV2Parser::~PacketV2Parser()
{

}

int PacketV2Parser::prefixLength() const
{
    return _implPtr->_prefixLength;
}

void PacketV2Parser::setPrefixLength(int len)
{
    if (!(len >= 0))
        throw std::invalid_argument("TS packet v2 parser: Prefix length must be positive-or-zero");

    _implPtr->_prefixLength = len;
}

bool PacketV2Parser::parse(const QByteArray &bytes, PacketV2 *packet, QString *errorMessage)
{
    if (!packet)
        throw std::invalid_argument("TS packet v2 parser: Packet can't be null");

    if (errorMessage)
        errorMessage->clear();

    {
        const int bytesLen = bytes.length();
        const int expectedLen = _implPtr->_prefixLength + PacketV2::sizeBasic;
        if (bytesLen != expectedLen) {
            if (errorMessage) {
                QDebug(errorMessage)
                    << "Expected TS packet size" << expectedLen
                    << "but got" << bytesLen;
            }
            return false;
        }
    }

    // Skip prefix.
    const QByteArray bytesBasic = bytes.mid(_implPtr->_prefixLength);

    impl::PacketV2ParserImpl::ParseState state { bytesBasic, packet, errorMessage };
    return _implPtr->parsePacket(&state);
}

void PacketV2Parser::parse(Upconvert<QByteArray, PacketV2> *upconvert, QString *errorMessage)
{
    if (!upconvert)
        throw std::invalid_argument("TS packet v2 parser: Conversion store can't be null");

    upconvert->success = parse(upconvert->source, &upconvert->result, errorMessage);
}


namespace impl {

class PacketV2GeneratorImpl {
    struct GenerateState {
        const PacketV2 &packet;
        BitStream       bitSink;
        QString        *errorMessagePtr;
    };

    bool generatePacket(GenerateState *statePtr);
    bool generateAdaptationField(GenerateState *statePtr);

    friend PacketV2Generator;
};

bool PacketV2GeneratorImpl::generatePacket(GenerateState *statePtr)
{
    const PacketV2 &packet(statePtr->packet);
    BitStream &bitSink(statePtr->bitSink);
    QString *errMsgPtr = statePtr->errorMessagePtr;

    {
        const int bytesLeft = bitSink.bytesLeft();
        if (bytesLeft != PacketV2::sizeBasic) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Not enough bytes left to generate packet: Need" << PacketV2::sizeBasic
                    << "but got" << bytesLeft;
            }
            return false;
        }
    }

    const int posBegin = bitSink.offsetBytes();

    try {
        if (!packet.isSyncByteFixedValue()) {
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "Invalid sync byte" << packet.syncByte.value);
        }

        bitSink << packet.syncByte;
    }
    catch (const std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr) << "Error at sync byte:" << ex.what();
        }
        return false;
    }

    try {
        bitSink
            << packet.transportErrorIndicator
            << packet.payloadUnitStartIndicator
            << packet.transportPriority
            << packet.pid;

        if (packet.isNullPacket())
            // Stop generating here. Rest will be left uninitialized and/or at zero-bits. (?)
            return true;
    }
    catch (const std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Error between transportErrorIndicator and PID:" << ex.what();
        }
        return false;
    }

    try {
        if (packet.transportScramblingControl.value == PacketV2::TransportScramblingControlType::Reserved1)
            throw std::runtime_error("Field transportScramblingControl has reserved value");
        if (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::Reserved1)
            throw std::runtime_error("Field adaptationFieldControl has reserved value");

        bitSink
            << packet.transportScramblingControl
            << packet.adaptationFieldControl
            << packet.continuityCounter;
    }
    catch (const std::exception &ex) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Error between transportScramblingControl and continuityCounter:" << ex.what();
        }
        return false;
    }

    if (packet.hasAdaptationField()) {
        try {
            // Generate adaptation field.
            if (!generateAdaptationField(statePtr))
                throw std::runtime_error(!errMsgPtr || errMsgPtr->isEmpty() ?
                    "Generate failed" : qPrintable(*errMsgPtr));
        }
        catch (const std::exception &ex) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Error generating adaptation field:" << ex.what();
            }
            return false;
        }
    }

    if (packet.hasPayload())
    {
        try {
            const int N = 184 - (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload ?
                                     packet.adaptationField.adaptationFieldLength.value + 1 : 0);

            const int payloadDataBytesLen = packet.payloadDataBytes.length();
            if (payloadDataBytesLen != N) {
                throw static_cast<std::runtime_error>(ExceptionBuilder()
                    << "Payload data bytes length computed to be" << N << "bytes long,"
                    << "but got" << payloadDataBytesLen << "bytes");
            }

            bitSink.putByteArrayAligned(packet.payloadDataBytes);
        }
        catch (const std::exception &ex) {
            if (errMsgPtr) {
                QDebug(errMsgPtr)
                    << "Error generating payload data:" << ex.what();
            }
            return false;
        }
    }

    const int posEnd = bitSink.offsetBytes();

    if (posEnd - posBegin != PacketV2::sizeBasic) {
        if (errMsgPtr) {
            QDebug(errMsgPtr)
                << "Intended to put" << PacketV2::sizeBasic << "bytes into bit sink, but"
                << "actually put" << (posEnd - posBegin) << "bytes";
        }
        return false;
    }

    return true;
}

bool PacketV2GeneratorImpl::generateAdaptationField(PacketV2GeneratorImpl::GenerateState *statePtr)
{
    const PacketV2                  &packet(statePtr->packet);
    const PacketV2::AdaptationField &af(packet.adaptationField);
    BitStream                       &bitSink(statePtr->bitSink);

    const auto &afLen(af.adaptationFieldLength);
    bitSink << afLen;
    if (!(afLen.value > 0))
        // (== 0 is used for a single stuffing byte, the adaptation field length)
        return true;

    const int posBegin = bitSink.offsetBytes();

    bitSink
        << af.discontinuityIndicator
        << af.randomAccessIndicator
        << af.elementaryStreamPriorityIndicator
        << af.pcrFlag
        << af.opcrFlag
        << af.splicingPointFlag
        << af.transportPrivateDataFlag
        << af.adaptationFieldExtensionFlag;

    if (af.pcrFlag)
        bitSink << af.programClockReference;

    if (af.opcrFlag)
        bitSink << af.originalProgramClockReference;

    if (af.splicingPointFlag)
        bitSink << af.spliceCountdown;

    if (af.transportPrivateDataFlag) {
        const auto &tpdLen(af.transportPrivateDataLength);
        bitSink << tpdLen;

        const auto &tpdBytes(af.transportPrivateDataBytes);

        const int tpdBytesLen = tpdBytes.length();
        if (tpdBytesLen != tpdLen.value) {
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "transportPrivateDataBytes length intended to be" << tpdLen.value
                << "but got" << tpdBytesLen);
        }

        bitSink.putByteArrayAligned(tpdBytes);
    }

    if (af.adaptationFieldExtensionFlag) {
        const auto &afeLen(af.adaptationFieldExtensionLength);
        bitSink << afeLen;

        const auto &afeBytes(af.adaptationFieldExtensionBytes);

        const int afeBytesLen = afeBytes.length();
        if (afeBytesLen != afeLen.value) {
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "adaptationFieldExtensionBytes length intended to be" << afeLen.value
                << "but got" << afeBytesLen);
        }

        bitSink.putByteArrayAligned(afeBytes);
    }

    if (!af.stuffingBytes.isEmpty())
        bitSink.putByteArrayAligned(af.stuffingBytes);


    const int posEnd = bitSink.offsetBytes();

    if (posEnd - posBegin != afLen.value) {
        throw static_cast<std::runtime_error>(ExceptionBuilder()
            << "Intended to put" << afLen.value << "bytes into bit sink, but"
            << "actually put" << (posEnd - posBegin) << "bytes");
    }


    return true;
}

}  // namespace TS::impl


PacketV2Generator::PacketV2Generator() :
    _implPtr(std::make_unique<impl::PacketV2GeneratorImpl>())
{

}

PacketV2Generator::~PacketV2Generator()
{

}

bool PacketV2Generator::generate(const PacketV2 &packet, QByteArray *bytes, QString *errorMessage)
{
    if (!bytes)
        throw std::invalid_argument("TS packet v2 generator: Bytes can't be null");

    if (errorMessage)
        errorMessage->clear();

    impl::PacketV2GeneratorImpl::GenerateState state { packet, QByteArray(PacketV2::sizeBasic, 0x00), errorMessage };

    if (!_implPtr->generatePacket(&state))
        return false;

    bytes->append(state.bitSink.bytes());
    return true;
}


}  // namespace TS
