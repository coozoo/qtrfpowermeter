QT += testlib
QT -= gui

CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET = tst_attenuator_chain

INCLUDEPATH += ../../src

SOURCES += \
    test_chain_safety_evaluator.cpp \
    ../../src/chainsafetyevaluator.cpp

HEADERS += \
    ../../src/chainsafetyevaluator.h
