QT += testlib gui
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_advanced_calibration_adapter

INCLUDEPATH += ../../src ../../3rdparty/spline/src

SOURCES += \
    test_advanced_calibration_adapter.cpp \
    ../../src/advancedcalibrationtablemodel.cpp \
    ../../src/calibrationmodel.cpp \
    ../../src/calibrationpoint.cpp

HEADERS += \
    ../../src/advancedcalibrationtablemodel.h \
    ../../src/calibrationmodel.h \
    ../../src/calibrationpoint.h
