QT += testlib gui widgets serialport
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_rfpmv5_parser

INCLUDEPATH += ../../src

SOURCES += \
    test_rfpmv5_parser.cpp \
    ../../src/rfpmv5device.cpp \
    ../../src/serialportinterface.cpp

HEADERS += \
    ../../src/abstractpmdevice.h \
    ../../src/pmdeviceproperties.h \
    ../../src/rfpmv5device.h \
    ../../src/serialportinterface.h
