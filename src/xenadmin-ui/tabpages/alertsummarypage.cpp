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

#include "alertsummarypage.h"
#include "ui_alertsummarypage.h"
#include "xenlib/alerts/alertmanager.h"
#include "xenlib/alerts/alert.h"
#include <QDebug>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QRadioButton>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QMessageBox>

AlertSummaryPage::AlertSummaryPage(QWidget* parent) : NotificationsBasePage(parent), ui(new Ui::AlertSummaryPage), m_dateFilterEnabled(false)
{
    this->ui->setupUi(this);
    this->ui->alertsTable->sortByColumn(4, Qt::DescendingOrder);
    this->ui->alertsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    
    // Connect table cell clicks for expand/collapse (C# Reference: GridViewAlerts_CellClick)
    connect(this->ui->alertsTable, &QTableWidget::cellClicked, this, &AlertSummaryPage::onExpanderClicked);
    
    // Connect to AlertManager signals
    connect(AlertManager::instance(), &AlertManager::collectionChanged, this, &AlertSummaryPage::buildAlertList);
    
    qDebug() << "AlertSummaryPage initialized with AlertManager integration";
}

AlertSummaryPage::~AlertSummaryPage()
{
    delete this->ui;
}

bool AlertSummaryPage::GetFilterIsOn() const
{
    // C# Reference: AlertSummaryPage.cs FilterAlert() line 283
    return !this->m_severityFilters.isEmpty() || 
           !this->m_serverFilters.isEmpty() || 
           this->m_dateFilterEnabled;
}

void AlertSummaryPage::refreshPage()
{
    this->buildAlertList();
}

void AlertSummaryPage::registerEventHandlers()
{
    // Event handlers already connected in constructor
}

void AlertSummaryPage::deregisterEventHandlers()
{
    // Disconnect handled automatically by Qt on destruction
}

void AlertSummaryPage::buildAlertList()
{
    if (!this->isVisible())
        return;
        
    this->ui->alertsTable->setUpdatesEnabled(false);
    this->ui->alertsTable->setRowCount(0);
    
    // Get alerts from AlertManager
    QList<Alert*> alerts = AlertManager::instance()->GetNonDismissingAlerts();
    
    // Apply filters (C# Reference: FilterAlert() line 283)
    QList<Alert*> filteredAlerts;
    for (Alert* alert : alerts)
    {
        if (!this->filterAlert(alert))
            filteredAlerts.append(alert);
    }
    
    for (Alert* alert : filteredAlerts)
    {
        int row = this->ui->alertsTable->rowCount();
        this->ui->alertsTable->insertRow(row);
        
        // Column 0: Expand/collapse icon (C# Reference: GridViewAlerts_CellClick)
        QTableWidgetItem* expandItem = new QTableWidgetItem();
        expandItem->setIcon(this->getExpanderIcon(alert->GetUUID()));
        expandItem->setData(Qt::UserRole, QVariant::fromValue(alert));
        this->ui->alertsTable->setItem(row, 0, expandItem);
        
        // Column 1: Severity (GetPriority)
        QString severity = Alert::PriorityToString(alert->GetPriority());
        QTableWidgetItem* severityItem = new QTableWidgetItem(severity);
        this->ui->alertsTable->setItem(row, 1, severityItem);
        
        // Column 2: Message (GetTitle + description if expanded)
        QString message = alert->GetTitle();
        bool isExpanded = this->m_expandedAlerts.contains(alert->GetUUID());
        if (isExpanded && !alert->GetDescription().isEmpty() && alert->GetDescription() != alert->GetTitle())
        {
            message += "\n" + alert->GetDescription();
        }
        QTableWidgetItem* messageItem = new QTableWidgetItem(message);
        this->ui->alertsTable->setItem(row, 2, messageItem);
        
        // Column 3: Location (AppliesTo)
        QString location = alert->AppliesTo();
        QTableWidgetItem* locationItem = new QTableWidgetItem(location);
        this->ui->alertsTable->setItem(row, 3, locationItem);
        
        // Column 4: Date
        QString dateStr = alert->GetTimestamp().toString("yyyy-MM-dd HH:mm:ss");
        QTableWidgetItem* dateItem = new QTableWidgetItem(dateStr);
        dateItem->setData(Qt::UserRole, alert->GetTimestamp());
        this->ui->alertsTable->setItem(row, 4, dateItem);
        
        // Column 5: Actions dropdown (C# Reference: GetAlertActionItems() line 200)
        QToolButton* actionBtn = new QToolButton();
        actionBtn->setText(tr("Actions"));
        actionBtn->setPopupMode(QToolButton::InstantPopup);
        
        QMenu* actionMenu = new QMenu(actionBtn);
        
        // Dismiss action (always available)
        QAction* dismissAction = actionMenu->addAction(tr("Dismiss"));
        connect(dismissAction, &QAction::triggered, this, [alert]()
        {
            alert->Dismiss();
            AlertManager::instance()->RemoveAlert(alert);
        });
        
        // TODO: Add Fix/Help/Web Console actions based on alert type
        // C# Reference: MessageAlert.cs FixLinkAction, HelpLinkAction, WebConsoleAction
        
        actionBtn->setMenu(actionMenu);
        this->ui->alertsTable->setCellWidget(row, 5, actionBtn);
    }
    
    this->ui->alertsTable->setUpdatesEnabled(true);
    
    qDebug() << "AlertSummaryPage: Displaying" << filteredAlerts.count() 
             << "of" << alerts.count() << "alerts";
}

