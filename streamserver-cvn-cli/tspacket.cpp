#include "tspacket.h"

#include <stdexcept>
#include <QDebug>

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
        quint8 byte = _bytes.at(byteIdx++);
        _adaptationFieldLen = byte;

        // TODO: Parse adaptation field
        byteIdx += _adaptationFieldLen;
    }
    else
        _adaptationFieldLen = 0;

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

QByteArray TSPacket::adaptationField() const
{
    if (!(_adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldOnly ||
          _adaptationFieldControl == AdaptationFieldControlType::AdaptationFieldThenPayload))
        return QByteArray();

    return _bytes.mid(_iAdaptationField, 1 + _adaptationFieldLen);
}

QByteArray TSPacket::payloadData() const
{
    return _bytes.mid(_iPayloadData);
}

QString TSPacket::toString() const
{
    QString ret;
    QDebug out(&ret);

    out.nospace()
        << "TEI="  << TEI()  << " "
        << "PUSI=" << PUSI() << " "
        << "TransportPriority=" << transportPrio() << " "
        << "PID="  << PID()  << " "
        << "TSC="  << TSC()  << " "
        << "AdaptationFieldControl=" << adaptationFieldControl() << " "
        << "ContinuityCounter="      << continuityCounter()      << " "
        << "AdaptationField="        << adaptationField()        << " "
        << "PayloadData="            << payloadData();

    return ret;
}
