QT += testlib gui widgets serialport
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_attdevice_parser

INCLUDEPATH += ../../src

SOURCES += \
    test_attdevice_parser.cpp \
    ../../src/attdevice.cpp \
    ../../src/serialportinterface.cpp

HEADERS += \
    ../../src/attdevice.h \
    ../../src/serialportinterface.h
