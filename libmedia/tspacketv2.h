#ifndef TSPACKET2_H
#define TSPACKET2_H

#include "libmedia_global.h"

#include <QObject>

#include "tsparser.h"
#include <memory>
#include <QByteArray>
#include <QString>
#include <QDebug>

namespace TS {


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

    // TODO: Add AdaptationField.

    PacketV2();

    bool isSyncByteFixedValue() const;
    bool isNullPacket() const;
};

LIBMEDIASHARED_EXPORT QDebug operator<<(QDebug debug, const PacketV2 &packet);


namespace impl {
class PacketV2ParserImpl;
}

class LIBMEDIASHARED_EXPORT PacketV2Parser
{
    std::unique_ptr<impl::PacketV2ParserImpl>  _implPtr;

public:
    struct Parse {
        QByteArray  bytes;
        PacketV2    packet;
        QString     errorMessage;
    };

    explicit PacketV2Parser();
    ~PacketV2Parser();

    int tsPacketSize() const;
    void setTSPacketSize(int size);

    bool parse(const QByteArray &bytes, Parse *output);
};


}  // namespace TS

#endif // TSPACKET2_H
