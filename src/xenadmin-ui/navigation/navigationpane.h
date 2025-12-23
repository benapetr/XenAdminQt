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

#ifndef NAVIGATIONPANE_H
#define NAVIGATIONPANE_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NavigationPane;
}
QT_END_NAMESPACE

class NavigationView;
class NotificationsView;
class NavigationButtonBig;
class NavigationButtonSmall;
class NavigationDropDownButtonBig;
class NavigationDropDownButtonSmall;
class NotificationButtonBig;
class NotificationButtonSmall;
class XenLib;

/**
 * @brief Navigation pane widget matching C# NavigationPane control
 *
 * Provides the left-side navigation UI with:
 * - NavigationView (Infrastructure/Objects/Organization tree)
 * - NotificationsView (Alerts/Updates/Events)
 * - Navigation mode buttons (Big + Small toolstrips)
 *
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationPane.cs
 */
class NavigationPane : public QWidget
{
    Q_OBJECT

    public:
        /**
         * Navigation modes matching C# NavigationPane.NavigationMode enum
         * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationPane.cs line 43
         */
        enum NavigationMode
        {
            Infrastructure,
            Objects,
            Tags,
            Folders,
            CustomFields,
            vApps,
            SavedSearch,
            Notifications
        };
        Q_ENUM(NavigationMode)

        /**
         * Notifications sub-modes (matches C# NotificationsSubMode)
         * File: xenadmin/XenAdmin/Controls/MainWindowControls/NotificationsView.cs
         */
        enum NotificationsSubMode
        {
            Alerts,
            Updates,
            Events
        };
        Q_ENUM(NotificationsSubMode)

        explicit NavigationPane(QWidget* parent = nullptr);
        ~NavigationPane();

        // Accessors matching C# NavigationPane properties
        NavigationMode currentMode() const
        {
            return this->m_currentMode;
        }

        // Access to internal views (for MainWindow to wire up signals)
        NavigationView* navigationView() const;
        NotificationsView* notificationsView() const;

        // Public methods matching C# NavigationPane interface
        void updateNotificationsButton(NotificationsSubMode mode, int entries);
        void switchToInfrastructureMode();
        void switchToNotificationsView(NotificationsSubMode subMode);
        void focusTreeView();
        void requestRefreshTreeView();
        void updateSearch(); // Matches C# NavigationPane.UpdateSearch (line 196)

        // Search mode control (matches C# NavigationPane.InSearchMode property)
        void setInSearchMode(bool enabled);

        // XenLib access (pass through to NavigationView)
        void setXenLib(XenLib* xenLib);

    signals:
        /**
         * Emitted when navigation mode changes
         * Matches C# NavigationPane.NavigationModeChanged event
         */
        void navigationModeChanged(NavigationMode mode);

        /**
         * Emitted when notifications sub-mode changes
         * Matches C# NavigationPane.NotificationsSubModeChanged event
         */
        void notificationsSubModeChanged(NotificationsSubMode subMode);

        /**
         * Tree view events - forwarded from NavigationView
         * Match C# NavigationPane events
         */
        void treeViewSelectionChanged();
        void treeNodeBeforeSelected();
        void treeNodeClicked();
        void treeNodeRightClicked();
        void treeViewRefreshed();
        void treeViewRefreshSuspended();
        void treeViewRefreshResumed();

        /**
         * Drag/drop command activation
         * Matches C# NavigationPane.DragDropCommandActivated event
         */
        void dragDropCommandActivated(const QString& commandKey);

    protected:
        /**
         * Override to preserve Panel2 height during resize
         * Matches C# NavigationPane.OnResize
         * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationPane.cs line 115
         */
        void resizeEvent(QResizeEvent* event) override;

    private slots:
        void onNavigationButtonChecked();
        void onOrganizationMenuItemTriggered();
        void onSearchMenuItemTriggered();

        // Slots for forwarding NavigationView events
        void onNavigationViewSelectionChanged();
        void onNavigationViewBeforeSelected();
        void onNavigationViewClicked();
        void onNavigationViewRightClicked();
        void onNavigationViewRefreshed();
        void onNavigationViewRefreshSuspended();
        void onNavigationViewRefreshResumed();
        void onNavigationViewDragDropCommand(const QString& commandKey);

        // Slots for NotificationsView events
        void onNotificationsSubModeChanged(NotificationsSubMode subMode);

    private:
        void setupNavigationButtons();
        void addNavigationItemPair(QObject* bigButton, QObject* smallButton);
        void populateOrganizationDropDown();
        void populateSearchDropDown();
        void onNavigationModeChanged();

        Ui::NavigationPane* ui;

        NavigationMode m_currentMode;
        NotificationsSubMode m_lastNotificationsMode;

        // Navigation buttons (big variants in Panel2)
        NavigationButtonBig* m_buttonInfraBig;
        NavigationButtonBig* m_buttonObjectsBig;
        NavigationDropDownButtonBig* m_buttonOrganizationBig;
        NavigationDropDownButtonBig* m_buttonSearchesBig;
        NotificationButtonBig* m_buttonNotifyBig;

        // Navigation buttons (small variants - shown when pane shrinks)
        NavigationButtonSmall* m_buttonInfraSmall;
        NavigationButtonSmall* m_buttonObjectsSmall;
        NavigationDropDownButtonSmall* m_buttonOrganizationSmall;
        NavigationDropDownButtonSmall* m_buttonSearchesSmall;
        NotificationButtonSmall* m_buttonNotifySmall;
};

#endif // NAVIGATIONPANE_H
