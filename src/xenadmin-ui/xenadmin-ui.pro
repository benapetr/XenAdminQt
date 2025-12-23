TEMPLATE = app
CONFIG += qt warn_on c++17
QT += widgets network xml charts
TARGET = xenadmin-qt

# Source files
SOURCES += \
    commands/host/hostcommand.cpp \
    main.cpp \
    mainwindow.cpp \
    tabpages/vmstoragetabpage.cpp \
    titlebar.cpp \
    placeholderwidget.cpp \
    settingsmanager.cpp \
    iconmanager.cpp \
    connectionprofile.cpp \
    dialogs/connectdialog.cpp \
    dialogs/debugwindow.cpp \
    dialogs/aboutdialog.cpp \
    dialogs/ballooningdialog.cpp \
    dialogs/commanderrordialog.cpp \
    dialogs/newvmwizard.cpp \
    dialogs/importwizard.cpp \
    dialogs/exportwizard.cpp \
    dialogs/newnetworkwizard.cpp \
    dialogs/newsrwizard.cpp \
    dialogs/newpooldialog.cpp \
    dialogs/hawizard.cpp \
    dialogs/editvmhaprioritiesdialog.cpp \
    dialogs/vmpropertiesdialog.cpp \
    dialogs/hostpropertiesdialog.cpp \
    dialogs/poolpropertiesdialog.cpp \
    dialogs/storagepropertiesdialog.cpp \
    dialogs/networkpropertiesdialog.cpp \
    dialogs/bondpropertiesdialog.cpp \
    dialogs/networkingpropertiesdialog.cpp \
    dialogs/vifdialog.cpp \
    dialogs/operationprogressdialog.cpp \
    dialogs/vmsnapshotdialog.cpp \
    dialogs/vdipropertiesdialog.cpp \
    dialogs/attachvirtualdiskdialog.cpp \
    dialogs/newvirtualdiskdialog.cpp \
    dialogs/movevirtualdiskdialog.cpp \
    dialogs/migratevirtualdiskdialog.cpp \
    dialogs/optionsdialog.cpp \
    dialogs/verticallytabbeddialog.cpp \
    controls/affinitypicker.cpp \
    settingspanels/generaleditpage.cpp \
    settingspanels/hostautostarteditpage.cpp \
    settingspanels/hostmultipathpage.cpp \
    settingspanels/hostpoweroneditpage.cpp \
    settingspanels/logdestinationeditpage.cpp \
    settingspanels/cpumemoryeditpage.cpp \
    settingspanels/bootoptionseditpage.cpp \
    settingspanels/vmhaeditpage.cpp \
    settingspanels/customfieldsdisplaypage.cpp \
    settingspanels/perfmonalerteditpage.cpp \
    settingspanels/homeservereditpage.cpp \
    settingspanels/vmadvancededitpage.cpp \
    settingspanels/vmenlightenmenteditpage.cpp \
    dialogs/optionspages/securityoptionspage.cpp \
    dialogs/optionspages/displayoptionspage.cpp \
    dialogs/optionspages/confirmationoptionspage.cpp \
    dialogs/optionspages/consolesoptionspage.cpp \
    dialogs/optionspages/saveandrestoreoptionspage.cpp \
    dialogs/optionspages/connectionoptionspage.cpp \
    operations/operationmanager.cpp \
    alerts/alert.cpp \
    alerts/alertmanager.cpp \
    alerts/messagealert.cpp \
    alerts/alarmmessagealert.cpp \
    alerts/policyalert.cpp \
    alerts/certificatealert.cpp \
    actions/meddlingaction.cpp \
    actions/meddlingactionmanager.cpp \
    commands/command.cpp \
    commands/contextmenubuilder.cpp \
    commands/host/hostmaintenancemodecommand.cpp \
    commands/host/reboothostcommand.cpp \
    commands/host/shutdownhostcommand.cpp \
    commands/host/hostpropertiescommand.cpp \
    commands/host/poweronhostcommand.cpp \
    commands/host/reconnecthostcommand.cpp \
    commands/host/disconnecthostcommand.cpp \
    commands/host/connectallhostscommand.cpp \
    commands/host/disconnectallhostscommand.cpp \
    commands/host/destroyhostcommand.cpp \
    commands/host/restarttoolstackcommand.cpp \
    commands/host/hostreconnectascommand.cpp \
    commands/host/removehostcommand.cpp \
    commands/host/rescanpifscommand.cpp \
    commands/shutdowncommand.cpp \
    commands/rebootcommand.cpp \
    commands/vm/startvmcommand.cpp \
    commands/vm/stopvmcommand.cpp \
    commands/vm/restartvmcommand.cpp \
    commands/vm/suspendvmcommand.cpp \
    commands/vm/resumevmcommand.cpp \
    commands/vm/vmoperationhelpers.cpp \
    commands/vm/pausevmcommand.cpp \
    commands/vm/unpausevmcommand.cpp \
    commands/vm/forceshutdownvmcommand.cpp \
    commands/vm/forcerebootvmcommand.cpp \
    commands/vm/migratevmcommand.cpp \
    commands/vm/clonevmcommand.cpp \
    commands/vm/vmlifecyclecommand.cpp \
    commands/vm/copyvmcommand.cpp \
    commands/vm/movevmcommand.cpp \
    commands/vm/installtoolscommand.cpp \
    commands/vm/uninstallvmcommand.cpp \
    commands/vm/deletevmcommand.cpp \
    commands/vm/deletevmsandtemplatescommand.cpp \
    commands/vm/convertvmtotemplatecommand.cpp \
    commands/vm/exportvmcommand.cpp \
    commands/vm/newvmcommand.cpp \
    commands/vm/vmpropertiescommand.cpp \
    commands/vm/takesnapshotcommand.cpp \
    commands/vm/deletesnapshotcommand.cpp \
    commands/vm/reverttosnapshotcommand.cpp \
    commands/vm/vmrecoverymodecommand.cpp \
    commands/vm/vappstartcommand.cpp \
    commands/vm/vappshutdowncommand.cpp \
    commands/vm/exportsnapshotastemplatecommand.cpp \
    commands/vm/newtemplatefromsnapshotcommand.cpp \
    commands/vm/disablechangedblocktrackingcommand.cpp \
    commands/vm/importvmcommand.cpp \
    commands/template/deletetemplatecommand.cpp \
    commands/template/exporttemplatecommand.cpp \
    commands/template/createvmfromtemplatecommand.cpp \
    commands/template/newvmfromtemplatecommand.cpp \
    commands/template/instantvmfromtemplatecommand.cpp \
    commands/template/copytemplatecommand.cpp \
    commands/storage/repairsrcommand.cpp \
    commands/storage/detachsrcommand.cpp \
    commands/storage/setdefaultsrcommand.cpp \
    commands/storage/newsrcommand.cpp \
    commands/storage/storagepropertiescommand.cpp \
    commands/storage/addvirtualdiskcommand.cpp \
    commands/storage/attachvirtualdiskcommand.cpp \
    commands/storage/reattachsrcommand.cpp \
    commands/storage/forgetsrcommand.cpp \
    commands/storage/destroysrcommand.cpp \
    commands/storage/activatevbdcommand.cpp \
    commands/storage/deactivatevbdcommand.cpp \
    commands/storage/detachvirtualdiskcommand.cpp \
    commands/storage/deletevirtualdiskcommand.cpp \
    commands/storage/movevirtualdiskcommand.cpp \
    commands/storage/trimsrcommand.cpp \
    commands/storage/vdieditsizelocationcommand.cpp \
    commands/storage/migratevirtualdiskcommand.cpp \
    commands/pool/poolpropertiescommand.cpp \
    commands/pool/joinpoolcommand.cpp \
    commands/pool/ejecthostfrompoolcommand.cpp \
    commands/pool/newpoolcommand.cpp \
    commands/pool/deletepoolcommand.cpp \
    commands/pool/rotatepoolsecretcommand.cpp \
    commands/pool/disconnectpoolcommand.cpp \
    commands/pool/haconfigurecommand.cpp \
    commands/pool/hadisablecommand.cpp \
    commands/network/newnetworkcommand.cpp \
    commands/network/networkpropertiescommand.cpp \
    commands/network/destroybondcommand.cpp \
    tabpages/basetabpage.cpp \
    tabpages/generaltabpage.cpp \
    tabpages/physicalstoragetabpage.cpp \
    tabpages/srstoragetabpage.cpp \
    tabpages/networktabpage.cpp \
    tabpages/nicstabpage.cpp \
    tabpages/consoletabpage.cpp \
    tabpages/cvmconsoletabpage.cpp \
    tabpages/snapshotstabpage.cpp \
    tabpages/performancetabpage.cpp \
    tabpages/bootoptionstab.cpp \
    tabpages/memorytabpage.cpp \
    tabpages/searchtabpage.cpp \
    tabpages/notificationsbasepage.cpp \
    tabpages/alertsummarypage.cpp \
    tabpages/eventspage.cpp \
    widgets/qvncclient.cpp \
    widgets/memorybar.cpp \
    widgets/progressbardelegate.cpp \
    widgets/verticaltabwidget.cpp \
    navigation/navigationpane.cpp \
    navigation/navigationview.cpp \
    widgets/notificationsview.cpp \
    widgets/notificationssubmodeitem.cpp \
    navigation/navigationbuttons.cpp \
    widgets/wizardnavigationpane.cpp \
    network/httpconnect.cpp \
    ConsoleView/ConsoleKeyHandler.cpp \
    ConsoleView/VNCGraphicsClient.cpp \
    ConsoleView/RdpClient.cpp \
    ConsoleView/XSVNCScreen.cpp \
    ConsoleView/VNCTabView.cpp \
    ConsoleView/VNCView.cpp \
    ConsoleView/ConsolePanel.cpp \
    widgets/isodropdownbox.cpp \
    navigation/navigationhistory.cpp

