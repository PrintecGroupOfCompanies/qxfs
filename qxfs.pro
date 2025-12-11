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
    xfs_global.h \
    qxfssocketstream.h \
    qxfsstream.h \


# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
