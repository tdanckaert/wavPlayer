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

include( ./config.pri )

TARGET = wavplayer
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++0x

LIBS += -ljack -lsndfile -lsamplerate -lmpg123

SOURCES += main.cpp\
        mainwindow.cpp \
    waveview.cpp \
    wave.cpp \
    cutter.cpp \
    jackplayer.cpp

HEADERS  += mainwindow.h \
    waveview.h \
    wave.h \
    cutter.h \
    jackplayer.h

FORMS    += mainwindow.ui
