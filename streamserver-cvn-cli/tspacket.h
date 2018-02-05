#ifndef TSPACKET_H
#define TSPACKET_H

#include <QObject>

#include <memory>
#include <QByteArray>
#include <QDebug>

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
public:
    class AdaptationField {
        QByteArray  _bytes;

        quint8      _length;

    public:
        explicit AdaptationField(const QByteArray &bytes);

        const QByteArray &bytes() const;

        quint8 length() const;
    };
private:
    std::shared_ptr<AdaptationField>  _adaptationField;
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
    std::shared_ptr<const AdaptationField> adaptationField() const;
    QByteArray payloadData() const;
};

QDebug operator<<(QDebug debug, const TSPacket &packet);
QDebug operator<<(QDebug debug, const TSPacket::AdaptationField &af);

#endif // TSPACKET_H
