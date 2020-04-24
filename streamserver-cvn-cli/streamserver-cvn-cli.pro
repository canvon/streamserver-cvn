QT += core network
QT -= gui

CONFIG += c++14

TARGET = streamserver-cvn-cli
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    streamserver.cpp \
    streamclient.cpp \
    http/httputil.cpp \
    http/httpheader_netside.cpp \
    http/httprequest_netside.cpp \
    http/httpresponse.cpp \
    http/httpserver.cpp

HEADERS += \
    streamserver.h \
    streamclient.h \
    http/httputil.h \
    http/httpheader_netside.h \
    http/httprequest_netside.h \
    http/httpresponse.h \
    http/httpserver.h

include(../config.pri)

CONFIG(debug, debug|release) {
    message("Building with debug messages")
} else {
    DEFINES += QT_NO_DEBUG_OUTPUT
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
include(../include/app_internal_libs.pri)
