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
#include "../widgets/notificationsview.h"
#include "../settingsmanager.h"
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QDebug>
#include <QMetaType>
#include <QTimer>
#include <QSplitter>

NavigationPane::NavigationPane(QWidget* parent) : QWidget(parent), ui(new Ui::NavigationPane), m_currentMode(Infrastructure), m_lastNotificationsMode(Alerts), m_buttonInfraBig(nullptr), m_buttonObjectsBig(nullptr), m_buttonOrganizationBig(nullptr), m_buttonSearchesBig(nullptr), m_buttonNotifyBig(nullptr), m_buttonInfraSmall(nullptr), m_buttonObjectsSmall(nullptr), m_buttonOrganizationSmall(nullptr), m_buttonSearchesSmall(nullptr), m_buttonNotifySmall(nullptr)
{
    // Required for queued delivery of navigation enums on Qt5.
    qRegisterMetaType<NavigationPane::NavigationMode>("NavigationPane::NavigationMode");
    qRegisterMetaType<NavigationPane::NotificationsSubMode>("NavigationPane::NotificationsSubMode");

    this->ui->setupUi(this);

    // Create NavigationView and NotificationsView
    // These replace the placeholders in the UI
    this->m_currentNavigationView = new NavigationView(this);
    this->m_currentNotificationsView = new NotificationsView(this);

    // Replace placeholder widgets with actual views
    QVBoxLayout* navPlaceholderLayout = qobject_cast<QVBoxLayout*>(this->ui->navigationViewPlaceholder->layout());
    if (navPlaceholderLayout)
    {
        navPlaceholderLayout->addWidget(this->m_currentNavigationView);
    }

    QVBoxLayout* notifPlaceholderLayout = qobject_cast<QVBoxLayout*>(this->ui->notificationsViewPlaceholder->layout());
    if (notifPlaceholderLayout)
    {
        notifPlaceholderLayout->addWidget(this->m_currentNotificationsView);
    }

    // Initially show navigation view, hide notifications view
    this->ui->navigationViewPlaceholder->setVisible(true);
    this->ui->notificationsViewPlaceholder->setVisible(false);

    // Create navigation buttons
    this->setupNavigationButtons();
    this->m_buttonPanelHeight = this->m_buttonInfraBig->height() * 5;
    this->ui->panel2->setMaximumHeight(this->m_buttonPanelHeight);
    this->ui->panel2->installEventFilter(this);
    this->ui->splitContainer->setCollapsible(0, false);
    this->ui->splitContainer->setCollapsible(1, true);
    this->ui->splitContainer->setStretchFactor(0, 1);
    this->ui->splitContainer->setStretchFactor(1, 0);
    connect(this->ui->splitContainer, &QSplitter::splitterMoved, this, [this]()
    {
        this->m_buttonPanelHeight = this->ui->splitContainer->sizes().value(1);
        this->updateButtonLayout();
    });
    QTimer::singleShot(0, this, &NavigationPane::restoreButtonPanelHeight);

    // Wire up NavigationView events to forward them
    connect(this->m_currentNavigationView, &NavigationView::treeViewSelectionChanged, this, &NavigationPane::onNavigationViewSelectionChanged);
    connect(this->m_currentNavigationView, &NavigationView::treeNodeBeforeSelected, this, &NavigationPane::onNavigationViewBeforeSelected);
    connect(this->m_currentNavigationView, &NavigationView::treeNodeClicked, this, &NavigationPane::onNavigationViewClicked);
    connect(this->m_currentNavigationView, &NavigationView::treeNodeRightClicked, this, &NavigationPane::onNavigationViewRightClicked);
    connect(this->m_currentNavigationView, &NavigationView::treeViewRefreshed, this, &NavigationPane::onNavigationViewRefreshed);
    connect(this->m_currentNavigationView, &NavigationView::dragDropCommandActivated, this, &NavigationPane::onNavigationViewDragDropCommand);
    connect(this->m_currentNavigationView, &NavigationView::connectToServerRequested, this, &NavigationPane::onNavigationViewConnectToServerRequested);
    connect(this->m_currentNavigationView, &NavigationView::treeNodeActivated, this, &NavigationPane::treeNodeActivated);

    // Wire up NotificationsView events
    connect(this->m_currentNotificationsView, &NotificationsView::notificationsSubModeChanged, this, &NavigationPane::onNotificationsSubModeChanged);

    // Set initial mode
    this->m_buttonInfraBig->setChecked(true);
}

NavigationPane::~NavigationPane()
{
    delete this->ui;
}

