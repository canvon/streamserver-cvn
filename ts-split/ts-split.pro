QT += core
QT -= gui

CONFIG += c++14

TARGET = ts-split
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Link against libinfra.
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libinfra/release/ -linfra
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libinfra/debug/ -linfra
else:unix: LIBS += -L$$OUT_PWD/../libinfra/ -linfra

# Link against libmedia.
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libmedia/release/ -lmedia
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libmedia/debug/ -lmedia
else:unix: LIBS += -L$$OUT_PWD/../libmedia/ -lmedia

!isEmpty(QMAKE_REL_RPATH_BASE): QMAKE_RPATHDIR += . ../libmedia ../libinfra
unix: QMAKE_RPATHDIR += /usr/lib/streamserver-cvn

INCLUDEPATH += $$PWD/../libinfra $$PWD/../libmedia
DEPENDPATH += $$PWD/../libinfra $$PWD/../libmedia
