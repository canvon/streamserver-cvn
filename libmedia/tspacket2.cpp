#include "tspacket2.h"

#include "humanreadable.h"

namespace TS {

Packet2::Packet2()
{

}

bool Packet2::isSyncByteFixedValue() const
{
    return syncByte.value == syncByteFixedValue;
}

bool Packet2::isNullPacket() const
{
    return pid.value == pidNullPacket;
}

QDebug operator<<(QDebug debug, const Packet2 &packet)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TS::Packet2(";

    if (packet.isSyncByteFixedValue())
        debug << "syncByte";
    else
        debug << "syncByte=" << packet.syncByte.value;

    debug << " transportErrorIndicator="   << packet.transportErrorIndicator.value;
    debug << " payloadUnitStartIndicator=" << packet.payloadUnitStartIndicator.value;
    debug << " transportPriority="         << packet.transportPriority.value;
    debug << " PID="                       << packet.pid.value;

    if (packet.isNullPacket()) {
        debug << " NullPacket";
    }
    else {
        debug << " " << packet.transportScramblingControl.value;
        debug << " " << packet.adaptationFieldControl.value;
        debug << " continuityCounter=" << packet.continuityCounter.value;

        if (packet.adaptationFieldControl.value == TS::Packet2::AdaptationFieldControlType::AdaptationFieldOnly ||
            packet.adaptationFieldControl.value == TS::Packet2::AdaptationFieldControlType::AdaptationFieldThenPayload)
        {
            // TODO: Add dump of AdaptationField.
            debug << " TODO=adaptationField";
        }
    }

    debug << ")";
    return debug;
}

}  // namespace TS
