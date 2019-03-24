# streamserver-cvn config.pri - qmake include file common to all sub-projects

# Use the new TS::PacketV2 code.
#
# This is not the default, yet, as not every part of the code base
# is converted to v2, yet. So the project won't compile...
#
# For working on v2 integration, a separate build profile with -DTS_PACKET_V2
# can be used, e.g., by running qmake with DEFINES+=TS_PACKET_V2
# additional argument.
#
#DEFINES += TS_PACKET_V2
