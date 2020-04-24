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

SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
include(../include/app_internal_libs.pri)
