QT += testlib
QT -= gui

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_cablemodel

INCLUDEPATH += ../../src ../../3rdparty/spline/src

SOURCES += \
    test_cablemodel.cpp \
    ../../src/cablemodel.cpp

HEADERS += \
    ../../src/cablemodel.h