void NavigationPane::setupNavigationButtons()
{
    // Create big buttons (vertical toolbar in Panel2)
    this->m_buttonInfraBig = new NavigationButtonBig(this);
    this->m_buttonInfraBig->setText("Infrastructure");
    this->m_buttonInfraBig->setIcon(QIcon(":/icons/infra_view_24.png"));
    this->m_buttonInfraBig->setTag(QVariant::fromValue(Infrastructure));

    this->m_buttonObjectsBig = new NavigationButtonBig(this);
    this->m_buttonObjectsBig->setText("Objects");
    this->m_buttonObjectsBig->setIcon(QIcon(":/icons/objects_24.png"));
    this->m_buttonObjectsBig->setTag(QVariant::fromValue(Objects));

    this->m_buttonOrganizationBig = new NavigationDropDownButtonBig(this);
    this->m_buttonOrganizationBig->setText("Organization");
    this->m_buttonOrganizationBig->setIcon(QIcon(":/icons/org_view_24.png"));

    this->m_buttonSearchesBig = new NavigationDropDownButtonBig(this);
    this->m_buttonSearchesBig->setText("Saved Searches");
    this->m_buttonSearchesBig->setIcon(QIcon(":/icons/saved_searches_24.png"));

    this->m_buttonNotifyBig = new NotificationButtonBig(this);
    this->m_buttonNotifyBig->setText("Notifications");
    this->m_buttonNotifyBig->setIcon(QIcon(":/icons/notif_none_24.png"));
    this->m_buttonNotifyBig->setTag(QVariant::fromValue(Notifications));

    // Create small buttons for the large buttons that overflow vertically.
    this->m_buttonInfraSmall = new NavigationButtonSmall(this);
    this->m_buttonInfraSmall->setIcon(QIcon(":/icons/infra_view_16.png"));
    this->m_buttonInfraSmall->setToolTip("Infrastructure");
    this->m_buttonInfraSmall->setTag(QVariant::fromValue(Infrastructure));

    this->m_buttonObjectsSmall = new NavigationButtonSmall(this);
    this->m_buttonObjectsSmall->setIcon(QIcon(":/icons/objects_16.png"));
    this->m_buttonObjectsSmall->setToolTip("Objects");
    this->m_buttonObjectsSmall->setTag(QVariant::fromValue(Objects));

    this->m_buttonOrganizationSmall = new NavigationDropDownButtonSmall(this);
    this->m_buttonOrganizationSmall->setIcon(QIcon(":/icons/org_view_16.png"));
    this->m_buttonOrganizationSmall->setToolTip("Organization");

    this->m_buttonSearchesSmall = new NavigationDropDownButtonSmall(this);
    this->m_buttonSearchesSmall->setIcon(QIcon(":/icons/saved_searches_16.png"));
    this->m_buttonSearchesSmall->setToolTip("Saved Searches");

    this->m_buttonNotifySmall = new NotificationButtonSmall(this);
    this->m_buttonNotifySmall->setIcon(QIcon(":/icons/notif_none_16.png"));
    this->m_buttonNotifySmall->setToolTip("Notifications");
    this->m_buttonNotifySmall->setTag(QVariant::fromValue(Notifications));

    // Pair buttons (big + small sync selection)
    this->addNavigationItemPair(this->m_buttonInfraBig, this->m_buttonInfraSmall);
    this->addNavigationItemPair(this->m_buttonObjectsBig, this->m_buttonObjectsSmall);
    this->addNavigationItemPair(this->m_buttonOrganizationBig, this->m_buttonOrganizationSmall);
    this->addNavigationItemPair(this->m_buttonSearchesBig, this->m_buttonSearchesSmall);
    this->addNavigationItemPair(this->m_buttonNotifyBig, this->m_buttonNotifySmall);

    // Add big buttons to toolStripBig placeholder
    QVBoxLayout* bigButtonLayout = qobject_cast<QVBoxLayout*>(this->ui->toolStripBigPlaceholder->layout());
    if (bigButtonLayout)
    {
        bigButtonLayout->addWidget(this->m_buttonInfraBig);
        bigButtonLayout->addWidget(this->m_buttonObjectsBig);
        bigButtonLayout->addWidget(this->m_buttonOrganizationBig);
        bigButtonLayout->addWidget(this->m_buttonSearchesBig);
        bigButtonLayout->addWidget(this->m_buttonNotifyBig);
        bigButtonLayout->addStretch(); // Push buttons to top
    }

    // Add small buttons to toolStripSmall placeholder
    QHBoxLayout* smallButtonLayout = qobject_cast<QHBoxLayout*>(this->ui->toolStripSmallPlaceholder->layout());
    if (smallButtonLayout)
    {
        smallButtonLayout->addStretch(); // Push buttons to right
        smallButtonLayout->addWidget(this->m_buttonInfraSmall);
        smallButtonLayout->addWidget(this->m_buttonObjectsSmall);
        smallButtonLayout->addWidget(this->m_buttonOrganizationSmall);
        smallButtonLayout->addWidget(this->m_buttonSearchesSmall);
        smallButtonLayout->addWidget(this->m_buttonNotifySmall);
    }

    // Populate dropdown menus
    this->populateOrganizationDropDown();
    this->populateSearchDropDown();

    // Connect big button signals
    connect(this->m_buttonInfraBig, &NavigationButtonBig::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });
    connect(this->m_buttonObjectsBig, &NavigationButtonBig::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });
    connect(this->m_buttonNotifyBig, &NotificationButtonBig::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });

    // Connect small button signals (for collapsed toolbar state)
    connect(this->m_buttonInfraSmall, &NavigationButtonSmall::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });
    connect(this->m_buttonObjectsSmall, &NavigationButtonSmall::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });
    connect(this->m_buttonNotifySmall, &NotificationButtonSmall::navigationViewChanged,
            this, [this]() { this->onNavigationButtonChecked(); });
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
    this->m_buttonOrganizationBig->setItemList(items);
    this->m_buttonOrganizationSmall->setItemList(items);
}

