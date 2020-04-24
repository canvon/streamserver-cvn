TARGET = tst_tsparsertest
CONFIG += testcase
CONFIG += console
CONFIG -= app_bundle
QT += testlib
QT -= gui

SSCVN_REL_ROOT = ../../../..
include($${SSCVN_REL_ROOT}/config.pri)

SOURCES += tst_tsparsertest.cpp

# Link against internal libraries used.
SSCVN_LIB_NAMES = infra media
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include($${SSCVN_REL_ROOT}/include/internal_lib.pri)
