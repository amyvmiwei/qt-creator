TEMPLATE = app
TARGET = leak3

CONFIG += debug

QT += core

macx:CONFIG -= app_bundle

win32-msvc*:QMAKE_CXXFLAGS += -w4996

SOURCES += main.cpp
