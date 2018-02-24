QT += core network
QT -= gui

CONFIG += c++14

TARGET = streamserver-cvn-cli
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    streamserver.cpp \
    tspacket.cpp \
    streamclient.cpp \
    httprequest.cpp \
    httpreply.cpp

HEADERS += \
    streamserver.h \
    tspacket.h \
    streamclient.h \
    httprequest.h \
    httpreply.h

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

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libinfra/release/ -linfra
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libinfra/debug/ -linfra
else:unix: LIBS += -L$$OUT_PWD/../libinfra/ -linfra

INCLUDEPATH += $$PWD/../libinfra
DEPENDPATH += $$PWD/../libinfra
