QT += core
QT -= gui

CONFIG += c++14

TARGET = ts-dump
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

include(../config.pri)

SSCVN_REL_ROOT = ..
SSCVN_LIB_NAMES = infra media
include(../include/app_internal_libs.pri)
