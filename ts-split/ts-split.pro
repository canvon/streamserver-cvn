QT += core
QT -= gui

TARGET = ts-split
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    splitter.cpp

HEADERS += \
    splitter.h

include(../config.pri)

SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
include(../include/app_internal_libs.pri)
