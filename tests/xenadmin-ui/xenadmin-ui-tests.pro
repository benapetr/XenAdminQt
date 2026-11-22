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
    ../../src/xenadmin-ui/controls/xensearch/foldernavigator.cpp \
    ../../src/xenadmin-ui/controls/xensearch/groupingcontrol.cpp \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.cpp \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.cpp \
    ../../src/xenadmin-ui/settingspanels/ieditpage.cpp \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.cpp \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.cpp \
    ../../src/xenadmin-ui/settingspanels/networkgeneraleditpage.cpp \
    ../../src/xenadmin-ui/settingspanels/nrpeeditpage.cpp \
    ../../src/xenadmin-ui/settingspanels/clusteringeditpage.cpp \
    ../../src/xenadmin-ui/settingspanels/vmappliancestartorderpage.cpp \
    ../../src/xenadmin-ui/tabpages/basetabpage.cpp \
    ../../src/xenadmin-ui/tabpages/adtabpage.cpp \
    ../../src/xenadmin-ui/dialogs/actionprogressdialog.cpp \
    ../../src/xenadmin-ui/dialogs/adpasswordprompt.cpp \
    ../../src/xenadmin-ui/dialogs/roleselectiondialog.cpp \
    ../../src/xenadmin-ui/dialogs/resolvingsubjectsdialog.cpp \
    ../../src/xenadmin-ui/settingspanels/vmgroupvmspage.cpp \
    ../../src/xenadmin-ui/settingspanels/policysnapshottypepage.cpp \
    ../../src/xenadmin-ui/commands/vm/vmgroup.cpp \
    ../../src/xenadmin-ui/settingsmanager.cpp \
    ../../src/xenadmin-ui/connectionprofile.cpp \
    ../../src/xenadmin-ui/ConsoleView/ConsoleKeyHandler.cpp \
    ../../src/xenadmin-ui/ConsoleView/VNCGraphicsClient.cpp \
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
    ../../src/xenadmin-ui/controls/xensearch/foldernavigator.h \
    ../../src/xenadmin-ui/controls/xensearch/groupingcontrol.h \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.h \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.h \
    ../../src/xenadmin-ui/settingspanels/ieditpage.h \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.h \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.h \
    ../../src/xenadmin-ui/settingspanels/networkgeneraleditpage.h \
    ../../src/xenadmin-ui/settingspanels/nrpeeditpage.h \
    ../../src/xenadmin-ui/settingspanels/clusteringeditpage.h \
    ../../src/xenadmin-ui/settingspanels/vmappliancestartorderpage.h \
    ../../src/xenadmin-ui/tabpages/basetabpage.h \
    ../../src/xenadmin-ui/tabpages/adtabpage.h \
    ../../src/xenadmin-ui/dialogs/actionprogressdialog.h \
    ../../src/xenadmin-ui/dialogs/adpasswordprompt.h \
    ../../src/xenadmin-ui/dialogs/roleselectiondialog.h \
    ../../src/xenadmin-ui/dialogs/resolvingsubjectsdialog.h \
    ../../src/xenadmin-ui/settingspanels/vmgroupvmspage.h \
    ../../src/xenadmin-ui/settingspanels/policysnapshottypepage.h \
    ../../src/xenadmin-ui/commands/vm/vmgroup.h \
    ../../src/xenadmin-ui/settingsmanager.h \
    ../../src/xenadmin-ui/ConsoleView/ConsoleKeyHandler.h \
    ../../src/xenadmin-ui/ConsoleView/VNCGraphicsClient.h \
    ../../src/xenadmin-ui/dialogs/newcustomfielddialog.h \
    ../../src/xenadmin-ui/dialogs/warningdialogs/warningdialog.h

FORMS += \
    ../../src/xenadmin-ui/dialogs/customsearchdialog.ui \
    ../../src/xenadmin-ui/dialogs/savesearchdialog.ui \
    ../../src/xenadmin-ui/settingspanels/perfmonalertoptionspage.ui \
    ../../src/xenadmin-ui/settingspanels/customfieldsdisplaypage.ui \
    ../../src/xenadmin-ui/settingspanels/networkgeneraleditpage.ui \
    ../../src/xenadmin-ui/settingspanels/nrpeeditpage.ui \
    ../../src/xenadmin-ui/settingspanels/clusteringeditpage.ui \
    ../../src/xenadmin-ui/settingspanels/vmappliancestartorderpage.ui \
    ../../src/xenadmin-ui/tabpages/adtabpage.ui \
    ../../src/xenadmin-ui/dialogs/actionprogressdialog.ui \
    ../../src/xenadmin-ui/dialogs/adpasswordprompt.ui \
    ../../src/xenadmin-ui/dialogs/roleselectiondialog.ui \
    ../../src/xenadmin-ui/dialogs/resolvingsubjectsdialog.ui \
    ../../src/xenadmin-ui/settingspanels/vmgroupvmspage.ui \
    ../../src/xenadmin-ui/settingspanels/policysnapshottypepage.ui \
    ../../src/xenadmin-ui/dialogs/newcustomfielddialog.ui

RESOURCES += ../../src/xenadmin-ui/resources.qrc

INCLUDEPATH += \
    ../../src \
    ../../src/xenadmin-ui \
    ../../src/xenadmin-ui/controls/xensearch \
    ../../src/xenlib

# Link with prebuilt xenlib (user-managed build output)
LIBS += -L../../release/xenlib -lxenlib
