// streamserver-cvn libmedia TSPacket vs TS::PacketV2 compatibility

#ifndef TSPACKET_COMPAT_H
#define TSPACKET_COMPAT_H


#ifndef TS_PACKET_V2
class TSPacket;
#endif

namespace TS {

#ifndef TS_PACKET_V2
using Packet = TSPacket;
#else
class PacketV2;
using Packet = PacketV2;
#endif

}  // namespace TS


#endif // TSPACKET_COMPAT_H
