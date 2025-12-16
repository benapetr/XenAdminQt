/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "navigationpane.h"
#include "ui_navigationpane.h"
#include "navigationbuttons.h"
#include "navigationview.h"
#include "notificationsview.h"
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QDebug>

NavigationPane::NavigationPane(QWidget* parent)
    : QWidget(parent), ui(new Ui::NavigationPane), m_currentMode(Infrastructure), m_lastNotificationsMode(Alerts), m_buttonInfraBig(nullptr), m_buttonObjectsBig(nullptr), m_buttonOrganizationBig(nullptr), m_buttonSearchesBig(nullptr), m_buttonNotifyBig(nullptr), m_buttonInfraSmall(nullptr), m_buttonObjectsSmall(nullptr), m_buttonOrganizationSmall(nullptr), m_buttonSearchesSmall(nullptr), m_buttonNotifySmall(nullptr)
{
    ui->setupUi(this);

    // Create NavigationView and NotificationsView
    // These replace the placeholders in the UI
    NavigationView* navView = new NavigationView(this);
    NotificationsView* notifView = new NotificationsView(this);

    // Replace placeholder widgets with actual views
    QVBoxLayout* navPlaceholderLayout = qobject_cast<QVBoxLayout*>(
        ui->navigationViewPlaceholder->layout());
    if (navPlaceholderLayout)
    {
        navPlaceholderLayout->addWidget(navView);
    }

    QVBoxLayout* notifPlaceholderLayout = qobject_cast<QVBoxLayout*>(
        ui->notificationsViewPlaceholder->layout());
    if (notifPlaceholderLayout)
    {
        notifPlaceholderLayout->addWidget(notifView);
    }

    // Initially show navigation view, hide notifications view
    ui->navigationViewPlaceholder->setVisible(true);
    ui->notificationsViewPlaceholder->setVisible(false);

    // Create navigation buttons
    setupNavigationButtons();

    // Wire up NavigationView events to forward them
    connect(navView, &NavigationView::treeViewSelectionChanged,
            this, &NavigationPane::onNavigationViewSelectionChanged);
    connect(navView, &NavigationView::treeNodeBeforeSelected,
            this, &NavigationPane::onNavigationViewBeforeSelected);
    connect(navView, &NavigationView::treeNodeClicked,
            this, &NavigationPane::onNavigationViewClicked);
    connect(navView, &NavigationView::treeNodeRightClicked,
            this, &NavigationPane::onNavigationViewRightClicked);
    connect(navView, &NavigationView::treeViewRefreshed,
            this, &NavigationPane::onNavigationViewRefreshed);
    connect(navView, &NavigationView::treeViewRefreshSuspended,
            this, &NavigationPane::onNavigationViewRefreshSuspended);
    connect(navView, &NavigationView::treeViewRefreshResumed,
            this, &NavigationPane::onNavigationViewRefreshResumed);
    connect(navView, &NavigationView::dragDropCommandActivated,
            this, &NavigationPane::onNavigationViewDragDropCommand);

    // Wire up NotificationsView events
    connect(notifView, &NotificationsView::notificationsSubModeChanged,
            this, &NavigationPane::onNotificationsSubModeChanged);

    // Set initial mode
    m_buttonInfraBig->setChecked(true);
}

NavigationPane::~NavigationPane()
{
    delete ui;
}

