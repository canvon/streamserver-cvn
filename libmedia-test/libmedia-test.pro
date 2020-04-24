TARGET = tst_tsparsertest
CONFIG += console
CONFIG -= app_bundle
QT += testlib
QT -= gui

include(../config.pri)

SOURCES += tst_tsparsertest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# Link against internal libraries used.
SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include(../include/internal_lib.pri)