bool AlertSummaryPage::filterAlert(Alert* alert) const
{
    // C# Reference: AlertSummaryPage.cs FilterAlert() line 283
    // Returns true if alert should be HIDDEN
    
    // Filter by severity
    if (!this->m_severityFilters.isEmpty())
    {
        if (!this->m_severityFilters.contains(alert->GetPriority()))
            return true;  // Hide this alert
    }
    
    // Filter by server (appliesTo location)
    if (!this->m_serverFilters.isEmpty())
    {
        QString location = alert->AppliesTo();
        bool found = false;
        for (const QString& filter : this->m_serverFilters)
        {
            if (location.contains(filter, Qt::CaseInsensitive))
            {
                found = true;
                break;
            }
        }
        if (!found)
            return true;  // Hide this alert
    }
    
    // Filter by date range
    if (this->m_dateFilterEnabled)
    {
        QDateTime timestamp = alert->GetTimestamp();
        if (timestamp < this->m_dateFilterFrom || timestamp > this->m_dateFilterTo)
            return true;  // Hide this alert
    }
    
    return false;  // Show this alert
}

QIcon AlertSummaryPage::getExpanderIcon(const QString& alertUuid) const
{
    // C# Reference: GridViewAlerts expandedState dictionary
    bool isExpanded = this->m_expandedAlerts.contains(alertUuid);
    QString iconPath = isExpanded ? 
        ":/images/expanded_triangle.png" : 
        ":/images/contracted_triangle.png";
    return QIcon(iconPath);
}

// Slots

void AlertSummaryPage::onExpanderClicked(int row, int column)
{
    // C# Reference: GridViewAlerts_CellClick() line 307
    if (column != 0)
        return;  // Only handle clicks on expand column
    
    QTableWidgetItem* item = this->ui->alertsTable->item(row, 0);
    if (!item)
        return;
    
    Alert* alert = item->data(Qt::UserRole).value<Alert*>();
    if (!alert)
        return;
    
    // Toggle expanded state
    QString uuid = alert->GetUUID();
    if (this->m_expandedAlerts.contains(uuid))
        this->m_expandedAlerts.remove(uuid);
    else
        this->m_expandedAlerts.insert(uuid);
    
    // Rebuild list to show/hide description
    this->buildAlertList();
}

