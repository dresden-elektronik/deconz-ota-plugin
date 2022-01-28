TARGET   = std_otau_plugin

TARGET = $$qtLibraryTarget($$TARGET)

DEFINES += DECONZ_DLLSPEC=Q_DECL_IMPORT

unix:contains(QMAKE_HOST.arch, armv6l) {
    DEFINES += ARCH_ARM ARCH_ARMV6
}

win32:LIBS+=  -L../.. -ldeCONZ1
unix:LIBS+=  -L../.. -ldeCONZ
win32:CONFIG += dll

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release \
               += c++14

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
           otau_file.cpp \
           otau_file_loader.cpp \
           otau_model.cpp \
           otau_node.cpp

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    std_otau_widget.ui
