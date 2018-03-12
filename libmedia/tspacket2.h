#ifndef TSPACKET2_H
#define TSPACKET2_H

#include "libmedia_global.h"

#include <QObject>

#include "tsparser.h"
#include <QDebug>

namespace TS {

class LIBMEDIASHARED_EXPORT Packet2
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

    // TODO: Add AdaptationField.

    Packet2();

    bool isSyncByteFixedValue() const;
    bool isNullPacket() const;
};

LIBMEDIASHARED_EXPORT QDebug operator<<(QDebug debug, const Packet2 &packet);

}  // namespace TS

#endif // TSPACKET2_H
