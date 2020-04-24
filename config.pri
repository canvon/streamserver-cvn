# streamserver-cvn config.pri - qmake include file common to all sub-projects

CONFIG += c++14

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


#
# Qt Creator template-based configuration settings follow...
#

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
