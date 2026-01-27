TEMPLATE = app
CONFIG += qt warn_on c++17
QT += widgets network xml charts
TARGET = xenadmin-qt

greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += QT_NO_DEPRECATED_WARNINGS
}

# Source files
SOURCES += \
    commands/host/hostcommand.cpp \
    commands/vm/vmcommand.cpp \
    dialogs/actionprogressdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    mainwindowtreebuilder.cpp \
    settingspanels/ieditpage.cpp \
    tabpages/vmstoragetabpage.cpp \
    titlebar.cpp \
    placeholderwidget.cpp \
    settingsmanager.cpp \
    iconmanager.cpp \
    connectionprofile.cpp \
    dialogs/addserverdialog.cpp \
    dialogs/connectingtoserverdialog.cpp \
    dialogs/reconnectasdialog.cpp \
    dialogs/debugwindow.cpp \
    dialogs/xencacheexplorer.cpp \
    dialogs/aboutdialog.cpp \
    dialogs/ballooningdialog.cpp \
    dialogs/commanderrordialog.cpp \
    dialogs/confirmvmdeletedialog.cpp \
    dialogs/copyvmdialog.cpp \
    dialogs/movevmdialog.cpp \
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
    dialogs/snapshotpropertiesdialog.cpp \
    dialogs/storagepropertiesdialog.cpp \
    dialogs/networkpropertiesdialog.cpp \
    dialogs/bondpropertiesdialog.cpp \
    dialogs/networkingpropertiesdialog.cpp \
    dialogs/vifdialog.cpp \
    dialogs/vmsnapshotdialog.cpp \
    dialogs/vdipropertiesdialog.cpp \
    dialogs/repairsrdialog.cpp \
    dialogs/attachvirtualdiskdialog.cpp \
    dialogs/newvirtualdiskdialog.cpp \
    dialogs/movevirtualdiskdialog.cpp \
    dialogs/migratevirtualdiskdialog.cpp \
    dialogs/optionsdialog.cpp \
    dialogs/verticallytabbeddialog.cpp \
    dialogs/customsearchdialog.cpp \
    dialogs/crosspoolmigratewizard.cpp \
    dialogs/crosspoolmigratewizardpages.cpp \
    dialogs/crosspoolmigratewizard_copymodepage.cpp \
    dialogs/crosspoolmigratewizard_intrapoolcopypage.cpp \
    dialogs/warningdialogs/closexencenterwarningdialog.cpp \
    controls/affinitypicker.cpp \
    controls/srpicker.cpp \
    controls/tooltipcontainer.cpp \
    controls/decentgroupbox.cpp \
    controls/dropdownbutton.cpp \
    controls/snapshottreeview.cpp \
    controls/multipledvdisolist.cpp \
    controls/vmoperationmenu.cpp \
    controls/pdsection.cpp \
    controls/xensearch/querypanel.cpp \
    controls/xensearch/searchoutput.cpp \
    controls/xensearch/foldernavigator.cpp \
    controls/xensearch/groupingcontrol.cpp \
    controls/xensearch/queryelement.cpp \
    controls/xensearch/querytype.cpp \
    controls/xensearch/searcher.cpp \
    controls/xensearch/resourceselectbutton.cpp \
    controls/xensearch/treewidgetgroupacceptor.cpp \
    xensearch/treesearch.cpp \
    xensearch/treenodegroupacceptor.cpp \
    xensearch/treenodefactory.cpp \
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
    settingspanels/pooladvancededitpage.cpp \
    settingspanels/securityeditpage.cpp \
    settingspanels/livepatchingeditpage.cpp \
    settingspanels/networkoptionseditpage.cpp \
    settingspanels/networkgeneraleditpage.cpp \
    dialogs/optionspages/securityoptionspage.cpp \
    dialogs/optionspages/displayoptionspage.cpp \
    dialogs/optionspages/confirmationoptionspage.cpp \
    dialogs/optionspages/consolesoptionspage.cpp \
    dialogs/optionspages/saveandrestoreoptionspage.cpp \
    dialogs/optionspages/connectionoptionspage.cpp \
    operations/operationmanager.cpp \
    selectionmanager.cpp \
    actions/meddlingaction.cpp \
    actions/meddlingactionmanager.cpp \
    commands/command.cpp \
    commands/contextmenubuilder.cpp \
    commands/connection/cancelhostconnectioncommand.cpp \
    commands/connection/disconnecthostsandpoolscommand.cpp \
    commands/connection/forgetsavedpasswordcommand.cpp \
    commands/connection/disconnectcommand.cpp \
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
    commands/host/certificatecommand.cpp \
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
    commands/vm/crosspoolmigratecommand.cpp \
    commands/vm/crosspoolmovevmcommand.cpp \
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
    commands/vm/newvmfromsnapshotcommand.cpp \
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
    commands/storage/srcommand.cpp \
    commands/storage/vbdcommand.cpp \
    commands/storage/vdicommand.cpp \
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
    commands/pool/addhosttopoolcommand.cpp \
    commands/pool/addnewhosttopoolcommand.cpp \
    commands/pool/addhosttoselectedpoolmenu.cpp \
    commands/pool/addselectedhosttopoolmenu.cpp \
    commands/pool/poolcommand.cpp \
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
    controls/memorybar.cpp \
    controls/shinybar.cpp \
    controls/vmshinybar.cpp \
    controls/hostshinybar.cpp \
    widgets/vmmemorycontrols.cpp \
    widgets/vmmemoryrow.cpp \
    controls/hostmemoryrow.cpp \
    widgets/progressbardelegate.cpp \
    widgets/verticaltabwidget.cpp \
    navigation/navigationpane.cpp \
    navigation/navigationview.cpp \
    widgets/notificationsview.cpp \
    widgets/notificationssubmodeitem.cpp \
    navigation/navigationbuttons.cpp \
    widgets/wizardnavigationpane.cpp \
    network/httpconnect.cpp \
    network/xenconnectionui.cpp \
    ConsoleView/ConsoleKeyHandler.cpp \
    ConsoleView/VNCGraphicsClient.cpp \
    ConsoleView/RdpClient.cpp \
    ConsoleView/XSVNCScreen.cpp \
    ConsoleView/VNCTabView.cpp \
    ConsoleView/VNCView.cpp \
    ConsoleView/ConsolePanel.cpp \
    widgets/isodropdownbox.cpp \
    widgets/cdchanger.cpp \
    navigation/navigationhistory.cpp

