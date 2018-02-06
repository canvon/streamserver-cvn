#include "tspacket.h"

#include <stdexcept>

extern int verbose;

TSPacket::TSPacket(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (!_bytes.startsWith(syncByte)) {
        if (verbose >= 1)
            qInfo() << "TS packet: Does not start with sync byte" << syncByte << "but" << _bytes.left(4);

        throw std::runtime_error("TS packet: Does not start with sync byte");
    }

    int byteIdx = 1;
    {
        quint8 byte = _bytes.at(byteIdx++);
        _TEI  = byte & 0x80;
        _PUSI = byte & 0x40;
        _transportPrio = byte & 0x20;

        quint8 byte2 = _bytes.at(byteIdx++);
        _PID = (byte & 0x1f) << 8 | byte2;
    }
    {
        quint8 byte = _bytes.at(byteIdx++);
        _TSC = static_cast<TSCType>((byte & 0xc0) >> 6);
        _adaptationFieldControl = static_cast<AdaptationFieldControlType>((byte & 0x30) >> 4);
        _continuityCounter = (byte & 0x0f);
    }

    _iAdaptationField = byteIdx;
    if (_adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldOnly ||
        _adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        quint8 len = _bytes.at(byteIdx++);

        // Parse adaptation field
        const QByteArray data = _bytes.mid(_iAdaptationField, 1 + len);
        _adaptationField = std::make_shared<AdaptationField>(data);
        byteIdx += len;
    }

    _iPayloadData = byteIdx;
}

const QByteArray &TSPacket::bytes() const
{
    return _bytes;
}

bool TSPacket::TEI() const
{
    return _TEI;
}

bool TSPacket::PUSI() const
{
    return _PUSI;
}

bool TSPacket::transportPrio() const
{
    return _transportPrio;
}

quint16 TSPacket::PID() const
{
    return _PID;
}

TSPacket::TSCType TSPacket::TSC() const
{
    return _TSC;
}

TSPacket::AdaptationFieldControlType TSPacket::adaptationFieldControl() const
{
    return _adaptationFieldControl;
}

quint8 TSPacket::continuityCounter() const
{
    return _continuityCounter;
}

std::shared_ptr<const TSPacket::AdaptationField> TSPacket::adaptationField() const
{
    return _adaptationField;
}

QByteArray TSPacket::payloadData() const
{
    return _bytes.mid(_iPayloadData);
}

TSPacket::AdaptationField::AdaptationField(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (_bytes.isEmpty())
        throw std::runtime_error("TS packet, Adaptation Field: Can't parse an empty byte array as Adaptation Field");

    int byteIdx = 0;

    _length = _bytes.at(byteIdx++);
    if (_bytes.length() != 1 + _length)
        throw std::runtime_error("TS packet, Adaptation Field: Adaptation Field Length " +
                                 std::to_string(_length) + " + 1 does not match byte array length " +
                                 std::to_string(_bytes.length()));

    if (!(byteIdx < _bytes.length())) {
        return;
    }
    else {
        quint8 byte = _bytes.at(byteIdx++);
        _discontinuityIndicator   = byte & 0x80;
        _randomAccessIndicator    = byte & 0x40;
        _ESPrioIndicator          = byte & 0x20;
        _PCRFlag                  = byte & 0x10;
        _OPCRFlag                 = byte & 0x08;
        _splicingPointFlag        = byte & 0x04;
        _transportPrivateDataFlag = byte & 0x02;
        _extensionFlag            = byte & 0x01;
        _flagsValid = true;
    }

    if (_PCRFlag) {
        // FIXME: Implement PCR decoding
        // Until then, just skip the field.
        byteIdx += 6;
    }

    if (_OPCRFlag) {
        // FIXME: As above
        byteIdx += 6;
    }

    if (_splicingPointFlag) {
        if (!(byteIdx < _bytes.length()))
            return;

        _spliceCountdown = _bytes.at(byteIdx++);
        _spliceCountdownValid = true;
    }

    if (_transportPrivateDataFlag) {
        if (!(byteIdx < _bytes.length()))
            return;

        _iTransportPrivateData = byteIdx;
        _transportPrivateDataLength = _bytes.at(byteIdx++);
        _transportPrivateData = _bytes.mid(_iTransportPrivateData, 1 + _transportPrivateDataLength);
        byteIdx += _transportPrivateDataLength;
        if (byteIdx > _bytes.length())
            return;
        _transportPrivateDataValid = true;
    }

    if (_extensionFlag) {
        if (!(byteIdx < _bytes.length()))
            return;

        _iExtension = byteIdx;
        _extensionLength = _bytes.at(byteIdx++);
        _extensionBytes = _bytes.mid(_iExtension, 1 + _extensionLength);
        byteIdx += _extensionLength;
        if (byteIdx > _bytes.length())
            return;
        _extensionValid = true;
    }

    _iStuffingBytes = byteIdx;
    _stuffingBytes = _bytes.mid(_iStuffingBytes);
}

const QByteArray &TSPacket::AdaptationField::bytes() const
{
    return _bytes;
}

quint8 TSPacket::AdaptationField::length() const
{
    return _length;
}

