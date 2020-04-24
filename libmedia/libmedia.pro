#-------------------------------------------------
#
# Project created by QtCreator 2018-02-25T13:08:37
#
#-------------------------------------------------

QT       -= gui

CONFIG += c++14

TARGET = media
VERSION = 0.1.0
TEMPLATE = lib

DEFINES += LIBMEDIA_LIBRARY

include(../config.pri)

SOURCES += \
    tspacket.cpp \
    tspacketv2.cpp \
    tsreader.cpp \
    tswriter.cpp

HEADERS += libmedia_global.h \
    conversionstore.h \
    tsprimitive.h \
    tspacket.h \
    tspacketv2.h \
    tspacket_compat.h \
    tsreader.h \
    tswriter.h

unix {
    target.path = /usr/lib/streamserver-cvn
    INSTALLS += target
}

# Link against internal library libinfra.
SSCVN_REL_ROOT = ..
SSCVN_LIB_NAME = infra
include(../include/internal_lib.pri)
