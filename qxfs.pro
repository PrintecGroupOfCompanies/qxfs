CFGPRI=$$clean_path($$PWD/../qxfsconfig.pri)

exists($$CFGPRI) {
    include($$CFGPRI)
}

QT -= gui
QT += network

CONFIG -= depend_includepath

INCLUDEPATH += $$INCLUDES
DEPENDPATH += $$INCLUDES

TEMPLATE = lib
DEFINES += QXFS_LIBRARY

# advapi32 for the token queries in qlocalipc.h
win32:LIBS += -ladvapi32

CONFIG += c++11

SOURCES += \
    qxfssocketstream.cpp \
    qxfsstream.cpp

HEADERS += \
    qlocalipc.h \
    qxfssocketstream.h \
    qxfsstream.h \


msvc {
    QMAKE_CFLAGS_RELEASE += /Zi
    QMAKE_CXXFLAGS_RELEASE += /Zi

    # For the linker:
    QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:REF /OPT:ICF
}
