#-------------------------------------------------
#
# Project created by QtCreator 2014-06-10T18:23:26
#
#-------------------------------------------------

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(release, debug|release) {
    message("Release build, disabling QDebug output.")
    DEFINES += QT_NO_DEBUG_OUTPUT
}

TARGET = wavplayer
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++0x

LIBS += -ljack -lsndfile

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui
