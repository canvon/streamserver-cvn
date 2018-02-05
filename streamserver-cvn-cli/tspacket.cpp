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

    // TODO: Implement
}

const QByteArray &TSPacket::AdaptationField::bytes() const
{
    return _bytes;
}

quint8 TSPacket::AdaptationField::length() const
{
    return _length;
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
    debug.nospace()
        << "AdaptationField("
        << "Length=" << af.length() << " "
        << "Data="   << af.bytes().toHex()
        << ")";

    return debug;
}
