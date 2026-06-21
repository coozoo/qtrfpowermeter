QT += testlib
QT -= gui

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_unitconverter

INCLUDEPATH += ../../src

SOURCES += \
    test_unitconverter.cpp \
    ../../src/unitconverter.cpp

HEADERS += \
    ../../src/unitconverter.h