bool TSPacket::AdaptationField::flagsValid() const
{
    return _flagsValid;
}

bool TSPacket::AdaptationField::discontinuityIndicator() const
{
    return _discontinuityIndicator;
}

bool TSPacket::AdaptationField::randomAccessIndicator() const
{
    return _randomAccessIndicator;
}

bool TSPacket::AdaptationField::ESPrioIndicator() const
{
    return _ESPrioIndicator;
}

bool TSPacket::AdaptationField::PCRFlag() const
{
    return _PCRFlag;
}

bool TSPacket::AdaptationField::OPCRFlag() const
{
    return _OPCRFlag;
}

bool TSPacket::AdaptationField::splicingPointFlag() const
{
    return _splicingPointFlag;
}

bool TSPacket::AdaptationField::transportPrivateDataFlag() const
{
    return _transportPrivateDataFlag;
}

bool TSPacket::AdaptationField::extensionFlag() const
{
    return _extensionFlag;
}

bool TSPacket::AdaptationField::spliceCountdownValid() const
{
    return _spliceCountdownValid;
}

qint8 TSPacket::AdaptationField::spliceCountdown() const
{
    return _spliceCountdown;
}

bool TSPacket::AdaptationField::transportPrivateDataValid() const
{
    return _transportPrivateDataValid;
}

const QByteArray &TSPacket::AdaptationField::transportPrivateData() const
{
    return _transportPrivateData;
}

bool TSPacket::AdaptationField::extensionValid() const
{
    return _extensionValid;
}

const QByteArray &TSPacket::AdaptationField::extensionBytes() const
{
    return _extensionBytes;
}

const QByteArray &TSPacket::AdaptationField::stuffingBytes() const
{
    return _stuffingBytes;
}

QDebug operator<<(QDebug debug, const TSPacket &packet)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TSPacket(";

    debug
        << "TEI="  << packet.TEI()  << " "
        << "PUSI=" << packet.PUSI() << " "
        << "TransportPriority=" << packet.transportPrio() << " "
        << "PID="  << packet.PID()  << " "
        << "TSC="  << packet.TSC()  << " "
        << "AdaptationFieldControl=" << packet.adaptationFieldControl() << " "
        << "ContinuityCounter="      << packet.continuityCounter();

    auto afPtr = packet.adaptationField();
    if (afPtr)
        debug << " " << "AdaptationField=" << *afPtr;

    debug << " " << "PayloadData=" << packet.payloadData().toHex();

    debug << ")";

    return debug;
}

QDebug operator<<(QDebug debug, const TSPacket::AdaptationField &af)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "AdaptationField(";

    debug << "Length=" << af.length();

    if (!af.flagsValid())
        return debug << " DataMissingStartingFrom=Flags)";
    debug << " "
        << "DiscontinuityIndicator=" << af.discontinuityIndicator() << " "
        << "RandomAccessIndicator="  << af.randomAccessIndicator()  << " "
        << "ElementaryStreamPriorityIndicator=" << af.ESPrioIndicator() << " "
        << "PCRFlag="                << af.PCRFlag()                << " "
        << "OPCRFlag="               << af.OPCRFlag()               << " "
        << "SplicingPointFlag="      << af.splicingPointFlag()      << " "
        << "TransportPrivateDataFlag=" << af.transportPrivateDataFlag() << " "
        << "ExtensionFlag="          << af.extensionFlag();

    // FIXME: Implement
    if (af.PCRFlag())
        debug << " " << "PCR=<not_implemented>";
    if (af.OPCRFlag())
        debug << " " << "OPCR=<not_implemented>";

    if (af.splicingPointFlag()) {
        if (!af.spliceCountdownValid())
            return debug << " DataMissingStartingFrom=SpliceCountdown)";
        debug << " " << "SpliceCountdown=" << af.spliceCountdown();
    }

    if (af.transportPrivateDataFlag()) {
        if (!af.transportPrivateDataValid())
            return debug << " DataMissingStartingFrom=TransportPrivateData)";
        debug << " " << "TransportPrivateData=" << af.transportPrivateData().toHex();
    }

    if (af.extensionFlag()) {
        if (!af.extensionValid())
            return debug << " DataMissingStartingFrom=Extension)";
        debug << " " << "ExtensionBytes=" << af.extensionBytes().toHex();
    }

    const QByteArray &stuffingBytes(af.stuffingBytes());
    if (!stuffingBytes.isEmpty()) {
        auto hasOtherThan = [&](QChar compare) {
            bool found = false;
            for (QChar c : stuffingBytes) {
                if (c != compare) {
                    found = true;
                    break;
                }
            }
            return found;
        };

        if (!hasOtherThan('\xff'))
            debug << " " << "StuffingBytes=" << stuffingBytes.length() << "x\"ff\"";
        else if (!hasOtherThan('\x00'))
            debug << " " << "StuffingBytes=" << stuffingBytes.length() << "x\"00\"";
        else
            // Secret message for bored technicians..?
            debug << " " << "StuffingBytes=" << stuffingBytes.toHex() << "/" << stuffingBytes;
    }

    debug << ")";

    return debug;
}
