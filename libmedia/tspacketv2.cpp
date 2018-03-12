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

    if (packet.adaptationFieldControl.value == TS::PacketV2::AdaptationFieldControlType::AdaptationFieldOnly ||
        packet.adaptationFieldControl.value == TS::PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        // Dump adaptation field.
        debug << " " << packet.adaptationField;
    }

    if (packet.adaptationFieldControl.value == TS::PacketV2::AdaptationFieldControlType::PayloadOnly ||
        packet.adaptationFieldControl.value == TS::PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload)
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
    int  _tsPacketSize = PacketV2::sizeBasic;

    bool parseAdaptationField(BitStream *bitSourcePtr, PacketV2Parser::Parse *output);

    friend PacketV2Parser;
};
}

PacketV2Parser::PacketV2Parser() :
    _implPtr(std::make_unique<impl::PacketV2ParserImpl>())
{

}

PacketV2Parser::~PacketV2Parser()
{

}

int PacketV2Parser::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void PacketV2Parser::setTSPacketSize(int size)
{
    // FIXME: Implement
    throw std::runtime_error("TS packet v2 parser: Setting TS packet size not implemented, yet");
}

bool PacketV2Parser::parse(const QByteArray &bytes, PacketV2Parser::Parse *output)
{
    if (!output)
        throw std::invalid_argument("TS packet v2 parser: Output can't be null");

    output->errorMessage.clear();
    output->bytes = bytes;
    BitStream bitSource(output->bytes);
    PacketV2 &packet(output->packet);

    if (bytes.length() != _implPtr->_tsPacketSize) {
        QDebug(&output->errorMessage)
            << "Expected TS packet size" << _implPtr->_tsPacketSize
            << "but got" << bytes.length();
        return false;
    }

    try {
        bitSource >> packet.syncByte;
        if (!packet.isSyncByteFixedValue())
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "No sync byte" << HumanReadable::Hexdump { QByteArray(1, PacketV2::syncByteFixedValue) }
                << "-- starts with" << HumanReadable::Hexdump { output->bytes.left(8) }.enableAll());
    }
    catch (std::exception &ex) {
        QDebug(&output->errorMessage)
            << "Error at sync byte:" << ex.what();
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
        QDebug(&output->errorMessage)
            << "Error between transportErrorIndicator and PID:" << ex.what();
        return false;
    }

    try {
        bitSource
            >> packet.transportScramblingControl
            >> packet.adaptationFieldControl
            >> packet.continuityCounter;
    }
    catch (std::exception &ex) {
        QDebug(&output->errorMessage)
            << "Error between transportScramblingControl and continuityCounter:" << ex.what();
        return false;
    }

    if (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldOnly ||
        packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        try {
            // Parse adaptation field.
            if (!_implPtr->parseAdaptationField(&bitSource, output))
                throw std::runtime_error(output->errorMessage.isEmpty() ?
                    "Parse failed" : qPrintable(output->errorMessage));
        }
        catch (std::exception &ex) {
            QDebug(&output->errorMessage)
                << "Error parsing adaptation field:" << ex.what();
            return false;
        }
    }

    if (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::PayloadOnly ||
        packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        try {
            int N = 184 - (packet.adaptationFieldControl.value == PacketV2::AdaptationFieldControlType::AdaptationFieldThenPayload ?
                               packet.adaptationField.adaptationFieldLength.value + 1 : 0);
            packet.payloadDataBytes = bitSource.takeByteArrayAligned(N);
        }
        catch (std::exception &ex) {
            QDebug(&output->errorMessage)
                << "Error extracting payload data:" << ex.what();
            return false;
        }
    }

    if (!bitSource.atEnd()) {
        QDebug(&output->errorMessage)
            << "Expected end of bit source, but"
            << bitSource.bytesLeft() << "bytes and"
            << bitSource.bitsLeft() << "bits left";
        return false;
    }

    return true;
}

bool impl::PacketV2ParserImpl::parseAdaptationField(BitStream *bitSourcePtr, PacketV2Parser::Parse *output)
{
    BitStream &bitSource(*bitSourcePtr);
    PacketV2 &packet(output->packet);
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


}  // namespace TS
