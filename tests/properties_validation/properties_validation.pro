QT += testlib gui widgets serialport
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_properties_validation

INCLUDEPATH += ../../src

SOURCES += \
    test_properties_validation.cpp \
    ../../src/pmdevicefactory.cpp \
    ../../src/rf8000device.cpp \
    ../../src/rfpmv5device.cpp \
    ../../src/rfpmv7device.cpp \
    ../../src/conceptrfrpmdevice.cpp \
    ../../src/conceptrfrpmlookuptables.cpp \
    ../../src/serialportinterface.cpp \
    ../../src/unitconverter.cpp

HEADERS += \
    ../../src/abstractpmdevice.h \
    ../../src/pmdeviceproperties.h \
    ../../src/pmdevicefactory.h \
    ../../src/rf8000device.h \
    ../../src/rfpmv5device.h \
    ../../src/rfpmv7device.h \
    ../../src/conceptrfrpmdevice.h \
    ../../src/conceptrfrpmlookuptables.h \
    ../../src/serialportinterface.h \
    ../../src/unitconverter.h
