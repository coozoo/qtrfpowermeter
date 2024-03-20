QT       += core gui serialport charts printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtrfpowermeter

TRANSLATIONS = ./translations/$${TARGET}_en_US.ts ./translations/$${TARGET}_uk_UA.ts

CONFIG += c++17
CONFIG += lrelease

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    chartmanager.cpp \
    chartrealtime.cpp \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp \
    serialportinterface.cpp

HEADERS += \
    chartmanager.h \
    chartrealtime.h \
    mainwindow.h \
    qcustomplot.h \
    serialportinterface.h

FORMS += \
    mainwindow.ui

win32:RC_FILE = icon.rc
macx:RC_FILE = icon.icns

RESOURCES += \
    resources.qrc

# Default rules for deployment.
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target

binary.files += $$TARGET
binary.path = /usr/bin
translations.files += ./translations/$$files(.qm/*.qm,true)
translations.path = /usr/share/$$TARGET
icon.files += images/icon.png
icon.path += /usr/share/icons
desktop.files += $${TARGET}.desktop
desktop.path += /usr/share/applications/
INSTALLS += binary translations icon desktop