void NavigationPane::setupNavigationButtons()
{
    // Create big buttons (vertical toolbar in Panel2)
    m_buttonInfraBig = new NavigationButtonBig(this);
    m_buttonInfraBig->setText("Infrastructure");
    m_buttonInfraBig->setIcon(QIcon(":/icons/infra_view_24.png")); // TODO: Add icons
    m_buttonInfraBig->setTag(QVariant::fromValue(Infrastructure));

    m_buttonObjectsBig = new NavigationButtonBig(this);
    m_buttonObjectsBig->setText("Objects");
    m_buttonObjectsBig->setIcon(QIcon(":/icons/objects_24.png"));
    m_buttonObjectsBig->setTag(QVariant::fromValue(Objects));

    m_buttonOrganizationBig = new NavigationDropDownButtonBig(this);
    m_buttonOrganizationBig->setText("Organization");
    m_buttonOrganizationBig->setIcon(QIcon(":/icons/org_view_24.png"));

    m_buttonSearchesBig = new NavigationDropDownButtonBig(this);
    m_buttonSearchesBig->setText("Saved Searches");
    m_buttonSearchesBig->setIcon(QIcon(":/icons/saved_searches_24.png"));

    m_buttonNotifyBig = new NotificationButtonBig(this);
    m_buttonNotifyBig->setText("Notifications");
    m_buttonNotifyBig->setIcon(QIcon(":/icons/notif_none_24.png"));
    m_buttonNotifyBig->setTag(QVariant::fromValue(Notifications));

    // Create small buttons (horizontal toolbar, shown when pane is narrow)
    m_buttonInfraSmall = new NavigationButtonSmall(this);
    m_buttonInfraSmall->setIcon(QIcon(":/icons/infra_view_16.png"));
    m_buttonInfraSmall->setToolTip("Infrastructure");
    m_buttonInfraSmall->setTag(QVariant::fromValue(Infrastructure));

    m_buttonObjectsSmall = new NavigationButtonSmall(this);
    m_buttonObjectsSmall->setIcon(QIcon(":/icons/objects_16.png"));
    m_buttonObjectsSmall->setToolTip("Objects");
    m_buttonObjectsSmall->setTag(QVariant::fromValue(Objects));

    m_buttonOrganizationSmall = new NavigationDropDownButtonSmall(this);
    m_buttonOrganizationSmall->setIcon(QIcon(":/icons/org_view_16.png"));
    m_buttonOrganizationSmall->setToolTip("Organization");

    m_buttonSearchesSmall = new NavigationDropDownButtonSmall(this);
    m_buttonSearchesSmall->setIcon(QIcon(":/icons/saved_searches_16.png"));
    m_buttonSearchesSmall->setToolTip("Saved Searches");

    m_buttonNotifySmall = new NotificationButtonSmall(this);
    m_buttonNotifySmall->setIcon(QIcon(":/icons/notif_none_16.png"));
    m_buttonNotifySmall->setToolTip("Notifications");
    m_buttonNotifySmall->setTag(QVariant::fromValue(Notifications));

    // Pair buttons (big + small sync selection)
    addNavigationItemPair(m_buttonInfraBig, m_buttonInfraSmall);
    addNavigationItemPair(m_buttonObjectsBig, m_buttonObjectsSmall);
    addNavigationItemPair(m_buttonOrganizationBig, m_buttonOrganizationSmall);
    addNavigationItemPair(m_buttonSearchesBig, m_buttonSearchesSmall);
    addNavigationItemPair(m_buttonNotifyBig, m_buttonNotifySmall);

    // Add big buttons to toolStripBig placeholder
    QVBoxLayout* bigButtonLayout = qobject_cast<QVBoxLayout*>(
        ui->toolStripBigPlaceholder->layout());
    if (bigButtonLayout)
    {
        bigButtonLayout->addWidget(m_buttonInfraBig);
        bigButtonLayout->addWidget(m_buttonObjectsBig);
        bigButtonLayout->addWidget(m_buttonOrganizationBig);
        bigButtonLayout->addWidget(m_buttonSearchesBig);
        bigButtonLayout->addWidget(m_buttonNotifyBig);
        bigButtonLayout->addStretch(); // Push buttons to top
    }

    // Add small buttons to toolStripSmall placeholder
    QHBoxLayout* smallButtonLayout = qobject_cast<QHBoxLayout*>(
        ui->toolStripSmallPlaceholder->layout());
    if (smallButtonLayout)
    {
        smallButtonLayout->addStretch(); // Push buttons to right
        smallButtonLayout->addWidget(m_buttonInfraSmall);
        smallButtonLayout->addWidget(m_buttonObjectsSmall);
        smallButtonLayout->addWidget(m_buttonOrganizationSmall);
        smallButtonLayout->addWidget(m_buttonSearchesSmall);
        smallButtonLayout->addWidget(m_buttonNotifySmall);
    }

    // Populate dropdown menus
    populateOrganizationDropDown();
    populateSearchDropDown();

    // Connect big button signals
    connect(m_buttonInfraBig, &NavigationButtonBig::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });
    connect(m_buttonObjectsBig, &NavigationButtonBig::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });
    connect(m_buttonNotifyBig, &NotificationButtonBig::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });

    // Connect small button signals (for collapsed toolbar state)
    connect(m_buttonInfraSmall, &NavigationButtonSmall::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });
    connect(m_buttonObjectsSmall, &NavigationButtonSmall::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });
    connect(m_buttonNotifySmall, &NotificationButtonSmall::navigationViewChanged,
            this, [this]() { onNavigationButtonChecked(); });
}