# Header files
HEADERS += \
    commands/host/hostcommand.h \
    globals.h \
    mainwindow.h \
    tabpages/vmstoragetabpage.h \
    titlebar.h \
    placeholderwidget.h \
    settingsmanager.h \
    iconmanager.h \
    connectionprofile.h \
    dialogs/connectdialog.h \
    dialogs/debugwindow.h \
    dialogs/aboutdialog.h \
    dialogs/ballooningdialog.h \
    dialogs/commanderrordialog.h \
    dialogs/newvmwizard.h \
    dialogs/importwizard.h \
    dialogs/exportwizard.h \
    dialogs/newnetworkwizard.h \
    dialogs/newsrwizard.h \
    dialogs/newpooldialog.h \
    dialogs/hawizard.h \
    dialogs/editvmhaprioritiesdialog.h \
    dialogs/vmpropertiesdialog.h \
    dialogs/hostpropertiesdialog.h \
    dialogs/poolpropertiesdialog.h \
    dialogs/storagepropertiesdialog.h \
    dialogs/networkpropertiesdialog.h \
    dialogs/bondpropertiesdialog.h \
    dialogs/networkingpropertiesdialog.h \
    dialogs/vifdialog.h \
    dialogs/operationprogressdialog.h \
    dialogs/vmsnapshotdialog.h \
    dialogs/vdipropertiesdialog.h \
    dialogs/attachvirtualdiskdialog.h \
    widgets/isodropdownbox.h \
    dialogs/newvirtualdiskdialog.h \
    dialogs/movevirtualdiskdialog.h \
    dialogs/migratevirtualdiskdialog.h \
    dialogs/optionsdialog.h \
    dialogs/verticallytabbeddialog.h \
    controls/affinitypicker.h \
    settingspanels/ieditpage.h \
    settingspanels/generaleditpage.h \
    settingspanels/hostautostarteditpage.h \
    settingspanels/hostmultipathpage.h \
    settingspanels/hostpoweroneditpage.h \
    settingspanels/logdestinationeditpage.h \
    settingspanels/cpumemoryeditpage.h \
    settingspanels/bootoptionseditpage.h \
    settingspanels/vmhaeditpage.h \
    settingspanels/customfieldsdisplaypage.h \
    settingspanels/perfmonalerteditpage.h \
    settingspanels/homeservereditpage.h \
    settingspanels/vmadvancededitpage.h \
    settingspanels/vmenlightenmenteditpage.h \
    dialogs/optionspages/ioptionspage.h \
    dialogs/optionspages/securityoptionspage.h \
    dialogs/optionspages/displayoptionspage.h \
    dialogs/optionspages/confirmationoptionspage.h \
    dialogs/optionspages/consolesoptionspage.h \
    dialogs/optionspages/saveandrestoreoptionspage.h \
    dialogs/optionspages/connectionoptionspage.h \
    operations/operationmanager.h \
    alerts/alert.h \
    alerts/alertmanager.h \
    alerts/messagealert.h \
    alerts/alarmmessagealert.h \
    alerts/policyalert.h \
    alerts/certificatealert.h \
    actions/meddlingaction.h \
    actions/meddlingactionmanager.h \
    commands/command.h \
    commands/contextmenubuilder.h \
    commands/host/hostmaintenancemodecommand.h \
    commands/host/reboothostcommand.h \
    commands/host/shutdownhostcommand.h \
    commands/host/hostpropertiescommand.h \
    commands/host/poweronhostcommand.h \
    commands/host/reconnecthostcommand.h \
    commands/host/disconnecthostcommand.h \
    commands/host/connectallhostscommand.h \
    commands/host/disconnectallhostscommand.h \
    commands/host/destroyhostcommand.h \
    commands/host/restarttoolstackcommand.h \
    commands/host/hostreconnectascommand.h \
    commands/host/removehostcommand.h \
    commands/host/rescanpifscommand.h \
    commands/shutdowncommand.h \
    commands/rebootcommand.h \
    commands/vm/startvmcommand.h \
    commands/vm/stopvmcommand.h \
    commands/vm/restartvmcommand.h \
    commands/vm/suspendvmcommand.h \
    commands/vm/resumevmcommand.h \
    commands/vm/vmoperationhelpers.h \
    commands/vm/pausevmcommand.h \
    commands/vm/unpausevmcommand.h \
    commands/vm/forceshutdownvmcommand.h \
    commands/vm/forcerebootvmcommand.h \
    commands/vm/migratevmcommand.h \
    commands/vm/clonevmcommand.h \
    commands/vm/vmlifecyclecommand.h \
    commands/vm/copyvmcommand.h \
    commands/vm/movevmcommand.h \
    commands/vm/installtoolscommand.h \
    commands/vm/uninstallvmcommand.h \
    commands/vm/deletevmcommand.h \
    commands/vm/deletevmsandtemplatescommand.h \
    commands/vm/convertvmtotemplatecommand.h \
    commands/vm/exportvmcommand.h \
    commands/vm/newvmcommand.h \
    commands/vm/vmpropertiescommand.h \
    commands/vm/takesnapshotcommand.h \
    commands/vm/deletesnapshotcommand.h \
    commands/vm/reverttosnapshotcommand.h \
    commands/vm/vmrecoverymodecommand.h \
    commands/vm/vappstartcommand.h \
    commands/vm/vappshutdowncommand.h \
    commands/vm/exportsnapshotastemplatecommand.h \
    commands/vm/newtemplatefromsnapshotcommand.h \
    commands/vm/disablechangedblocktrackingcommand.h \
    commands/vm/importvmcommand.h \
    commands/template/deletetemplatecommand.h \
    commands/template/exporttemplatecommand.h \
    commands/template/createvmfromtemplatecommand.h \
    commands/template/newvmfromtemplatecommand.h \
    commands/template/instantvmfromtemplatecommand.h \
    commands/template/copytemplatecommand.h \
    commands/storage/repairsrcommand.h \
    commands/storage/detachsrcommand.h \
    commands/storage/setdefaultsrcommand.h \
    commands/storage/newsrcommand.h \
    commands/storage/storagepropertiescommand.h \
    commands/storage/addvirtualdiskcommand.h \
    commands/storage/attachvirtualdiskcommand.h \
    commands/storage/reattachsrcommand.h \
    commands/storage/forgetsrcommand.h \
    commands/storage/destroysrcommand.h \
    commands/storage/activatevbdcommand.h \
    commands/storage/deactivatevbdcommand.h \
    commands/storage/detachvirtualdiskcommand.h \
    commands/storage/deletevirtualdiskcommand.h \
    commands/storage/movevirtualdiskcommand.h \
    commands/storage/trimsrcommand.h \
    commands/storage/vdieditsizelocationcommand.h \
    commands/storage/migratevirtualdiskcommand.h \
    commands/pool/poolpropertiescommand.h \
    commands/pool/joinpoolcommand.h \
    commands/pool/ejecthostfrompoolcommand.h \
    commands/pool/newpoolcommand.h \
    commands/pool/deletepoolcommand.h \
    commands/pool/rotatepoolsecretcommand.h \
    commands/pool/disconnectpoolcommand.h \
    commands/pool/haconfigurecommand.h \
    commands/pool/hadisablecommand.h \
    commands/network/newnetworkcommand.h \
    commands/network/networkpropertiescommand.h \
    commands/network/destroybondcommand.h \
    tabpages/basetabpage.h \
    tabpages/generaltabpage.h \
    tabpages/physicalstoragetabpage.h \
    tabpages/srstoragetabpage.h \
    tabpages/networktabpage.h \
    tabpages/nicstabpage.h \
    tabpages/consoletabpage.h \
    tabpages/cvmconsoletabpage.h \
    tabpages/snapshotstabpage.h \
    tabpages/performancetabpage.h \
    tabpages/bootoptionstab.h \
    tabpages/memorytabpage.h \
    tabpages/searchtabpage.h \
    tabpages/notificationsbasepage.h \
    tabpages/alertsummarypage.h \
    tabpages/eventspage.h \
    widgets/qvncclient.h \
    widgets/memorybar.h \
    widgets/progressbardelegate.h \
    widgets/verticaltabwidget.h \
    navigation/navigationpane.h \
    navigation/navigationview.h \
    widgets/notificationsview.h \
    widgets/notificationssubmodeitem.h \
    navigation/navigationbuttons.h \
    widgets/wizardnavigationpane.h \
    network/httpconnect.h \
    navigation/navigationhistory.h \
    ConsoleView/IRemoteConsole.h \
    ConsoleView/ConsoleKeyHandler.h \
    ConsoleView/VNCGraphicsClient.h \
    ConsoleView/RdpClient.h \
    ConsoleView/XSVNCScreen.h \
    ConsoleView/VNCTabView.h \
    ConsoleView/VNCView.h \
    ConsoleView/ConsolePanel.h