void NavigationPane::populateSearchDropDown()
{
    // TODO: Implement when Search infrastructure is ported
    // Matches C# NavigationPane.PopulateSearchDropDown
    // Should populate from Search.Searches array
}

void NavigationPane::onNavigationButtonChecked()
{
    QObject* senderObj = this->sender();

    // Determine which mode was selected
    NavigationMode newMode = this->m_currentMode;

    if (senderObj == this->m_buttonInfraBig || senderObj == this->m_buttonInfraSmall)
    {
        newMode = Infrastructure;
    } else if (senderObj == this->m_buttonObjectsBig || senderObj == this->m_buttonObjectsSmall)
    {
        newMode = Objects;
    } else if (senderObj == this->m_buttonNotifyBig || senderObj == this->m_buttonNotifySmall)
    {
        newMode = Notifications;
    }

    if (newMode != this->m_currentMode)
    {
        this->m_currentMode = newMode;
        this->onNavigationModeChanged();
        emit navigationModeChanged(this->m_currentMode);
    }
}

void NavigationPane::onOrganizationMenuItemTriggered()
{
    QAction* action = qobject_cast<QAction*>(this->sender());
    if (!action)
        return;

    NavigationMode mode = action->data().value<NavigationMode>();
    if (mode != this->m_currentMode)
    {
        this->m_currentMode = mode;

        // Check the organization button since we're in organization mode
        this->m_buttonOrganizationBig->setChecked(true);

        this->onNavigationModeChanged();
        emit navigationModeChanged(this->m_currentMode);
    }
} 

void NavigationPane::onSearchMenuItemTriggered()
{
    // TODO: Implement when Search infrastructure is ported
}

