#ifndef TSPACKET2_H
#define TSPACKET2_H

#include "libmedia_global.h"

#include <QObject>

#include "tsprimitive.h"
#include <memory>
#include <QByteArray>
#include <QString>
#include <QDebug>

namespace TS {


// Store timestamp:
// PCR is MPEG-TS mnemonic for "program clock reference"

struct ProgramClockReference
{
    static constexpr quint64 systemClockFrequencyHz = 27000000;  // 27 MHz
    static constexpr quint64 pcrBaseFactor = 300;
    static constexpr quint8  reserved1FixedValue = 0x3f;  // all-one bits

    uimsbf<33, quint64>  pcrBase      { 0 };
    bslbf < 6, quint8 >  reserved1    { reserved1FixedValue };
    uimsbf< 9, quint16>  pcrExtension { 0 };

    quint64 pcrValue() const;
    quint64 toNanosecs() const;
    double toSecs() const;
};

QDebug operator<<(QDebug debug, const ProgramClockReference &pcr);

BitStream &operator>>(BitStream &bitSource, ProgramClockReference &pcr);


// An MPEG-TS packet, streamserver-cvn API version V2.

class LIBMEDIASHARED_EXPORT PacketV2
{
    Q_GADGET

public:
    static constexpr int sizeBasic = 188;

    static constexpr quint8 syncByteFixedValue = '\x47';
    bslbf8  syncByte      { syncByteFixedValue };
    bslbf1  transportErrorIndicator   { false };
    bslbf1  payloadUnitStartIndicator { false };
    bslbf1  transportPriority         { false };
    static constexpr quint16   pidNullPacket = 0x1fff;
    uimsbf<13, quint16>  pid { pidNullPacket };

    enum class TransportScramblingControlType {
        NotScrambled,
        Reserved1,
        ScrambledEvenKey,
        ScrambledOddKey,
    };
    Q_ENUM(TransportScramblingControlType)
    bslbf<2, TransportScramblingControlType>  transportScramblingControl { TransportScramblingControlType::NotScrambled };

    enum class AdaptationFieldControlType {
        Reserved1,
        PayloadOnly,
        AdaptationFieldOnly,
        AdaptationFieldThenPayload,
    };
    Q_ENUM(AdaptationFieldControlType)
    bslbf<2, AdaptationFieldControlType>  adaptationFieldControl { AdaptationFieldControlType::Reserved1 };

    uimsbf<4, quint8>  continuityCounter { 0 };

    struct AdaptationField
    {
        uimsbf<8, quint8>  adaptationFieldLength  { 0 };
        bslbf1  discontinuityIndicator            { false };
        bslbf1  randomAccessIndicator             { false };
        bslbf1  elementaryStreamPriorityIndicator { false };
        bslbf1  pcrFlag                           { false };
        bslbf1  opcrFlag                          { false };
        bslbf1  splicingPointFlag                 { false };
        bslbf1  transportPrivateDataFlag          { false };
        bslbf1  adaptationFieldExtensionFlag      { false };

        ProgramClockReference  programClockReference, originalProgramClockReference;

        tcimsbf<8, qint8>  spliceCountdown { 0 };

        uimsbf<8, quint8>  transportPrivateDataLength { 0 };
        QByteArray         transportPrivateDataBytes;

        // TODO: Actually implement adaptation field extension.
        uimsbf<8, quint8>  adaptationFieldExtensionLength { 0 };
        QByteArray         adaptationFieldExtensionBytes;

        QByteArray  stuffingBytes;
    };
    AdaptationField  adaptationField;

    QByteArray  payloadDataBytes;


    PacketV2();

    bool isSyncByteFixedValue() const;
    bool isNullPacket() const;
    bool hasAdaptationField() const;
    bool hasPayload() const;
};

LIBMEDIASHARED_EXPORT QDebug operator<<(QDebug debug, const PacketV2 &packet);
LIBMEDIASHARED_EXPORT QDebug operator<<(QDebug debug, const PacketV2::AdaptationField &af);


namespace impl {
class PacketV2ParserImpl;
}

class LIBMEDIASHARED_EXPORT PacketV2Parser
{
    std::unique_ptr<impl::PacketV2ParserImpl>  _implPtr;

public:
    explicit PacketV2Parser();
    ~PacketV2Parser();

    int tsPacketSize() const;
    void setTSPacketSize(int size);

    bool parse(const QByteArray &bytes, PacketV2 *packet, QString *errorMessage = nullptr);
};


}  // namespace TS

#endif // TSPACKET2_H
