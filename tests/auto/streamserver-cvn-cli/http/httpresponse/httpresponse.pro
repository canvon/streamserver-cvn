TARGET = tst_httpresponse
CONFIG += testcase
CONFIG += console
CONFIG -= app_bundle
QT += testlib
QT -= gui

SSCVN_REL_ROOT = ../../../../..
include($${SSCVN_REL_ROOT}/config.pri)

SOURCES += tst_httpresponse.cpp

SSCVN_APP_REL_DIR = $${SSCVN_REL_ROOT}/streamserver-cvn-cli

SSCVN_APP_OBJS = httputil.o httpresponse.o
for(OBJ, SSCVN_APP_OBJS): OBJECTS += $${OUT_PWD}/$${SSCVN_APP_REL_DIR}/$${OBJ}
INCLUDEPATH += $${PWD}/$${SSCVN_APP_REL_DIR}
DEPENDPATH  += $${PWD}/$${SSCVN_APP_REL_DIR}

# Link against internal libraries used.
SSCVN_LIB_NAMES = infra  # media
for(SSCVN_LIB_NAME, SSCVN_LIB_NAMES): include($${SSCVN_REL_ROOT}/include/internal_lib.pri)
