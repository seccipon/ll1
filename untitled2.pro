TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    event.cpp \
    eventhandler.cpp \
    event1.cpp

HEADERS += \
    event.h \
    eventhandler.h \
    event1.h

LIBS += /usr/lib/libboost_thread.so.1.49.0