void AlertSummaryPage::onFilterBySeverity()
{
    // C# Reference: toolStripDropDownSeveritiesFilter similar to EventsPage status filter
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Filter by Severity"));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Create checkboxes for each priority level
    QCheckBox* cbPriority1 = new QCheckBox(tr("Priority 1 (Critical)"), &dialog);
    QCheckBox* cbPriority2 = new QCheckBox(tr("Priority 2 (High)"), &dialog);
    QCheckBox* cbPriority3 = new QCheckBox(tr("Priority 3 (Medium)"), &dialog);
    QCheckBox* cbPriority4 = new QCheckBox(tr("Priority 4 (Low)"), &dialog);
    QCheckBox* cbPriority5 = new QCheckBox(tr("Priority 5 (Info)"), &dialog);
    
    // Set current filter state
    cbPriority1->setChecked(this->m_severityFilters.isEmpty() || 
                           this->m_severityFilters.contains(AlertPriority::Priority1));
    cbPriority2->setChecked(this->m_severityFilters.isEmpty() || 
                           this->m_severityFilters.contains(AlertPriority::Priority2));
    cbPriority3->setChecked(this->m_severityFilters.isEmpty() || 
                           this->m_severityFilters.contains(AlertPriority::Priority3));
    cbPriority4->setChecked(this->m_severityFilters.isEmpty() || 
                           this->m_severityFilters.contains(AlertPriority::Priority4));
    cbPriority5->setChecked(this->m_severityFilters.isEmpty() || 
                           this->m_severityFilters.contains(AlertPriority::Priority5));
    
    layout->addWidget(cbPriority1);
    layout->addWidget(cbPriority2);
    layout->addWidget(cbPriority3);
    layout->addWidget(cbPriority4);
    layout->addWidget(cbPriority5);
    
    // Add buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted)
    {
        // Update filter state
        this->m_severityFilters.clear();
        
        if (cbPriority1->isChecked())
            this->m_severityFilters.insert(AlertPriority::Priority1);
        if (cbPriority2->isChecked())
            this->m_severityFilters.insert(AlertPriority::Priority2);
        if (cbPriority3->isChecked())
            this->m_severityFilters.insert(AlertPriority::Priority3);
        if (cbPriority4->isChecked())
            this->m_severityFilters.insert(AlertPriority::Priority4);
        if (cbPriority5->isChecked())
            this->m_severityFilters.insert(AlertPriority::Priority5);
        
        // If all are checked, clear the filter (show all)
        if (cbPriority1->isChecked() && cbPriority2->isChecked() && 
            cbPriority3->isChecked() && cbPriority4->isChecked() && 
            cbPriority5->isChecked())
        {
            this->m_severityFilters.clear();
        }
        
        this->buildAlertList();
    }
}

void AlertSummaryPage::onFilterByServer()
{
    // C# Reference: toolStripDropDownButtonServerFilter similar to EventsPage location filter
    // Get list of all unique locations from alerts
    QSet<QString> allLocations;
    const QList<Alert*> alerts = AlertManager::instance()->GetNonDismissingAlerts();
    
    for (Alert* alert : alerts)
    {
        QString location = alert->AppliesTo();
        if (!location.isEmpty())
            allLocations.insert(location);
    }
    
    if (allLocations.isEmpty())
    {
        QMessageBox::information(this, tr("Filter by Server"),
                               tr("No servers available to filter."));
        return;
    }
    
    // Create dialog with checkboxes for each location
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Filter by Server"));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QMap<QString, QCheckBox*> locationCheckboxes;
    
    for (const QString& location : allLocations)
    {
        QCheckBox* cb = new QCheckBox(location, &dialog);
        cb->setChecked(this->m_serverFilters.isEmpty() || 
                      this->m_serverFilters.contains(location));
        layout->addWidget(cb);
        locationCheckboxes[location] = cb;
    }
    
    // Add buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted)
    {
        // Update filter state
        this->m_serverFilters.clear();
        
        bool allChecked = true;
        for (auto it = locationCheckboxes.begin(); it != locationCheckboxes.end(); ++it)
        {
            if (it.value()->isChecked())
                this->m_serverFilters.append(it.key());
            else
                allChecked = false;
        }
        
        // If all are checked, clear the filter (show all)
        if (allChecked)
            this->m_serverFilters.clear();
        
        this->buildAlertList();
    }
}

