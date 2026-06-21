QT += testlib gui widgets serialport
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_rf8000_parser

INCLUDEPATH += ../../src

SOURCES += \
    test_rf8000_parser.cpp \
    ../../src/rf8000device.cpp \
    ../../src/serialportinterface.cpp

HEADERS += \
    ../../src/abstractpmdevice.h \
    ../../src/pmdeviceproperties.h \
    ../../src/rf8000device.h \
    ../../src/serialportinterface.h
