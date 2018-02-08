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
    QString     _errorMessage;
public:
    enum class AdditionalInfoLengthType {
        None = 0,
        TimeCodePrefix = 4,
        ForwardErrorCorrection1 = 16,
        ForwardErrorCorrection2 = 20,
    };
    Q_ENUM(AdditionalInfoLengthType)
private:
    AdditionalInfoLengthType  _additionalInfoLength = AdditionalInfoLengthType::None;
    QByteArray  _timeCode;
public:
    enum class ValidityType {
        None,
        SyncByte,
        PID,
        ContinuityCounter,
        AdaptationField,
        PayloadData,
    };
    Q_ENUM(ValidityType)
private:
    ValidityType  _validity = ValidityType::None;
    int           _iSyncByte;

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
        QString     _errorMessage;

        quint8      _length;
        bool        _flagsValid = false;
        bool        _discontinuityIndicator;
        bool        _randomAccessIndicator;
        bool        _ESPrioIndicator;
        bool        _PCRFlag;
        bool        _OPCRFlag;
        bool        _splicingPointFlag;
        bool        _transportPrivateDataFlag;
        bool        _extensionFlag;

        //quint64     _PCR;
        //quint64     _OPCR;
        bool        _spliceCountdownValid = false;
        qint8       _spliceCountdown;

        bool        _transportPrivateDataValid = false;
        int         _iTransportPrivateData;
        quint8      _transportPrivateDataLength;
        QByteArray  _transportPrivateData;

        bool        _extensionValid = false;
        int         _iExtension;
        quint8      _extensionLength;
        QByteArray  _extensionBytes;

        int         _iStuffingBytes;
        QByteArray  _stuffingBytes;

    public:
        explicit AdaptationField(const QByteArray &bytes);

        const QByteArray &bytes() const;
        const QString &errorMessage() const;

        quint8 length() const;
        bool flagsValid() const;
        bool discontinuityIndicator() const;
        bool randomAccessIndicator() const;
        bool ESPrioIndicator() const;
        bool PCRFlag() const;
        bool OPCRFlag() const;
        bool splicingPointFlag() const;
        bool transportPrivateDataFlag() const;
        bool extensionFlag() const;

        //PCR
        //OPCR
        bool spliceCountdownValid() const;
        qint8 spliceCountdown() const;
        bool transportPrivateDataValid() const;
        const QByteArray &transportPrivateData() const;
        bool extensionValid() const;
        const QByteArray &extensionBytes() const;
        const QByteArray &stuffingBytes() const;
    };
private:
    std::shared_ptr<AdaptationField>  _adaptationFieldPtr;
    int         _iPayloadData;

public:
    explicit TSPacket(const QByteArray &bytes);

    static const int lengthBasic = 188;
    static const char syncByte = '\x47';
    static const quint16 PIDNullPacket = 0x1fff;

    const QByteArray &bytes() const;
    const QString &errorMessage() const;
    AdditionalInfoLengthType additionalInfoLength() const;
    const QByteArray &timeCode() const;
    ValidityType validity() const;

    bool TEI() const;
    bool PUSI() const;
    bool transportPrio() const;
    quint16 PID() const;
    bool isNullPacket() const;
    TSCType TSC() const;
    AdaptationFieldControlType adaptationFieldControl() const;
    quint8 continuityCounter() const;
    std::shared_ptr<const AdaptationField> adaptationField() const;
    QByteArray payloadData() const;

    QByteArray toBasicPacketBytes() const;
};

QDebug operator<<(QDebug debug, const TSPacket &packet);
QDebug operator<<(QDebug debug, const TSPacket::AdaptationField &af);

#endif // TSPACKET_H
