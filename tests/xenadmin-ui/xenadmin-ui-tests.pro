TEMPLATE = app
CONFIG += testcase warn_on c++17
QT += testlib widgets network xml

TARGET = xenadmin-ui-tests

SOURCES += \
    test_main.cpp \
    ../../src/xenadmin-ui/iconmanager.cpp \
    ../../src/xenadmin-ui/controls/dropdownbutton.cpp \
    ../../src/xenadmin-ui/controls/xensearch/queryelement.cpp \
    ../../src/xenadmin-ui/controls/xensearch/querytype.cpp \
    ../../src/xenadmin-ui/controls/xensearch/resourceselectbutton.cpp \
    ../../src/xenadmin-ui/controls/xensearch/searcher.cpp \
    ../../src/xenadmin-ui/controls/xensearch/groupingcontrol.cpp \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.cpp \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.cpp \
    ../../src/xenadmin-ui/settingspanels/ieditpage.cpp \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.cpp \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.cpp \
    ../../src/xenadmin-ui/dialogs/newcustomfielddialog.cpp \
    ../../src/xenadmin-ui/dialogs/warningdialogs/warningdialog.cpp

# Search editor widgets (moc)
HEADERS += \
    ../../src/xenadmin-ui/iconmanager.h \
    ../../src/xenadmin-ui/controls/dropdownbutton.h \
    ../../src/xenadmin-ui/controls/xensearch/queryelement.h \
    ../../src/xenadmin-ui/controls/xensearch/querytype.h \
    ../../src/xenadmin-ui/controls/xensearch/resourceselectbutton.h \
    ../../src/xenadmin-ui/controls/xensearch/searcher.h \
    ../../src/xenadmin-ui/controls/xensearch/groupingcontrol.h \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.h \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.h \
    ../../src/xenadmin-ui/settingspanels/ieditpage.h \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.h \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.h \
    ../../src/xenadmin-ui/dialogs/newcustomfielddialog.h \
    ../../src/xenadmin-ui/dialogs/warningdialogs/warningdialog.h

FORMS += \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.ui \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.ui \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.ui \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.ui \
    ../../src/xenadmin-ui/dialogs/newcustomfielddialog.ui

INCLUDEPATH += \
    ../../src \
    ../../src/xenadmin-ui \
    ../../src/xenadmin-ui/controls/xensearch \
    ../../src/xenlib

# Link with prebuilt xenlib (user-managed build output)
LIBS += -L../../release/xenlib -lxenlib