# UI files
FORMS += \
    mainwindow.ui \
    dialogs/connectdialog.ui \
    dialogs/debugwindow.ui \
    dialogs/ballooningdialog.ui \
    dialogs/commanderrordialog.ui \
    dialogs/newpooldialog.ui \
    dialogs/bondpropertiesdialog.ui \
    dialogs/networkingpropertiesdialog.ui \
    dialogs/vifdialog.ui \
    dialogs/vmsnapshotdialog.ui \
    dialogs/vdipropertiesdialog.ui \
    dialogs/attachvirtualdiskdialog.ui \
    dialogs/newvirtualdiskdialog.ui \
    dialogs/movevirtualdiskdialog.ui \
    dialogs/optionsdialog.ui \
    dialogs/newvmwizard.ui \
    dialogs/newsrwizard.ui \
    dialogs/verticallytabbeddialog.ui \
    controls/affinitypicker.ui \
    settingspanels/generaleditpage.ui \
    settingspanels/hostautostarteditpage.ui \
    settingspanels/hostmultipathpage.ui \
    settingspanels/hostpoweroneditpage.ui \
    settingspanels/logdestinationeditpage.ui \
    settingspanels/cpumemoryeditpage.ui \
    settingspanels/bootoptionseditpage.ui \
    settingspanels/vmhaeditpage.ui \
    settingspanels/customfieldsdisplaypage.ui \
    settingspanels/perfmonalerteditpage.ui \
    settingspanels/homeservereditpage.ui \
    settingspanels/vmadvancededitpage.ui \
    settingspanels/vmenlightenmenteditpage.ui \
    dialogs/optionspages/securityoptionspage.ui \
    dialogs/optionspages/displayoptionspage.ui \
    dialogs/optionspages/confirmationoptionspage.ui \
    dialogs/optionspages/consolesoptionspage.ui \
    dialogs/optionspages/saveandrestoreoptionspage.ui \
    dialogs/optionspages/connectionoptionspage.ui \
    tabpages/generaltabpage.ui \
    tabpages/physicalstoragetabpage.ui \
    tabpages/srstoragetabpage.ui \
    tabpages/networktabpage.ui \
    tabpages/nicstabpage.ui \
    tabpages/consoletabpage.ui \
    tabpages/cvmconsoletabpage.ui \
    tabpages/snapshotstabpage.ui \
    tabpages/performancetabpage.ui \
    tabpages/memorytabpage.ui \
    tabpages/vmstoragetabpage.ui \
    tabpages/alertsummarypage.ui \
    tabpages/eventspage.ui \
    navigation/navigationpane.ui \
    navigation/navigationview.ui \
    widgets/notificationsview.ui \
    widgets/wizardnavigationpane.ui \
    ConsoleView/VNCTabView.ui \
    ConsoleView/ConsolePanel.ui

