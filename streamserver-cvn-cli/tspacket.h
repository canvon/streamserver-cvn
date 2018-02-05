#ifndef TSPACKET_H
#define TSPACKET_H

#include <QObject>

#include <QByteArray>

class TSPacket
{
    Q_GADGET

    QByteArray  _bytes;

    // MPEG-TS packet contents:
    bool        _TEI, _PUSI, _transportPrio;
    quint16     _PID;
public:
    enum class TSCType {
        NotScrambled,
        Reserved1,
        ScrambledEvenKey,
        ScrambledOddKey,
    };
    Q_ENUM(TSCType)
private:
    TSCType     _TSC;
public:
    enum class AdaptationFieldControlType {
        Reserved1,
        PayloadOnly,
        AdaptationFieldOnly,
        AdaptationFieldThenPayload,
    };
    Q_ENUM(AdaptationFieldControlType)
private:
    AdaptationFieldControlType  _adaptationFieldControl;
    quint8      _continuityCounter;
    int         _iAdaptationField;
    quint8      _adaptationFieldLen;
    int         _iPayloadData;

public:
    explicit TSPacket(const QByteArray &bytes);

    static const char syncByte = '\x47';

    const QByteArray &bytes() const;

    bool TEI() const;
    bool PUSI() const;
    bool transportPrio() const;
    quint16 PID() const;
    TSCType TSC() const;
    AdaptationFieldControlType adaptationFieldControl() const;
    quint8 continuityCounter() const;
    QByteArray adaptationField() const;
    QByteArray payloadData() const;

    QString toString() const;
};

#endif // TSPACKET_H
