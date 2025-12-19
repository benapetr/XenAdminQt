/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
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

    QString helpID() const override { return "AlertSummaryDialog"; }
    bool filterIsOn() const override;

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
