TEMPLATE = app
CONFIG += testcase warn_on c++17
QT += testlib widgets network xml

TARGET = xenadmin-ui-tests

SOURCES += \
    test_main.cpp

INCLUDEPATH += \
    ../../src \
    ../../src/xenadmin-ui \
    ../../src/xenlib

# Link with prebuilt xenlib (user-managed build output)
LIBS += -L../../release/xenlib -lxenlib
