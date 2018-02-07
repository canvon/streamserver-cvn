#include "tspacket.h"

#include <stdexcept>

#include "humanreadable.h"

extern int verbose;

TSPacket::TSPacket(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (_bytes.length() < lengthBasic) {
        QDebug(&_errorMessage)
            << "Invalid packet length" << _bytes.length() << "bytes"
            << "(which is less than basic length" << lengthBasic << "bytes)";
        return;
    }

    int byteIdx = 0;
    if (_bytes.length() == lengthBasic) {
        _type = TypeType::Basic;
    }
    else if (_bytes.length() == 4 + lengthBasic) {
        _type = TypeType::TimeCode;
        byteIdx += 4;
    }
    else {
        QDebug(&_errorMessage) << "Unrecognized packet length" << _bytes.length() << "bytes";
        return;
    }

    {
        _iSyncByte = byteIdx;
        quint8 byte = _bytes.at(byteIdx++);
        if (!(byte == syncByte)) {
            QDebug(&_errorMessage)
                << "No sync byte" << QByteArray(1, syncByte).toHex()
                << "at offset" << _iSyncByte
                << "-- starts with" << HumanReadable::Hexdump { _bytes.left(8) }.enableAll();
            return;
        }
        _validity = ValidityType::SyncByte;
    }

    {
        quint8 byte = _bytes.at(byteIdx++);
        _TEI  = byte & 0x80;
        _PUSI = byte & 0x40;
        _transportPrio = byte & 0x20;

        quint8 byte2 = _bytes.at(byteIdx++);
        _PID = (byte & 0x1f) << 8 | byte2;
        _validity = ValidityType::PID;
    }
    if (_PID == PIDNullPacket)
        return;

    {
        quint8 byte = _bytes.at(byteIdx++);
        _TSC = static_cast<TSCType>((byte & 0xc0) >> 6);
        _adaptationFieldControl = static_cast<AdaptationFieldControlType>((byte & 0x30) >> 4);
        _continuityCounter = (byte & 0x0f);
        _validity = ValidityType::ContinuityCounter;
    }

    _iAdaptationField = byteIdx;
    if (_adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldOnly ||
        _adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        quint8 len = _bytes.at(byteIdx++);

        // Parse adaptation field
        const QByteArray data = _bytes.mid(_iAdaptationField, 1 + len);
        _adaptationField = std::make_shared<AdaptationField>(data);
        const QString &errmsg(_adaptationField->errorMessage());
        if (!errmsg.isNull()) {
            _errorMessage = "Error parsing Adaptation Field: " + errmsg;
            return;
        }
        byteIdx += len;
        if (byteIdx > _bytes.length()) {
            QDebug(&_errorMessage).nospace()
                << "Adaptation Field tries to extend to after packet end"
                << " (offset after AF end would be " << byteIdx << ","
                << " while packet length is " << _bytes.length() << ")";
            return;
        }
        _validity = ValidityType::AdaptationField;
    }

    _iPayloadData = byteIdx;
    if (_adaptationFieldControl == AdaptationFieldControlType::PayloadOnly ||
        _adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        if (!(byteIdx <= _bytes.length())) {
            QDebug(&_errorMessage)
                << "Payload Data offset" << _iPayloadData
                << "is larger than packet size" << _bytes.length();
            return;
        }
        _validity = ValidityType::PayloadData;
    }
}

const QByteArray &TSPacket::bytes() const
{
    return _bytes;
}

TSPacket::ValidityType TSPacket::validity() const
{
    return _validity;
}

const QString &TSPacket::errorMessage() const
{
    return _errorMessage;
}

