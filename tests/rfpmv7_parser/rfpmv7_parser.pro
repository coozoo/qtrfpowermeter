QT += testlib gui widgets serialport
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_rfpmv7_parser

INCLUDEPATH += ../../src

SOURCES += \
    test_rfpmv7_parser.cpp \
    ../../src/rfpmv7device.cpp \
    ../../src/serialportinterface.cpp \
    ../../src/unitconverter.cpp

HEADERS += \
    ../../src/abstractpmdevice.h \
    ../../src/pmdeviceproperties.h \
    ../../src/rfpmv7device.h \
    ../../src/serialportinterface.h \
    ../../src/unitconverter.h
