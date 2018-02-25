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

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    humanreadable.cpp \
    log_backend.cpp

HEADERS += libinfra_global.h \
    humanreadable.h \
    demangle.h \
    log.h \
    log_backend.h

unix {
    target.path = /usr/lib/streamserver-cvn
    INSTALLS += target
}