TEMPLATE = app
CONFIG += console
TARGET = obj2json

LIBS += -lcore
include(../../shared.pri)
PRE_TARGETDEPS += $$quote($$CORELIBFILE)

SOURCES += obj2json.cpp