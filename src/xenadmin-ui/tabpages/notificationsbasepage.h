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

#ifndef NOTIFICATIONSBASEPAGE_H
#define NOTIFICATIONSBASEPAGE_H

#include <QWidget>
#include "navigation/navigationpane.h"

class XenLib;

/**
 * Base class for notification pages (Alerts, Events/History).
 * 
 * C# Equivalent: xenadmin/XenAdmin/TabPages/NotificationsBasePage.cs
 * 
 * This base class provides common infrastructure for notification pages
 * that appear when the user switches to Notifications mode in the navigation pane.
 * Unlike BaseTabPage which shows tabs for selected objects, NotificationsBasePage
 * shows full-page views (Alerts, Events) that replace the tab control.
 */
class NotificationsBasePage : public QWidget
{
    Q_OBJECT

    public:
        explicit NotificationsBasePage(QWidget* parent = nullptr);
        virtual ~NotificationsBasePage();

        /**
         * Show this notification page and refresh its content.
         * C# Reference: NotificationsBasePage.ShowPage() line 48
         */
        void showPage();

        /**
         * Hide this notification page and deregister event handlers.
         * C# Reference: NotificationsBasePage.HidePage() line 56
         */
        void hidePage();

        /**
         * Get the notifications sub-mode this page represents.
         * C# Reference: NotificationsBasePage.NotificationsSubMode line 63
         */
        virtual NavigationPane::NotificationsSubMode notificationsSubMode() const = 0;

        /**
         * Get the help ID for context-sensitive help.
         * C# Reference: NotificationsBasePage.HelpID line 65
         */
        virtual QString helpID() const
        {
            return "";
        }

        /**
         * Check if any filters are currently active on this page.
         * C# Reference: NotificationsBasePage.FilterIsOn line 67
         */
        virtual bool filterIsOn() const
        {
            return false;
        }

        /**
         * Set the XenLib instance for accessing XenAPI.
         */
        virtual void setXenLib(XenLib* xenLib);

    signals:
        /**
         * Emitted when filters change on this page.
         * C# Reference: NotificationsBasePage.FiltersChanged event line 39
         */
        void filtersChanged();

    protected:
        /**
         * Refresh the page content (rebuild lists, update display).
         * Override in derived classes to implement page-specific refresh logic.
         * C# Reference: NotificationsBasePage.RefreshPage() line 44
         */
        virtual void refreshPage();

        /**
         * Register event handlers when the page becomes visible.
         * Override in derived classes to subscribe to events.
         * C# Reference: NotificationsBasePage.RegisterEventHandlers() line 49
         */
        virtual void registerEventHandlers();

        /**
         * Deregister event handlers when the page is hidden.
         * Override in derived classes to unsubscribe from events.
         * C# Reference: NotificationsBasePage.DeregisterEventHandlers() line 52
         */
        virtual void deregisterEventHandlers();

        /**
         * Called by derived classes when filters change.
         * C# Reference: NotificationsBasePage.OnFiltersChanged() line 41
         */
        void onFiltersChanged();

        XenLib* m_xenLib;
};

#endif // NOTIFICATIONSBASEPAGE_H
