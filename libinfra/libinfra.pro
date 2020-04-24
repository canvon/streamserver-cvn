#-------------------------------------------------
#
# Project created by QtCreator 2018-02-24T10:38:11
#
#-------------------------------------------------

QT       -= gui

TARGET = infra
VERSION = 0.1.0
TEMPLATE = lib

DEFINES += LIBINFRA_LIBRARY

include(../config.pri)

SOURCES += \
    humanreadable.cpp \
    log_backend.cpp

HEADERS += libinfra_global.h \
    humanreadable.h \
    numericconverter.h \
    numericrange.h \
    exceptionbuilder.h \
    demangle.h \
    log.h \
    log_backend.h

unix {
    target.path = /usr/lib/streamserver-cvn
    INSTALLS += target
}
