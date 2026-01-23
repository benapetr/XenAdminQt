TEMPLATE = app
CONFIG += testcase warn_on c++17
QT += testlib network xml

TARGET = xenlib-tests

SOURCES += \
    test_main.cpp \
    test_helpers.cpp

HEADERS += \
    test_helpers.h

RESOURCES += \
    testdata.qrc

INCLUDEPATH += \
    ../../src \
    ../../src/xenlib

# Link with prebuilt xenlib (user-managed build output)
LIBS += -L../../release/xenlib -lxenlib
