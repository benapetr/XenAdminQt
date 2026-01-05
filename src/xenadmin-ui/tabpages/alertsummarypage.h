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

#ifndef ALERTSUMMARYPAGE_H
#define ALERTSUMMARYPAGE_H

#include "notificationsbasepage.h"
#include "../alerts/alert.h"
#include <QDateTime>
#include <QSet>
#include <QStringList>

namespace Ui {
class AlertSummaryPage;
}

class AlertSummaryPage : public NotificationsBasePage
{
    Q_OBJECT

    public:
        explicit AlertSummaryPage(QWidget* parent = nullptr);
        ~AlertSummaryPage();

        NavigationPane::NotificationsSubMode notificationsSubMode() const override
        {
            return NavigationPane::Alerts;
        }

        QString GetHelpID() const override { return "AlertSummaryDialog"; }
        bool GetFilterIsOn() const override;

    protected:
        void refreshPage() override;
        void registerEventHandlers() override;
        void deregisterEventHandlers() override;

    private slots:
        void onExpanderClicked(int row, int column);
        void onFilterBySeverity();
        void onFilterByServer();
        void onFilterByDate();
        void onDismissAll();
        void onDismissSelected();

    private:
        Ui::AlertSummaryPage* ui;

        // Filter state (C# Reference: AlertSummaryPage.cs FilterAlert() line 283)
        QSet<AlertPriority> m_severityFilters;  // Empty = show all
        QStringList m_serverFilters;            // Empty = show all
        bool m_dateFilterEnabled;
        QDateTime m_dateFilterFrom;
        QDateTime m_dateFilterTo;

        // Expanded state tracking (C# Reference: AlertSummaryPage.cs expandedState)
        QSet<QString> m_expandedAlerts;  // Set of alert UUIDs that are expanded

        void buildAlertList();
        bool filterAlert(Alert* alert) const;
        QIcon getExpanderIcon(const QString& alertUuid) const;
};

#endif // ALERTSUMMARYPAGE_H
