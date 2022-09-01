#-------------------------------------------------
#
# Project created by QtCreator 2021-06-08T13:24:57
#
#-------------------------------------------------

QT       += core gui dtkwidget svg network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ccc-app-manager
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11 link_pkgconfig

SOURCES += \
        src/main.cpp \
        src/mainwindow.cpp \
    src/appmanagerwidget.cpp \
    src/appmanagermodel.cpp \
    src/job/appmanagerjob.cpp \
    src/common/appmanagercommon.cpp \
    src/dlg/pkgdownloaddlg.cpp \
    src/pkgmonitor/pkgmonitor.cpp

HEADERS += \
        src/mainwindow.h \
    src/appmanagerwidget.h \
    src/appmanagermodel.h \
    src/job/appmanagerjob.h \
    src/common/appmanagercommon.h \
    src/dlg/pkgdownloaddlg.h \
    src/pkgmonitor/pkgmonitor.h

# Default rules for deployment.
target.path = /opt/apps/com.github.ccc-app-manager/files

icon.files = ./ccc-app-manager.svg
icon.path = /opt/apps/com.github.ccc-app-manager/files

opt_desktop.files = ./com.github.ccc-app-manager.desktop
opt_desktop.path = /opt/apps/com.github.ccc-app-manager/entries

usr_desktop.files = ./com.github.ccc-app-manager.desktop
usr_desktop.path = /usr/share/applications

pkg.files = ./pkg/*
pkg.path = /opt/apps/com.github.ccc-app-manager/files/pkg

INSTALLS += target icon opt_desktop usr_desktop pkg

RESOURCES += \
    resources/icons.qrc

LIBS += -L$$PWD/zlib -lz \
    -L$$PWD/ -lgsettings-qt
