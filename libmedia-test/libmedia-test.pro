#-------------------------------------------------
#
# Project created by QtCreator 2018-03-11T16:19:23
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

CONFIG += c++14

TARGET = tst_tsparsertest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

include(../config.pri)

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += tst_tsparsertest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# Link against internal libraries used.
SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include(../include/internal_lib.pri)
