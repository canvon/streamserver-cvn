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

SOURCES += tst_tsparsertest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# Link against internal libraries used.
SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include(../include/internal_lib.pri)
