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

CONFIG += c++11

SOURCES += \
    qxfssocketstream.cpp \
    qxfsstream.cpp

HEADERS += \
    qxfssocketstream.h \
    qxfsstream.h \


msvc {
    QMAKE_CFLAGS_RELEASE += /Zi
    QMAKE_CXXFLAGS_RELEASE += /Zi

    # For the linker:
    QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:REF /OPT:ICF
}