void NavigationPane::addNavigationItemPair(QObject* bigButton, QObject* smallButton)
{
    // Cast to INavigationItem interface using dynamic_cast
    INavigationItem* bigItem = dynamic_cast<INavigationItem*>(bigButton);
    INavigationItem* smallItem = dynamic_cast<INavigationItem*>(smallButton);

    if (bigItem && smallItem)
    {
        bigItem->setPairedItem(smallItem);
        smallItem->setPairedItem(bigItem);
    }
}

void NavigationPane::populateOrganizationDropDown()
{
    // Matches C# NavigationPane.PopulateOrganizationDropDown
    // TODO: Implement organization dropdown population
    // QMenu* menu = m_buttonOrganizationBig->dropDownMenu();

    QAction* tagsAction = new QAction(QIcon(":/icons/tag_16.png"), "Tags", this);
    tagsAction->setData(QVariant::fromValue(Tags));
    connect(tagsAction, &QAction::triggered, this, &NavigationPane::onOrganizationMenuItemTriggered);

    QAction* foldersAction = new QAction(QIcon(":/icons/folder_16.png"), "Folders", this);
    foldersAction->setData(QVariant::fromValue(Folders));
    connect(foldersAction, &QAction::triggered, this, &NavigationPane::onOrganizationMenuItemTriggered);

    QAction* fieldsAction = new QAction(QIcon(":/icons/fields_16.png"), "Custom Fields", this);
    fieldsAction->setData(QVariant::fromValue(CustomFields));
    connect(fieldsAction, &QAction::triggered, this, &NavigationPane::onOrganizationMenuItemTriggered);

    QAction* vappsAction = new QAction(QIcon(":/icons/vapp_16.png"), "vApps", this);
    vappsAction->setData(QVariant::fromValue(vApps));
    connect(vappsAction, &QAction::triggered, this, &NavigationPane::onOrganizationMenuItemTriggered);

    QList<QAction*> items = {tagsAction, foldersAction, fieldsAction, vappsAction};
    m_buttonOrganizationBig->setItemList(items);
    m_buttonOrganizationSmall->setItemList(items);
}

void NavigationPane::populateSearchDropDown()
{
    // TODO: Implement when Search infrastructure is ported
    // Matches C# NavigationPane.PopulateSearchDropDown
    // Should populate from Search.Searches array
}

void NavigationPane::onNavigationButtonChecked()
{
    QObject* senderObj = QObject::sender();

    // Determine which mode was selected
    NavigationMode newMode = m_currentMode;

    if (senderObj == m_buttonInfraBig || senderObj == m_buttonInfraSmall)
    {
        newMode = Infrastructure;
    } else if (senderObj == m_buttonObjectsBig || senderObj == m_buttonObjectsSmall)
    {
        newMode = Objects;
    } else if (senderObj == m_buttonNotifyBig || senderObj == m_buttonNotifySmall)
    {
        newMode = Notifications;
    }

    if (newMode != m_currentMode)
    {
        m_currentMode = newMode;
        onNavigationModeChanged();
        emit navigationModeChanged(m_currentMode);
    }
}

void NavigationPane::onOrganizationMenuItemTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action)
        return;

    NavigationMode mode = action->data().value<NavigationMode>();
    if (mode != m_currentMode)
    {
        m_currentMode = mode;

        // Check the organization button since we're in organization mode
        m_buttonOrganizationBig->setChecked(true);

        onNavigationModeChanged();
        emit navigationModeChanged(m_currentMode);
    }
}

void NavigationPane::onSearchMenuItemTriggered()
{
    // TODO: Implement when Search infrastructure is ported
}

void NavigationPane::onNavigationModeChanged()
{
    // Matches C# NavigationPane.OnNavigationModeChanged
    if (m_currentMode == Notifications)
    {
        // Switch to notifications view
        ui->navigationViewPlaceholder->setVisible(false);
        ui->notificationsViewPlaceholder->setVisible(true);

        // Select last notifications mode
        NotificationsView* notifView = findChild<NotificationsView*>();
        if (notifView)
        {
            notifView->selectNotificationsSubMode(m_lastNotificationsMode);
        }
    } else
    {
        // Switch to navigation view
        ui->notificationsViewPlaceholder->setVisible(false);
        ui->navigationViewPlaceholder->setVisible(true);

        // Update tree view for new mode
        NavigationView* navView = navigationView();
        if (navView)
        {
            // Set the navigation mode so tree builder uses correct layout
            navView->setNavigationMode(m_currentMode);
            navView->resetSearchBox();
            // requestRefreshTreeView() is called by setNavigationMode()
            navView->focusTreeView();
        }
    }
}

