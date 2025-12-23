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

#ifndef NOTIFICATIONSVIEW_H
#define NOTIFICATIONSVIEW_H

#include <QWidget>
#include <QListWidget>
#include "../navigation/navigationpane.h"
#include "notificationssubmodeitem.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NotificationsView;
}
QT_END_NAMESPACE

/**
 * @brief Notifications view widget for Alerts/Events
 *
 * Matches C# NotificationsView control (FlickerFreeListBox with custom-drawn items)
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NotificationsView.cs
 */
class NotificationsView : public QWidget
{
    Q_OBJECT

    public:
        explicit NotificationsView(QWidget* parent = nullptr);
        ~NotificationsView();

        // Public methods matching C# NotificationsView interface
        void selectNotificationsSubMode(NavigationPane::NotificationsSubMode subMode);
        void updateEntries(NavigationPane::NotificationsSubMode mode, int entries);
        int getTotalEntries() const;

    signals:
        void notificationsSubModeChanged(NavigationPane::NotificationsSubMode subMode);

    private slots:
        void onItemClicked(QListWidgetItem* item);

    private:
        void initializeItems();
        QListWidgetItem* getItemForSubMode(NavigationPane::NotificationsSubMode subMode) const;

        Ui::NotificationsView* ui;
        NotificationsSubModeItemDelegate* m_delegate;

        // Entry counts for each sub-mode
        int m_alertsCount;
        int m_eventsCount;

        // List items
        QListWidgetItem* m_alertsItem;
        QListWidgetItem* m_eventsItem;
};

#endif // NOTIFICATIONSVIEW_H