# Header files
HEADERS += \
    commands/host/hostcommand.h \
    commands/vm/vmcommand.h \
    dialogs/actionprogressdialog.h \
    globals.h \
    mainwindow.h \
    mainwindowtreebuilder.h \
    selectionmanager.h \
    tabpages/vmstoragetabpage.h \
    titlebar.h \
    placeholderwidget.h \
    settingsmanager.h \
    iconmanager.h \
    connectionprofile.h \
    dialogs/addserverdialog.h \
    dialogs/connectingtoserverdialog.h \
    dialogs/reconnectasdialog.h \
    dialogs/debugwindow.h \
    dialogs/xencacheexplorer.h \
    dialogs/aboutdialog.h \
    dialogs/ballooningdialog.h \
    dialogs/commanderrordialog.h \
    dialogs/confirmvmdeletedialog.h \
    dialogs/copyvmdialog.h \
    dialogs/movevmdialog.h \
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
    dialogs/snapshotpropertiesdialog.h \
    dialogs/storagepropertiesdialog.h \
    dialogs/networkpropertiesdialog.h \
    dialogs/bondpropertiesdialog.h \
    dialogs/networkingpropertiesdialog.h \
    dialogs/vifdialog.h \
    dialogs/vmsnapshotdialog.h \
    dialogs/vdipropertiesdialog.h \
    dialogs/repairsrdialog.h \
    dialogs/attachvirtualdiskdialog.h \
    widgets/isodropdownbox.h \
    widgets/cdchanger.h \
    dialogs/newvirtualdiskdialog.h \
    dialogs/movevirtualdiskdialog.h \
    dialogs/migratevirtualdiskdialog.h \
    dialogs/optionsdialog.h \
    dialogs/verticallytabbeddialog.h \
    dialogs/customsearchdialog.h \
    dialogs/crosspoolmigratewizard.h \
    dialogs/crosspoolmigratewizardpages.h \
    dialogs/crosspoolmigratewizard_copymodepage.h \
    dialogs/crosspoolmigratewizard_intrapoolcopypage.h \
    dialogs/warningdialogs/closexencenterwarningdialog.h \
    network/xenconnectionui.h \
    controls/affinitypicker.h \
    controls/srpicker.h \
    controls/tooltipcontainer.h \
    controls/decentgroupbox.h \
    controls/dropdownbutton.h \
    controls/vmoperationmenu.h \
    controls/snapshottreeview.h \
    controls/multipledvdisolist.h \
    controls/pdsection.h \
    controls/xensearch/querypanel.h \
    controls/xensearch/searchoutput.h \
    controls/xensearch/foldernavigator.h \
    controls/xensearch/groupingcontrol.h \
    controls/xensearch/queryelement.h \
    controls/xensearch/querytype.h \
    controls/xensearch/searcher.h \
    controls/xensearch/resourceselectbutton.h \
    controls/xensearch/treewidgetgroupacceptor.h \
    xensearch/treesearch.h \
    xensearch/treenodegroupacceptor.h \
    xensearch/treenodefactory.h \
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
    settingspanels/pooladvancededitpage.h \
    settingspanels/securityeditpage.h \
    settingspanels/livepatchingeditpage.h \
    settingspanels/networkoptionseditpage.h \
    settingspanels/networkgeneraleditpage.h \
    dialogs/optionspages/ioptionspage.h \
    dialogs/optionspages/securityoptionspage.h \
    dialogs/optionspages/displayoptionspage.h \
    dialogs/optionspages/confirmationoptionspage.h \
    dialogs/optionspages/consolesoptionspage.h \
    dialogs/optionspages/saveandrestoreoptionspage.h \
    dialogs/optionspages/connectionoptionspage.h \
    operations/operationmanager.h \
    actions/meddlingaction.h \
    actions/meddlingactionmanager.h \
    commands/command.h \
    commands/contextmenubuilder.h \
    commands/connection/disconnectcommand.h \
    commands/connection/cancelhostconnectioncommand.h \
    commands/connection/disconnecthostsandpoolscommand.h \
    commands/connection/forgetsavedpasswordcommand.h \
    commands/host/certificatecommand.h \
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
    commands/vm/crosspoolmigratecommand.h \
    commands/vm/crosspoolmovevmcommand.h \
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
    commands/vm/newvmfromsnapshotcommand.h \
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
    commands/storage/srcommand.h \
    commands/storage/vbdcommand.h \
    commands/storage/vdicommand.h \
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
    commands/pool/addhosttopoolcommand.h \
    commands/pool/addnewhosttopoolcommand.h \
    commands/pool/addhosttoselectedpoolmenu.h \
    commands/pool/addselectedhosttopoolmenu.h \
    commands/pool/poolcommand.h \
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
    controls/memorybar.h \
    controls/shinybar.h \
    controls/vmshinybar.h \
    controls/hostshinybar.h \
    widgets/vmmemorycontrols.h \
    widgets/vmmemoryrow.h \
    controls/hostmemoryrow.h \
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
    dialogs/addserverdialog.ui \
    dialogs/connectingtoserverdialog.ui \
    dialogs/reconnectasdialog.ui \
    dialogs/debugwindow.ui \
    dialogs/xencacheexplorer.ui \
    dialogs/ballooningdialog.ui \
    dialogs/commanderrordialog.ui \
    dialogs/confirmvmdeletedialog.ui \
    dialogs/copyvmdialog.ui \
    dialogs/movevmdialog.ui \
    dialogs/newpooldialog.ui \
    dialogs/bondpropertiesdialog.ui \
    dialogs/repairsrdialog.ui \
    dialogs/networkingpropertiesdialog.ui \
    dialogs/vifdialog.ui \
    dialogs/vmsnapshotdialog.ui \
    dialogs/vdipropertiesdialog.ui \
    dialogs/attachvirtualdiskdialog.ui \
    dialogs/newvirtualdiskdialog.ui \
    dialogs/movevirtualdiskdialog.ui \
    dialogs/optionsdialog.ui \
    dialogs/newnetworkwizard.ui \
    dialogs/newvmwizard.ui \
    dialogs/newsrwizard.ui \
    dialogs/crosspoolmigratewizard.ui \
    dialogs/crosspoolmigratewizard_copymodepage.ui \
    dialogs/crosspoolmigratewizard_intrapoolcopypage.ui \
    dialogs/verticallytabbeddialog.ui \
    dialogs/customsearchdialog.ui \
    dialogs/warningdialogs/closexencenterwarningdialog.ui \
    controls/affinitypicker.ui \
    controls/srpicker.ui \
    controls/multipledvdisolist.ui \
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
    settingspanels/pooladvancededitpage.ui \
    settingspanels/securityeditpage.ui \
    settingspanels/livepatchingeditpage.ui \
    settingspanels/networkoptionseditpage.ui \
    settingspanels/networkgeneraleditpage.ui \
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
    controls/hostmemoryrow.ui \
    navigation/navigationpane.ui \
    navigation/navigationview.ui \
    widgets/notificationsview.ui \
    widgets/wizardnavigationpane.ui \
    ConsoleView/VNCTabView.ui \
    ConsoleView/ConsolePanel.ui

RESOURCES += resources.qrc

# Link with xenlib
INCLUDEPATH += .. ../xenlib

# Windows: using xenlib as a static lib; avoid dllimport in headers
win32:DEFINES += XENLIB_STATIC

# On Windows (MinGW/MSVC), build completely statically, so that we can monolithic .exe file
win32 {
    CONFIG += static
    QMAKE_LFLAGS += -static -static-libstdc++ -static-libgcc
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