// Event forwarding slots
void NavigationPane::onNavigationViewSelectionChanged()
{
    emit treeViewSelectionChanged();
}

void NavigationPane::onNavigationViewBeforeSelected()
{
    emit treeNodeBeforeSelected();
}

void NavigationPane::onNavigationViewClicked()
{
    emit treeNodeClicked();
}

void NavigationPane::onNavigationViewRightClicked()
{
    emit treeNodeRightClicked();
}

void NavigationPane::onNavigationViewRefreshed()
{
    emit treeViewRefreshed();
}

void NavigationPane::onNavigationViewRefreshSuspended()
{
    emit treeViewRefreshSuspended();
}

void NavigationPane::onNavigationViewRefreshResumed()
{
    emit treeViewRefreshResumed();
}

void NavigationPane::onNavigationViewDragDropCommand(const QString& commandKey)
{
    emit dragDropCommandActivated(commandKey);
}

void NavigationPane::onNotificationsSubModeChanged(NotificationsSubMode subMode)
{
    m_lastNotificationsMode = subMode;
    emit notificationsSubModeChanged(subMode);
}

// Public methods
NavigationView* NavigationPane::navigationView() const
{
    return findChild<NavigationView*>();
}

NotificationsView* NavigationPane::notificationsView() const
{
    return findChild<NotificationsView*>();
}

void NavigationPane::updateNotificationsButton(NotificationsSubMode mode, int entries)
{
    NotificationsView* notifView = notificationsView();
    if (notifView)
    {
        notifView->updateEntries(mode, entries);
        int totalEntries = notifView->getTotalEntries();
        m_buttonNotifyBig->setUnreadEntries(totalEntries);
        m_buttonNotifySmall->setUnreadEntries(totalEntries);
    }
}

void NavigationPane::switchToInfrastructureMode()
{
    if (!m_buttonInfraBig->isChecked())
    {
        m_buttonInfraBig->setChecked(true);
    }
}

void NavigationPane::switchToNotificationsView(NotificationsSubMode subMode)
{
    // Check the notification button if switching programmatically
    if (!m_buttonNotifyBig->isChecked())
    {
        m_buttonNotifyBig->setChecked(true);
    }

    NotificationsView* notifView = notificationsView();
    if (notifView)
    {
        notifView->selectNotificationsSubMode(subMode);
    }
}

void NavigationPane::focusTreeView()
{
    NavigationView* navView = navigationView();
    if (navView)
    {
        navView->focusTreeView();
    }
}

void NavigationPane::requestRefreshTreeView()
{
    NavigationView* navView = navigationView();
    if (navView)
    {
        navView->requestRefreshTreeView();
    }
}

void NavigationPane::updateSearch()
{
    // Matches C# NavigationPane.UpdateSearch (line 196)
    // Update NavigationView's CurrentSearch based on current mode
    // TODO: Once Search infrastructure is ported from C#, this will set
    // navigationView->setCurrentSearch(getSearchForMode(m_currentMode))
    // For now, just trigger a refresh
    NavigationView* navView = navigationView();
    if (navView)
    {
        navView->requestRefreshTreeView();
    }
}

void NavigationPane::setInSearchMode(bool enabled)
{
    // Matches C# NavigationPane.InSearchMode property
    NavigationView* navView = navigationView();
    if (navView)
    {
        // TODO: Implement when NavigationView gains InSearchMode property
        // navView->setInSearchMode(enabled);
        Q_UNUSED(enabled);
    }
}

void NavigationPane::setXenLib(XenLib* xenLib)
{
    // Pass XenLib to NavigationView for tree building
    NavigationView* navView = navigationView();
    if (navView)
    {
        navView->setXenLib(xenLib);
    }
}

void NavigationPane::resizeEvent(QResizeEvent* event)
{
    // Matches C# NavigationPane.OnResize
    // Preserve Panel2 height during resize
    QWidget::resizeEvent(event);

    QSplitter* splitter = ui->splitContainer;
    int panel2Height = splitter->widget(1)->height();
    int totalHeight = splitter->height();
    int splitterWidth = splitter->handleWidth();

    // Calculate new splitter distance to preserve panel2Height
    int newDistance = totalHeight - panel2Height - splitterWidth;
    if (newDistance > 0)
    {
        QList<int> sizes;
        sizes << newDistance << panel2Height;
        splitter->setSizes(sizes);
    }
}
