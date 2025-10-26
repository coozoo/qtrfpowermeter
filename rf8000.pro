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
INCLUDEPATH += $$PWD/3rdparty/spline/src
INCLUDEPATH += $$PWD/src

SOURCES += \
    3rdparty/qcustomplot/qcustomplot.cpp \
    main.cpp \
    src/attdevice.cpp \
    src/attenuationmanager.cpp \
    src/attenuatorwidget.cpp \
    src/cablelosscalculatorwindow.cpp \
    src/cablemodel.cpp \
    src/cablewidget.cpp \
    src/calibrationmanager.cpp \
    src/calibrationmodel.cpp \
    src/calibrationpoint.cpp \
    src/chartmanager.cpp \
    src/chartrealtime.cpp \
    src/devicecomboboxdelegate.cpp \
    src/fixedattenuatorcontrol.cpp \
    src/internalattenuatorcontrol.cpp \
    src/mainwindow.cpp \
    src/pmdevicefactory.cpp \
    src/qtcoaxcablelosscalcmanager.cpp \
    src/qtdigitalattenuator.cpp \
    src/rf8000device.cpp \
    src/rfpmv7device.cpp \
    src/serialportinterface.cpp \
    src/targetpowercalculator.cpp \
    src/unitconverter.cpp

HEADERS += \
    3rdparty/qcustomplot/qcustomplot.h \
    3rdparty/spline/src/spline.h \
    src/abstractpmdevice.h \
    src/attdevice.h \
    src/attenuationmanager.h \
    src/attenuatorwidget.h \
    src/cablelosscalculatorwindow.h \
    src/cablemodel.h \
    src/cablewidget.h \
    src/calibrationmanager.h \
    src/calibrationmodel.h \
    src/calibrationpoint.h \
    src/chartmanager.h \
    src/chartrealtime.h \
    src/devicecomboboxdelegate.h \
    src/fixedattenuatorcontrol.h \
    src/internalattenuatorcontrol.h \
    src/mainwindow.h \
    src/pmdevicefactory.h \
    src/pmdeviceproperties.h \
    src/qtcoaxcablelosscalcmanager.h \
    src/qtdigitalattenuator.h \
    src/rf8000device.h \
    src/rfpmv7device.h \
    src/serialportinterface.h \
    src/targetpowercalculator.h \
    src/unitconverter.h

FORMS += \
    src/cablelosscalculatorwindow.ui \
    src/calibrationmanager.ui \
    src/mainwindow.ui \
    src/qtdigitalattenuator.ui

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