void AlertSummaryPage::onFilterByDate()
{
    // C# Reference: toolStripDropDownButtonDateFilter similar to EventsPage date filter
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Filter by Dates"));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Radio buttons for preset date ranges
    QRadioButton* rbShowAll = new QRadioButton(tr("Show All"), &dialog);
    QRadioButton* rbLast24Hours = new QRadioButton(tr("Last 24 Hours"), &dialog);
    QRadioButton* rbLast3Days = new QRadioButton(tr("Last 3 Days"), &dialog);
    QRadioButton* rbLast7Days = new QRadioButton(tr("Last 7 Days"), &dialog);
    QRadioButton* rbLast30Days = new QRadioButton(tr("Last 30 Days"), &dialog);
    QRadioButton* rbCustom = new QRadioButton(tr("Custom Range"), &dialog);
    
    // Date edit widgets for custom range
    QWidget* customWidget = new QWidget(&dialog);
    QHBoxLayout* customLayout = new QHBoxLayout(customWidget);
    QLabel* lblFrom = new QLabel(tr("From:"), customWidget);
    QDateEdit* dateFrom = new QDateEdit(customWidget);
    dateFrom->setCalendarPopup(true);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
    QLabel* lblTo = new QLabel(tr("To:"), customWidget);
    QDateEdit* dateTo = new QDateEdit(customWidget);
    dateTo->setCalendarPopup(true);
    dateTo->setDate(QDate::currentDate());
    customLayout->addWidget(lblFrom);
    customLayout->addWidget(dateFrom);
    customLayout->addWidget(lblTo);
    customLayout->addWidget(dateTo);
    customWidget->setEnabled(false);
    
    // Set current state
    rbShowAll->setChecked(!this->m_dateFilterEnabled);
    
    layout->addWidget(rbShowAll);
    layout->addWidget(rbLast24Hours);
    layout->addWidget(rbLast3Days);
    layout->addWidget(rbLast7Days);
    layout->addWidget(rbLast30Days);
    layout->addWidget(rbCustom);
    layout->addWidget(customWidget);
    
    // Enable custom date fields when custom is selected
    connect(rbCustom, &QRadioButton::toggled, customWidget, &QWidget::setEnabled);
    
    // Add buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted)
    {
        QDateTime now = QDateTime::currentDateTime();
        
        if (rbShowAll->isChecked())
        {
            this->m_dateFilterEnabled = false;
        }
        else if (rbLast24Hours->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-1);
            this->m_dateFilterTo = now;
        }
        else if (rbLast3Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-3);
            this->m_dateFilterTo = now;
        }
        else if (rbLast7Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-7);
            this->m_dateFilterTo = now;
        }
        else if (rbLast30Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-30);
            this->m_dateFilterTo = now;
        }
        else if (rbCustom->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = QDateTime(dateFrom->date(), QTime(0, 0, 0));
            this->m_dateFilterTo = QDateTime(dateTo->date(), QTime(23, 59, 59));
        }
        
        this->buildAlertList();
    }
}

void AlertSummaryPage::onDismissAll()
{
    // C# Reference: tsbtnDismissAll_Click
    const QList<Alert*> alerts = AlertManager::instance()->GetNonDismissingAlerts();
    
    if (alerts.isEmpty())
        return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Dismiss All Alerts"),
        tr("Are you sure you want to dismiss all %1 alerts?").arg(alerts.count()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes)
    {
        for (Alert* alert : alerts)
        {
            alert->Dismiss();
            AlertManager::instance()->RemoveAlert(alert);
        }
    }
}

void AlertSummaryPage::onDismissSelected()
{
    // C# Reference: tsbtnDismissAll similar pattern
    QList<QTableWidgetItem*> selectedItems = this->ui->alertsTable->selectedItems();
    if (selectedItems.isEmpty())
        return;
    
    // Get unique rows
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems)
    {
        selectedRows.insert(item->row());
    }
    
    if (selectedRows.isEmpty())
        return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Dismiss Selected Alerts"),
        tr("Are you sure you want to dismiss %1 selected alert(s)?").arg(selectedRows.count()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes)
    {
        QList<Alert*> alertsToDismiss;
        for (int row : selectedRows)
        {
            QTableWidgetItem* item = this->ui->alertsTable->item(row, 0);
            if (item)
            {
                Alert* alert = item->data(Qt::UserRole).value<Alert*>();
                if (alert)
                    alertsToDismiss.append(alert);
            }
        }
        
        for (Alert* alert : alertsToDismiss)
        {
            alert->Dismiss();
            AlertManager::instance()->RemoveAlert(alert);
        }
    }
}
