QT += testlib gui
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_calibrationmodel

INCLUDEPATH += ../../src ../../3rdparty/spline/src

SOURCES += \
    test_calibrationmodel.cpp \
    ../../src/calibrationmodel.cpp \
    ../../src/calibrationpoint.cpp

HEADERS += \
    ../../src/calibrationmodel.h \
    ../../src/calibrationpoint.h