TSPacket::TypeType TSPacket::type() const
{
    return _type;
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

bool TSPacket::isNullPacket() const
{
    return _validity >= ValidityType::PID && _PID == PIDNullPacket;
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

QByteArray TSPacket::toBasicPacketBytes() const
{
    return _bytes.mid(_iSyncByte, lengthBasic);
}

TSPacket::AdaptationField::AdaptationField(const QByteArray &bytes) :
    _bytes(bytes)
{
    if (_bytes.isEmpty()) {
        _errorMessage = "Can't parse an empty byte array as Adaptation Field";
        return;
    }

    int byteIdx = 0;

    _length = _bytes.at(byteIdx++);
    if (_bytes.length() != 1 + _length) {
        QDebug(&_errorMessage)
            << "Adaptation Field Length" << _length << "+ 1 does not match"
            << "byte array length" << _bytes.length();
        return;
    }

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
        if (!(byteIdx < _bytes.length())) {
            QDebug(&_errorMessage)
                << "Can't read SpliceCountdown, as offset" << byteIdx
                << "is already past the" << _bytes.length() << "bytes of Adaptation Field";
            return;
        }

        _spliceCountdown = _bytes.at(byteIdx++);
        _spliceCountdownValid = true;
    }

    if (_transportPrivateDataFlag) {
        if (!(byteIdx < _bytes.length())) {
            QDebug(&_errorMessage)
                << "Can't read TransportPrivateData, as start offset" << byteIdx
                << "is already past the" << _bytes.length() << "bytes of Adaptation Field";
            return;
        }

        _iTransportPrivateData = byteIdx;
        _transportPrivateDataLength = _bytes.at(byteIdx++);
        _transportPrivateData = _bytes.mid(_iTransportPrivateData, 1 + _transportPrivateDataLength);
        byteIdx += _transportPrivateDataLength;
        if (byteIdx > _bytes.length()) {
            QDebug(&_errorMessage)
                << "Can't finish reading TransportPrivateData, as post-offset" << byteIdx
                << "is such that part of the data would have to be outside of the"
                << _bytes.length() << "bytes of Adaptation Field";
            return;
        }
        _transportPrivateDataValid = true;
    }

    if (_extensionFlag) {
        if (!(byteIdx < _bytes.length())) {
            QDebug(&_errorMessage)
                << "Can't read Extension, as start offset" << byteIdx
                << "is already past the" << _bytes.length() << "bytes of Adaptation Field";
            return;
        }

        _iExtension = byteIdx;
        _extensionLength = _bytes.at(byteIdx++);
        _extensionBytes = _bytes.mid(_iExtension, 1 + _extensionLength);
        byteIdx += _extensionLength;
        if (byteIdx > _bytes.length()) {
            QDebug(&_errorMessage)
                << "Can't finish reading Extension, as post-offset" << byteIdx
                << "is such that part of the data would have to be outside of the"
                << _bytes.length() << "bytes of Adaptation Field";
            return;
        }
        _extensionValid = true;
    }

    _iStuffingBytes = byteIdx;
    _stuffingBytes = _bytes.mid(_iStuffingBytes);
}

const QByteArray &TSPacket::AdaptationField::bytes() const
{
    return _bytes;
}

const QString &TSPacket::AdaptationField::errorMessage() const
{
    return _errorMessage;
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

    if (!packet.errorMessage().isNull())
        debug << "HasError ";

    debug << packet.type();
    if (packet.type() == TSPacket::TypeType::Unrecognized)
        return debug << " Bytes=" << HumanReadable::Hexdump { packet.bytes() }.enableByteCount() << ")";

    debug << " " << packet.validity();
    if (packet.validity() < TSPacket::ValidityType::PID)
        return debug << " Bytes=" << HumanReadable::Hexdump { packet.bytes() }.enableByteCount() << ")";

    debug << " "
        << "TEI="  << packet.TEI()  << " "
        << "PUSI=" << packet.PUSI() << " "
        << "TransportPriority=" << packet.transportPrio() << " "
        << "PID="  << packet.PID();
    if (packet.isNullPacket())
        debug << " NullPacket";
    if (packet.validity() < TSPacket::ValidityType::ContinuityCounter) {
        const QByteArray rest = packet.toBasicPacketBytes().mid(3);  // TODO: How to consistenly compute the offset?, without recomputing byteIdx...
        debug << " RemainingInnerBytes=" << HumanReadable::Hexdump { rest }.enableAll();

        return debug << ")";
    }

    debug << " "
        << packet.TSC()                    << " "
        << packet.adaptationFieldControl() << " "
        << "ContinuityCounter="            << packet.continuityCounter();
    if (packet.validity() < TSPacket::ValidityType::AdaptationField) {
        const QByteArray rest = packet.toBasicPacketBytes().mid(4);  // TODO: (See above.)
        debug << " RemainingInnerBytes=" << HumanReadable::Hexdump { rest }.enableAll();
        return debug << ")";
    }

    auto afPtr = packet.adaptationField();
    if (afPtr)
        debug << " " << *afPtr;
    if (packet.validity() < TSPacket::ValidityType::PayloadData) {
        const QByteArray rest = packet.toBasicPacketBytes().mid(4 + afPtr->bytes().length());  // TODO: See above.)
        debug << " RemainingInnerBytes=" << HumanReadable::Hexdump { rest }.enableAll();
        return debug << ")";
    }

    debug << " " << "PayloadData=" << HumanReadable::Hexdump { packet.payloadData() }.enableByteCount().enableCompressTrailing();

    debug << ")";

    return debug;
}

QDebug operator<<(QDebug debug, const TSPacket::AdaptationField &af)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TSPacket::AdaptationField(";

    if (!af.errorMessage().isNull())
        debug << "HasError ";

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
        debug << " " << "TransportPrivateData=" << HumanReadable::Hexdump { af.transportPrivateData() }.enableAll();
    }

    if (af.extensionFlag()) {
        if (!af.extensionValid())
            return debug << " DataMissingStartingFrom=Extension)";
        debug << " " << "ExtensionBytes=" << HumanReadable::Hexdump { af.extensionBytes() }.enableByteCount().enableCompressTrailing();
    }

    const QByteArray &stuffingBytes(af.stuffingBytes());
    if (!stuffingBytes.isEmpty()) {
        // If non-compressible: Secret message for bored technicians..?
        debug << " StuffingBytes=" << HumanReadable::Hexdump { stuffingBytes }.enableAll();
    }

    debug << ")";

    return debug;
}
