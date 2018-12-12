QT += core network
QT -= gui

# CONFIG += c++11

TARGET = db-query-proxy
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    log.cpp \
    daemon.cpp \
    proxy-server.cpp \
    query-processor.cpp

HEADERS += \
    log.h \
    daemon.h \
    proxy-server.h \
    query-processor.h

OBJECTS_DIR = ./obj/
MOC_DIR = ./moc/
DESTDIR = ../bin/
CONFIG += debug