void NavigationPane::onNavigationModeChanged()
{
    // Matches C# NavigationPane.OnNavigationModeChanged
    if (this->m_currentMode == Notifications)
    {
        // Switch to notifications view
        this->ui->navigationViewPlaceholder->setVisible(false);
        this->ui->notificationsViewPlaceholder->setVisible(true);

        // Select last notifications mode
        NotificationsView* notifView = this->findChild<NotificationsView*>();
        if (notifView)
        {
            notifView->selectNotificationsSubMode(this->m_lastNotificationsMode);
        }
    } else
    {
        // Switch to navigation view
        this->ui->notificationsViewPlaceholder->setVisible(false);
        this->ui->navigationViewPlaceholder->setVisible(true);

        // Update tree view for new mode
        // Set the navigation mode so tree builder uses correct layout
        this->m_currentNavigationView->SetNavigationMode(this->m_currentMode);
        this->m_currentNavigationView->ResetSearchBox();
        this->m_currentNavigationView->FocusTreeView();
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

void NavigationPane::onNavigationViewDragDropCommand(const QString& commandKey)
{
    emit dragDropCommandActivated(commandKey);
}

void NavigationPane::onNavigationViewConnectToServerRequested()
{
    emit connectToServerRequested();
}

void NavigationPane::onNotificationsSubModeChanged(NotificationsSubMode subMode)
{
    this->m_lastNotificationsMode = subMode;
    emit notificationsSubModeChanged(subMode);
}

// Public methods
NavigationView* NavigationPane::GetNavigationView() const
{
    return this->m_currentNavigationView;
}

NotificationsView* NavigationPane::GetNotificationsView() const
{
    return this->m_currentNotificationsView;
} 

void NavigationPane::UpdateNotificationsButton(NotificationsSubMode mode, int entries)
{
    this->m_currentNotificationsView->updateEntries(mode, entries);
    int totalEntries = this->m_currentNotificationsView->getTotalEntries();
    this->m_buttonNotifyBig->setUnreadEntries(totalEntries);
    this->m_buttonNotifySmall->setUnreadEntries(totalEntries);
} 

void NavigationPane::SwitchToInfrastructureMode()
{
    if (!this->m_buttonInfraBig->isChecked())
    {
        this->m_buttonInfraBig->setChecked(true);
    }
}

void NavigationPane::SwitchToNotificationsView(NotificationsSubMode subMode)
{
    // Check the notification button if switching programmatically
    if (!this->m_buttonNotifyBig->isChecked())
    {
        this->m_buttonNotifyBig->setChecked(true);
    }

    this->m_currentNotificationsView->selectNotificationsSubMode(subMode);
}

void NavigationPane::FocusTreeView()
{
    this->m_currentNavigationView->FocusTreeView();
}

void NavigationPane::RequestRefreshTreeView()
{
    this->m_currentNavigationView->RequestRefreshTreeView();
}

void NavigationPane::UpdateSearch()
{
    // Matches C# NavigationPane.UpdateSearch (line 196)
    // Update NavigationView's CurrentSearch based on current mode
    // TODO: Once Search infrastructure is ported from C#, this will set
    // GetNavigationView->setCurrentSearch(getSearchForMode(m_currentMode))
    // For now, apply view filters and trigger a refresh
    SettingsManager& settings = SettingsManager::instance();
    NavigationView::ViewFilters filters;
    filters.showDefaultTemplates = settings.GetDefaultTemplatesVisible();
    filters.showUserTemplates = settings.GetUserTemplatesVisible();
    filters.showLocalStorage = settings.GetLocalSRsVisible();
    filters.showHiddenObjects = settings.GetShowHiddenObjects();
    this->m_currentNavigationView->SetViewFilters(filters);
}

void NavigationPane::SetInSearchMode(bool enabled)
{
    // Matches C# NavigationPane.InSearchMode property
    this->m_currentNavigationView->SetInSearchMode(enabled);
}

void NavigationPane::updateButtonLayout()
{
    // C# toolStripBig_LayoutCompleted shows each small button only when its
    // fixed-height large counterpart no longer fits in the vertical strip.
    const QList<NavigationButtonBig*> bigButtons = {
        this->m_buttonInfraBig, this->m_buttonObjectsBig, this->m_buttonOrganizationBig,
        this->m_buttonSearchesBig, this->m_buttonNotifyBig
    };
    const QList<NavigationButtonSmall*> smallButtons = {
        this->m_buttonInfraSmall, this->m_buttonObjectsSmall, this->m_buttonOrganizationSmall,
        this->m_buttonSearchesSmall, this->m_buttonNotifySmall
    };
    int remainingHeight = this->ui->panel2->height();
    for (int i = 0; i < bigButtons.size(); ++i)
    {
        const int buttonHeight = bigButtons.at(i)->height();
        const bool fits = remainingHeight >= buttonHeight;
        bigButtons.at(i)->setVisible(fits);
        smallButtons.at(i)->setVisible(!fits);
        if (fits)
            remainingHeight -= buttonHeight;
    }
}

void NavigationPane::restoreButtonPanelHeight()
{
    const int available = qMax(0, this->ui->splitContainer->height() - this->ui->splitContainer->handleWidth());
    const int buttonHeight = qMin(this->m_buttonPanelHeight, available);
    this->ui->splitContainer->setSizes(QList<int>{available - buttonHeight, buttonHeight});
    this->updateButtonLayout();
}

bool NavigationPane::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == this->ui->panel2 && event->type() == QEvent::Resize)
        this->updateButtonLayout();
    return QWidget::eventFilter(watched, event);
}

void NavigationPane::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Wait for the outer layout to resize the splitter, then preserve the
    // user's chosen button area rather than scaling it with the tree.
    QTimer::singleShot(0, this, &NavigationPane::restoreButtonPanelHeight);
}
