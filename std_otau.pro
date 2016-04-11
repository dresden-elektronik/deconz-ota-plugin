include(../../../compile.pri)

TARGET   = std_otau_plugin
include(../common.pri)

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += core gui widgets
}

CONFIG(debug, debug|release) {
    LIBS += -L../../debug
}

CONFIG(release, debug|release) {
    LIBS += -L../../release
}

QMAKE_CXXFLAGS += -Wno-attributes \
                  -Wall

INCLUDEPATH    += ../.. \
                  ../../common

HEADERS  = std_otau_plugin.h \
           std_otau_widget.h \
           otau_file.h \
           otau_file_loader.h \
           otau_model.h \
           otau_node.h

SOURCES  = std_otau_plugin.cpp \
           std_otau_widget.cpp \
           ../../deconz/crc8.c \
           otau_file.cpp \
           otau_file_loader.cpp \
           otau_model.cpp \
           otau_node.cpp

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    std_otau_widget.ui
