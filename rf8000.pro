QT       += core gui serialport charts printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtrfpowermeter

TRANSLATIONS = ./translations/$${TARGET}_en_US.ts ./translations/$${TARGET}_uk_UA.ts
TEMPLATE = app

CONFIG += c++17
# do not show qDebug() messages in release builds
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
CONFIG += lrelease

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += $$PWD/3rdparty/qcustomplot

SOURCES += \
    3rdparty/qcustomplot/qcustomplot.cpp \
    attdevice.cpp \
    attenuationmanager.cpp \
    attenuatorwidget.cpp \
    calibrationmanager.cpp \
    calibrationmodel.cpp \
    calibrationpoint.cpp \
    chartmanager.cpp \
    chartrealtime.cpp \
    fixedattenuatorcontrol.cpp \
    main.cpp \
    mainwindow.cpp \
    qtdigitalattenuator.cpp \
    serialportinterface.cpp \
    targetpowercalculator.cpp \
    unitconverter.cpp

HEADERS += \
    3rdparty/qcustomplot/qcustomplot.h \
    attdevice.h \
    attenuationmanager.h \
    attenuatorwidget.h \
    calibrationmanager.h \
    calibrationmodel.h \
    calibrationpoint.h \
    chartmanager.h \
    chartrealtime.h \
    fixedattenuatorcontrol.h \
    mainwindow.h \
    qtdigitalattenuator.h \
    serialportinterface.h \
    targetpowercalculator.h \
    unitconverter.h

FORMS += \
    calibrationmanager.ui \
    mainwindow.ui \
    qtdigitalattenuator.ui

win32:RC_FILE = icon.rc
macx:RC_FILE = icon.icns

DISTFILES +=

RESOURCES += \
    resources.qrc

# Default rules for deployment.
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target

binary.files += $$TARGET
binary.path = /usr/bin
translations.files += ./translations/$$files(*.qm/*.qm,true)
translations.path = /usr/share/$$TARGET
icon.files += images/qtrfpowermeter.svg
icon.path += /usr/share/icons/hicolor/scalable/apps
desktop.files += $${TARGET}.desktop
desktop.path += /usr/share/applications/
INSTALLS += binary translations icon desktop