# Resources (empty for now)
RESOURCES += resources.qrc

# Link with xenlib
INCLUDEPATH += ../xenlib

# Windows: using xenlib as a static lib; avoid dllimport in headers
win32:DEFINES += XENLIB_STATIC

# On Windows (MinGW/MSVC), link against the sibling build output
# so the linker finds libxenlib.a (debug) or the release variant.
win32 {
    CONFIG(debug, debug|release) {
        LIBS += -L$$OUT_PWD/../xenlib/debug -lxenlib
        PRE_TARGETDEPS += $$OUT_PWD/../xenlib/debug/libxenlib.a
    } else {
        LIBS += -L$$OUT_PWD/../xenlib/release -lxenlib
        PRE_TARGETDEPS += $$OUT_PWD/../xenlib/release/libxenlib.a
    }
} else {
    # On Unix-like platforms keep existing behavior
    LIBS += -L../xenlib -lxenlib
}

# RDP support configuration (platform-specific)
# - Linux: Optional FreeRDP library
# - macOS: Optional FreeRDP via Homebrew
# - Windows: Native RDP (TODO: implement Windows-specific RDP client)

unix:!macx {
    # Linux: Check if FreeRDP is available using pkg-config
    system(pkg-config --exists freerdp2) {
        message("FreeRDP found - enabling RDP support")
        DEFINES += HAVE_FREERDP
        LIBS += -lfreerdp2 -lfreerdp-client2 -lwinpr2
        INCLUDEPATH += /usr/include/freerdp2 /usr/include/winpr2
    } else {
        message("FreeRDP not found - RDP support disabled (VNC-only mode)")
        message("Install FreeRDP: sudo apt-get install libfreerdp-dev libfreerdp-client2 libwinpr2-dev")
    }
}

macx {
    # macOS: Check if FreeRDP is available via Homebrew
    system(pkg-config --exists freerdp2) {
        message("FreeRDP found - enabling RDP support")
        DEFINES += HAVE_FREERDP
        LIBS += -lfreerdp2 -lfreerdp-client2 -lwinpr2
        INCLUDEPATH += $$system(brew --prefix freerdp)/include/freerdp2
        INCLUDEPATH += $$system(brew --prefix freerdp)/include/winpr2
    } else {
        message("FreeRDP not found - RDP support disabled (VNC-only mode)")
        message("Install FreeRDP: brew install freerdp")
    }
}

win32 {
    # Windows: RDP support disabled for now (needs native Windows implementation)
    # TODO: Implement Windows-specific RDP client using native Windows RDP APIs
    message("RDP support disabled on Windows (VNC-only mode)")
    message("Windows native RDP implementation pending - will fall back to VNC")
}

# Installation
target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
