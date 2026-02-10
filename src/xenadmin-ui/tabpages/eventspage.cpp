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

#include "eventspage.h"
#include "ui_eventspage.h"
#include "../iconmanager.h"
#include "../settingsmanager.h"
#include "operations/operationmanager.h"
#include "xen/network/connection.h"
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QDateEdit>
#include <QLabel>
#include <QTime>
#include <QStringList>
#include <QToolButton>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QItemSelectionModel>
#include <algorithm>

// Register OperationRecord pointer as a metatype so it can be used in QVariant
Q_DECLARE_METATYPE(OperationManager::OperationRecord*)

EventsPage::EventsPage(QWidget* parent) : NotificationsBasePage(parent), ui(new Ui::EventsPage)
{
    // C# Reference: xenadmin/XenAdmin/TabPages/HistoryPage.cs line 56
    this->ui->setupUi(this);

    // Set initial sort (by date descending)
    // C# Reference: HistoryPage constructor line 61
    this->ui->eventsTable->sortByColumn(4, Qt::DescendingOrder); // columnDate

    // Setup table appearance
    this->ui->eventsTable->horizontalHeader()->setStretchLastSection(false);
    this->ui->eventsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // Message column
    this->ui->eventsTable->setWordWrap(true);
    this->ui->eventsTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->ui->eventsTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect toolbar actions
    connect(this->ui->actionFilterStatus, &QAction::triggered, this, &EventsPage::onFilterStatusChanged);
    connect(this->ui->actionFilterLocation, &QAction::triggered, this, &EventsPage::onFilterLocationChanged);
    connect(this->ui->actionFilterDates, &QAction::triggered, this, &EventsPage::onFilterDatesChanged);
    connect(this->ui->actionDismiss, &QAction::triggered, this, &EventsPage::onDismissAll);

    // Connect Dismiss Selected action
    connect(this->ui->actionDismissSelected, &QAction::triggered, this, &EventsPage::onDismissSelected);

    // Connect table signals
    connect(this->ui->eventsTable, &QTableWidget::cellClicked, this, &EventsPage::onEventsTableCellClicked);
    connect(this->ui->eventsTable, &QTableWidget::cellDoubleClicked, this, &EventsPage::onEventsTableCellDoubleClicked);
    connect(this->ui->eventsTable, &QTableWidget::itemSelectionChanged, this, &EventsPage::onEventsTableSelectionChanged);
    connect(this->ui->eventsTable->horizontalHeader(), &QHeaderView::sectionClicked, this, &EventsPage::onEventsTableHeaderClicked);
    connect(this->ui->eventsTable, &QWidget::customContextMenuRequested, this, &EventsPage::onEventsContextMenuRequested);

    this->updateButtons();

    // Connect to OperationManager signals
    // C# Reference: HistoryPage constructor line 65 - ActionBase.NewAction += Action_NewAction
    connect(OperationManager::instance(), &OperationManager::newOperation, this, &EventsPage::onNewOperation);
}

EventsPage::~EventsPage()
{
    // C# Reference: HistoryPage.Dispose() line 17
    this->deregisterEventHandlers();
    delete this->ui;
}

bool EventsPage::GetFilterIsOn() const
{
    // C# Reference: HistoryPage.FilterIsOn line 175
    return !this->m_statusFilters.isEmpty() || !this->m_locationFilters.isEmpty() || this->m_dateFilterEnabled;
}

void EventsPage::refreshPage()
{
    // C# Reference: HistoryPage.RefreshPage() line 68
    // Note: C# version initializes host filter list here
    // We could populate m_locationFilters here if needed
    this->buildRowList();
}

void EventsPage::registerEventHandlers()
{
    // C# Reference: HistoryPage.RegisterEventHandlers() line 74
    // Connect to OperationManager signals for tracking operation changes
    // C# equivalent: ConnectionsManager.History.CollectionChanged += History_CollectionChanged
    connect(OperationManager::instance(), &OperationManager::recordAdded, this, &EventsPage::onOperationAdded);
    connect(OperationManager::instance(), &OperationManager::recordUpdated, this, &EventsPage::onOperationUpdated);
    connect(OperationManager::instance(), &OperationManager::recordRemoved, this, &EventsPage::onOperationRemoved);
}

void EventsPage::deregisterEventHandlers()
{
    // C# Reference: HistoryPage.DeregisterEventHandlers() line 79
    // Disconnect from OperationManager signals
    // C# equivalent: ConnectionsManager.History.CollectionChanged -= History_CollectionChanged
    disconnect(OperationManager::instance(), &OperationManager::recordAdded, this, &EventsPage::onOperationAdded);
    disconnect(OperationManager::instance(), &OperationManager::recordUpdated, this, &EventsPage::onOperationUpdated);
    disconnect(OperationManager::instance(), &OperationManager::recordRemoved, this, &EventsPage::onOperationRemoved);
}

void EventsPage::buildRowList()
{
    // C# Reference: HistoryPage.BuildRowList() line 178
    if (!this->isVisible())
        return;

    const bool sortingEnabled = this->ui->eventsTable->isSortingEnabled();
    const int sortColumn = this->ui->eventsTable->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = this->ui->eventsTable->horizontalHeader()->sortIndicatorOrder();
    if (sortingEnabled)
        this->ui->eventsTable->setSortingEnabled(false);

    this->ui->eventsTable->setRowCount(0);

    // Get all operations from OperationManager
    // C# equivalent: var actions = ConnectionsManager.History.Where(a => !FilterAction(a)).ToList();
    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->GetRecords();
    
    QList<OperationManager::OperationRecord*> filteredRecords;
    for (auto* record : records)
    {
        if (!this->filterRecord(record))
            filteredRecords.append(record);
    }

    // Sort records (by date descending by default)
    this->sortRecords(filteredRecords);

    // Create rows for each record
    for (auto* record : filteredRecords)
    {
        this->createRecordRow(record);
    }

    if (sortingEnabled)
    {
        this->ui->eventsTable->setSortingEnabled(true);
        this->ui->eventsTable->sortItems(sortColumn, sortOrder);
    }

    this->updateButtons();
}

void EventsPage::sortRecords(QList<OperationManager::OperationRecord*>& records)
{
    // C# Reference: HistoryPage.SortActions() line 203
    // Sort by date descending (most recent first)
    std::sort(records.begin(), records.end(), 
              [](OperationManager::OperationRecord* a, OperationManager::OperationRecord* b) {
                  return a->started > b->started;
              });
}

bool EventsPage::filterRecord(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.FilterAction() line 224
    // Returns true if record should be filtered OUT (hidden)
    if (!record)
        return true;

    // Filter by status
    if (!this->m_statusFilters.isEmpty())
    {
        if (!this->m_statusFilters.contains(record->state))
            return true;  // Hide this record
    }

    // Filter by location
    if (!this->m_locationFilters.isEmpty() && record->operation && record->operation->GetConnection())
    {
        QString location = record->operation->GetConnection()->GetHostname();
        if (!this->m_locationFilters.contains(location))
            return true;  // Hide this record
    }

    // Filter by date range
    if (this->m_dateFilterEnabled)
    {
        if (this->m_dateFilterFrom.isValid() && record->started < this->m_dateFilterFrom)
            return true;  // Too old
        if (this->m_dateFilterTo.isValid() && record->started > this->m_dateFilterTo)
            return true;  // Too new
    }

    return false;  // Don't filter out (show this record)
}

void EventsPage::createRecordRow(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.CreateActionRow() line 250
    if (!record)
        return;

    int row = this->ui->eventsTable->rowCount();
    this->ui->eventsTable->insertRow(row);

    // Column 0: Expander (for showing details)
    // C# Reference: HistoryPage line 569 - expanderCell.Value = Images.StaticImages.contracted_triangle
    QTableWidgetItem* expanderItem = new QTableWidgetItem();
    expanderItem->setData(Qt::UserRole, QVariant::fromValue(record));
    QIcon contractedIcon(":/icons/contracted_triangle.png");
    expanderItem->setIcon(contractedIcon);
    this->ui->eventsTable->setItem(row, 0, expanderItem);

    // Column 1: Status icon/text
    QTableWidgetItem* statusItem = new QTableWidgetItem(this->getStatusText(record->state));
    statusItem->setData(Qt::UserRole, QVariant::fromValue(record));
    statusItem->setIcon(this->getStatusIcon(record->state));
    this->ui->eventsTable->setItem(row, 1, statusItem);

    // Column 2: Message/Title
    QString title = this->buildRecordTitle(record);
    QTableWidgetItem* messageItem = new QTableWidgetItem(title);
    messageItem->setData(Qt::UserRole, QVariant::fromValue(record));
    messageItem->setToolTip(this->buildRecordDetails(record));
    this->ui->eventsTable->setItem(row, 2, messageItem);

    // Column 3: Location (connection name)
    QString location = "";
    if (record->operation && record->operation->GetConnection())
        location = record->operation->GetConnection()->GetHostname();
    QTableWidgetItem* locationItem = new QTableWidgetItem(location);
    locationItem->setData(Qt::UserRole, QVariant::fromValue(record));
    this->ui->eventsTable->setItem(row, 3, locationItem);

    // Column 4: Date
    QTableWidgetItem* dateItem = new QTableWidgetItem(
        record->started.toString("yyyy-MM-dd HH:mm:ss"));
    dateItem->setData(Qt::UserRole, QVariant::fromValue(record));
    this->ui->eventsTable->setItem(row, 4, dateItem);

    // Column 5: Actions
    QTableWidgetItem* actionsItem = new QTableWidgetItem("");
    actionsItem->setData(Qt::UserRole, QVariant::fromValue(record));
    this->ui->eventsTable->setItem(row, 5, actionsItem);

    QToolButton* actionBtn = new QToolButton(this->ui->eventsTable);
    actionBtn->setText(tr("Actions"));
    actionBtn->setPopupMode(QToolButton::InstantPopup);

    QMenu* actionMenu = new QMenu(actionBtn);
    QAction* copyAction = actionMenu->addAction(tr("Copy"));
    QAction* dismissAction = actionMenu->addAction(tr("Dismiss"));
    dismissAction->setEnabled(record->state == AsyncOperation::Completed);

    connect(copyAction, &QAction::triggered, this, [this, record]()
    {
        if (!record)
            return;
        const int currentRow = this->findRowFromRecord(record);
        if (currentRow >= 0)
            this->copyRowsToClipboard(QList<int>() << currentRow);
    });
    connect(dismissAction, &QAction::triggered, this, [this, record]()
    {
        if (!record || record->state != AsyncOperation::Completed)
            return;
        this->dismissRecords(QList<OperationManager::OperationRecord*>() << record,
                             true,
                             tr("Dismiss Event"),
                             tr("Are you sure you want to dismiss this completed event?"));
    });

    actionBtn->setMenu(actionMenu);
    this->ui->eventsTable->setCellWidget(row, 5, actionBtn);
}

QString EventsPage::getStatusText(AsyncOperation::OperationState state) const
{
    // C# Reference: ActionBase.Status property
    switch (state)
    {
        case AsyncOperation::NotStarted:
            return tr("Not Started");
        case AsyncOperation::Running:
            return tr("In Progress");
        case AsyncOperation::Completed:
            return tr("Completed");
        case AsyncOperation::Failed:
            return tr("Failed");
        case AsyncOperation::Cancelled:
            return tr("Cancelled");
        default:
            return tr("Unknown");
    }
}

QIcon EventsPage::getStatusIcon(AsyncOperation::OperationState state) const
{
    // Mirror C# HistoryPage visual cues using XenAdmin status icons.
    switch (state)
    {
        case AsyncOperation::Completed:
            return IconManager::instance().GetSuccessIcon();
        case AsyncOperation::Failed:
            return IconManager::instance().GetErrorIcon();
        case AsyncOperation::Cancelled:
            return IconManager::instance().GetCancelledIcon();
        case AsyncOperation::Running:
            return IconManager::instance().GetInProgressIcon();
        case AsyncOperation::NotStarted:
            return IconManager::instance().GetNotStartedIcon();
        default:
            return IconManager::instance().GetNotStartedIcon();
    }
}

int EventsPage::findRowFromRecord(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.FindRowFromAction() line 281
    for (int row = 0; row < this->ui->eventsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui->eventsTable->item(row, 0);
        if (item && item->data(Qt::UserRole).value<OperationManager::OperationRecord*>() == record)
            return row;
    }
    return -1;
}

void EventsPage::removeRecordRow(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.RemoveActionRow() line 268
    int row = this->findRowFromRecord(record);
    if (row >= 0)
        this->ui->eventsTable->removeRow(row);
}

void EventsPage::updateRecordRow(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.action_Changed() line 122
    int row = this->findRowFromRecord(record);
    if (row < 0)
        return;

    // Update status
    QTableWidgetItem* statusItem = this->ui->eventsTable->item(row, 1);
    if (statusItem)
    {
        statusItem->setText(this->getStatusText(record->state));
        statusItem->setIcon(this->getStatusIcon(record->state));
    }

    // Update message if it changed
    QTableWidgetItem* messageItem = this->ui->eventsTable->item(row, 2);
    if (messageItem)
    {
        bool isExpanded = this->m_expandedRows.contains(row);
        if (isExpanded)
            messageItem->setText(this->buildRecordDetails(record));
        else
            messageItem->setText(this->buildRecordTitle(record));
        messageItem->setToolTip(this->buildRecordDetails(record));
    }
}

void EventsPage::updateButtons()
{
    // C# Reference: HistoryPage.UpdateButtons() line 292
    bool hasSelection = this->ui->eventsTable->selectionModel()->hasSelection();
    
    // Enable dismiss all if there are any completed records
    bool hasCompletedRecords = false;
    bool hasSelectedCompletedRecords = false;
    
    for (int row = 0; row < this->ui->eventsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui->eventsTable->item(row, 0);
        if (item)
        {
            auto* record = item->data(Qt::UserRole).value<OperationManager::OperationRecord*>();
            if (record && record->state == AsyncOperation::Completed)
            {
                hasCompletedRecords = true;
                
                // Check if this row is selected
                if (this->ui->eventsTable->item(row, 0)->isSelected())
                {
                    hasSelectedCompletedRecords = true;
                }
            }
        }
    }
    
    this->ui->actionDismiss->setEnabled(hasCompletedRecords);
    this->ui->actionDismissSelected->setEnabled(hasSelection && hasSelectedCompletedRecords);
}

void EventsPage::toggleExpandedState(int row)
{
    // C# Reference: HistoryPage.ToggleExpandedState() line 303
    if (row < 0 || row >= this->ui->eventsTable->rowCount())
        return;

    QTableWidgetItem* expanderItem = this->ui->eventsTable->item(row, 0);
    QTableWidgetItem* messageItem = this->ui->eventsTable->item(row, 2);
    if (!expanderItem || !messageItem)
        return;

    auto* record = messageItem->data(Qt::UserRole).value<OperationManager::OperationRecord*>();
    if (!record)
        return;

    bool isExpanded = this->m_expandedRows.contains(row);
    
    if (isExpanded)
    {
        // Collapse: show title only
        // C# Reference: HistoryPage line 312 - expanderCell.Value = Images.StaticImages.contracted_triangle
        this->m_expandedRows.remove(row);
        messageItem->setText(record->title);
        QIcon contractedIcon(":/icons/contracted_triangle.png");
        expanderItem->setIcon(contractedIcon);
    } else
    {
        // Expand: show full details
        // C# Reference: HistoryPage line 317 - expanderCell.Value = Images.StaticImages.expanded_triangle
        this->m_expandedRows.insert(row);
        messageItem->setText(this->buildRecordDetails(record));
        QIcon expandedIcon(":/icons/expanded_triangle.png");
        expanderItem->setIcon(expandedIcon);
    }
}

QString EventsPage::buildRecordTitle(OperationManager::OperationRecord* record) const
{
    if (!record)
        return QString();

    if (!record->title.isEmpty())
        return record->title;

    if (!record->description.isEmpty())
        return record->description;

    // Fall back to connection hostname if available
    if (record->operation && record->operation->GetConnection())
        return record->operation->GetConnection()->GetHostname();

    return tr("Operation");
}

QString EventsPage::buildRecordDescription(OperationManager::OperationRecord* record) const
{
    if (!record)
        return QString();

    if (!record->errorMessage.isEmpty())
        return record->errorMessage;

    return record->description;
}

QString EventsPage::formatElapsedTime(OperationManager::OperationRecord* record) const
{
    if (!record || !record->started.isValid())
        return QString();

    QDateTime end = record->finished.isValid() ? record->finished : QDateTime::currentDateTime();
    if (end < record->started)
        end = record->started;

    qint64 seconds = record->started.secsTo(end);
    if (seconds <= 0)
        return QString();

    qint64 days = seconds / 86400;
    seconds %= 86400;
    qint64 hours = seconds / 3600;
    seconds %= 3600;
    qint64 minutes = seconds / 60;
    seconds %= 60;

    QStringList parts;
    if (days > 0)
        parts << tr("%1d").arg(days);
    if (hours > 0)
        parts << tr("%1h").arg(hours);
    if (minutes > 0)
        parts << tr("%1m").arg(minutes);
    if (seconds > 0 || parts.isEmpty())
        parts << tr("%1s").arg(seconds);

    return tr("Time: %1").arg(parts.join(" "));
}

QString EventsPage::buildRecordDetails(OperationManager::OperationRecord* record) const
{
    if (!record)
        return QString();

    QStringList lines;
    QString title = this->buildRecordTitle(record);
    if (!title.isEmpty())
        lines << title;

    QString description = this->buildRecordDescription(record);
    if (!description.isEmpty())
        lines << description;

    QString timeInfo = this->formatElapsedTime(record);
    if (!timeInfo.isEmpty())
        lines << timeInfo;

    return lines.join("\n");
}

// Slots

void EventsPage::onFilterStatusChanged()
{
    // C# Reference: HistoryPage.toolStripDdbFilterStatus_FilterChanged() line 493
    // Create a dialog with checkboxes for each status
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Filter by Status"));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Create checkboxes for each status type
    QCheckBox* cbCompleted = new QCheckBox(tr("Completed"), &dialog);
    QCheckBox* cbFailed = new QCheckBox(tr("Failed"), &dialog);
    QCheckBox* cbRunning = new QCheckBox(tr("In Progress"), &dialog);
    QCheckBox* cbCancelled = new QCheckBox(tr("Cancelled"), &dialog);
    QCheckBox* cbNotStarted = new QCheckBox(tr("Not Started"), &dialog);
    
    // Set current filter state
    cbCompleted->setChecked(this->m_statusFilters.isEmpty() || this->m_statusFilters.contains(AsyncOperation::Completed));
    cbFailed->setChecked(this->m_statusFilters.isEmpty() || this->m_statusFilters.contains(AsyncOperation::Failed));
    cbRunning->setChecked(this->m_statusFilters.isEmpty() || this->m_statusFilters.contains(AsyncOperation::Running));
    cbCancelled->setChecked(this->m_statusFilters.isEmpty() || this->m_statusFilters.contains(AsyncOperation::Cancelled));
    cbNotStarted->setChecked(this->m_statusFilters.isEmpty() || this->m_statusFilters.contains(AsyncOperation::NotStarted));
    
    layout->addWidget(cbCompleted);
    layout->addWidget(cbFailed);
    layout->addWidget(cbRunning);
    layout->addWidget(cbCancelled);
    layout->addWidget(cbNotStarted);
    
    // Add buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted)
    {
        // Update filter state
        this->m_statusFilters.clear();
        
        // Only add checked statuses to filter (unchecked ones will be filtered out)
        if (cbCompleted->isChecked())
            this->m_statusFilters.insert(AsyncOperation::Completed);
        if (cbFailed->isChecked())
            this->m_statusFilters.insert(AsyncOperation::Failed);
        if (cbRunning->isChecked())
            this->m_statusFilters.insert(AsyncOperation::Running);
        if (cbCancelled->isChecked())
            this->m_statusFilters.insert(AsyncOperation::Cancelled);
        if (cbNotStarted->isChecked())
            this->m_statusFilters.insert(AsyncOperation::NotStarted);
        
        // If all are checked, clear the filter (show all)
        if (cbCompleted->isChecked() && cbFailed->isChecked() && 
            cbRunning->isChecked() && cbCancelled->isChecked() && 
            cbNotStarted->isChecked())
        {
            this->m_statusFilters.clear();
        }
        
        this->buildRowList();
    }
}

void EventsPage::onFilterLocationChanged()
{
    // C# Reference: HistoryPage.toolStripDdbFilterLocation_FilterChanged() line 498
    // Get list of all unique locations (hostnames) from operations
    QSet<QString> allLocations;
    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->GetRecords();
    
    for (auto* record : records)
    {
        if (record && record->operation && record->operation->GetConnection())
        {
            QString hostname = record->operation->GetConnection()->GetHostname();
            if (!hostname.isEmpty())
                allLocations.insert(hostname);
        }
    }
    
    if (allLocations.isEmpty())
    {
        QMessageBox::information(this, tr("Filter by Location"),
                               tr("No locations available to filter."));
        return;
    }
    
    // Create dialog with checkboxes for each location
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Filter by Location"));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QMap<QString, QCheckBox*> locationCheckboxes;
    
    for (const QString& location : allLocations)
    {
        QCheckBox* cb = new QCheckBox(location, &dialog);
        cb->setChecked(this->m_locationFilters.isEmpty() || 
                      this->m_locationFilters.contains(location));
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
        this->m_locationFilters.clear();
        
        bool allChecked = true;
        for (auto it = locationCheckboxes.begin(); it != locationCheckboxes.end(); ++it)
        {
            if (it.value()->isChecked())
                this->m_locationFilters.append(it.key());
            else
                allChecked = false;
        }
        
        // If all are checked, clear the filter (show all)
        if (allChecked)
            this->m_locationFilters.clear();
        
        this->buildRowList();
    }
}

void EventsPage::onFilterDatesChanged()
{
    // C# Reference: HistoryPage.toolStripDdbFilterDates_FilterChanged() line 503
    // Create dialog with preset date ranges
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
        } else if (rbLast24Hours->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-1);
            this->m_dateFilterTo = now;
        } else if (rbLast3Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-3);
            this->m_dateFilterTo = now;
        } else if (rbLast7Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-7);
            this->m_dateFilterTo = now;
        } else if (rbLast30Days->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = now.addDays(-30);
            this->m_dateFilterTo = now;
        } else if (rbCustom->isChecked())
        {
            this->m_dateFilterEnabled = true;
            this->m_dateFilterFrom = QDateTime(dateFrom->date(), QTime(0, 0, 0));
            this->m_dateFilterTo = QDateTime(dateTo->date(), QTime(23, 59, 59));
        }
        
        this->buildRowList();
    }
}

void EventsPage::onDismissAll()
{
    // C# Reference: HistoryPage.tsmiDismissAll_Click() line 418
    const QList<OperationManager::OperationRecord*>& allRecords = OperationManager::instance()->GetRecords();
    
    if (allRecords.isEmpty())
        return;

    // Determine which records to dismiss
    QList<OperationManager::OperationRecord*> recordsToDismiss;
    
    if (this->GetFilterIsOn())
    {
        // If filter is active, ask user if they want to dismiss all or just filtered
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Dismiss Events"));
        msgBox.setText(tr("Do you want to dismiss all completed events, or only the filtered visible events?"));
        msgBox.setIcon(QMessageBox::Question);
        
        QPushButton* dismissAllBtn = msgBox.addButton(tr("Dismiss All"), QMessageBox::YesRole);
        QPushButton* dismissFilteredBtn = msgBox.addButton(tr("Dismiss Filtered"), QMessageBox::NoRole);
        msgBox.addButton(QMessageBox::Cancel);
        
        msgBox.exec();
        
        QAbstractButton* clicked = msgBox.clickedButton();
        if (clicked == dismissAllBtn)
        {
            // Dismiss all completed operations
            for (auto* record : allRecords)
            {
                if (record && record->state == AsyncOperation::Completed)
                    recordsToDismiss.append(record);
            }
        } else if (clicked == dismissFilteredBtn)
        {
            // Dismiss only visible filtered completed operations
            for (int row = 0; row < this->ui->eventsTable->rowCount(); ++row)
            {
                QTableWidgetItem* item = this->ui->eventsTable->item(row, 0);
                if (item)
                {
                    auto* record = item->data(Qt::UserRole).value<OperationManager::OperationRecord*>();
                    if (record && record->state == AsyncOperation::Completed)
                        recordsToDismiss.append(record);
                }
            }
        } else
        {
            return;  // User cancelled
        }
    } else
    {
        if (!SettingsManager::instance().GetDoNotConfirmDismissEvents())
        {
            // No filter active - confirm dismissing all completed events
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Dismiss All Events"));
            msgBox.setText(tr("Are you sure you want to dismiss all completed events?"));
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);

            if (msgBox.exec() != QMessageBox::Yes)
                return;
        }
        
        // Dismiss all completed operations
        for (auto* record : allRecords)
        {
            if (record && record->state == AsyncOperation::Completed)
                recordsToDismiss.append(record);
        }
    }
    
    // Remove the records
    if (!recordsToDismiss.isEmpty())
    {
        OperationManager::instance()->RemoveRecords(recordsToDismiss);
    }
}

void EventsPage::onDismissSelected()
{
    // C# Reference: HistoryPage.tsmiDismissSelected_Click() line 464
    QList<OperationManager::OperationRecord*> selectedRecords = this->selectedCompletedRecords();
    if (selectedRecords.isEmpty())
    {
        QMessageBox::information(this, tr("Dismiss Selected"),
                               tr("No completed events are selected."));
        return;
    }

    this->dismissRecords(selectedRecords,
                         true,
                         tr("Dismiss Selected Events"),
                         tr("Are you sure you want to dismiss the selected completed events?"));
}

void EventsPage::onEventsContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = this->ui->eventsTable->indexAt(pos);
    if (index.isValid() && !this->ui->eventsTable->selectionModel()->isRowSelected(index.row(), QModelIndex()))
    {
        this->ui->eventsTable->clearSelection();
        this->ui->eventsTable->selectRow(index.row());
    }

    const QModelIndexList selectedRowIndexes = this->ui->eventsTable->selectionModel()->selectedRows();
    QList<int> selectedRows;
    selectedRows.reserve(selectedRowIndexes.size());
    for (const QModelIndex& rowIndex : selectedRowIndexes)
        selectedRows.append(rowIndex.row());

    QList<OperationManager::OperationRecord*> completedRecords =
        this->selectedCompletedRecords(index.isValid() ? index.row() : -1);

    QMenu menu(this->ui->eventsTable);
    QAction* copyAction = menu.addAction(tr("Copy"));
    QAction* dismissAction = menu.addAction(tr("Dismiss"));
    copyAction->setEnabled(!selectedRows.isEmpty());
    dismissAction->setEnabled(!completedRecords.isEmpty());

    QAction* chosen = menu.exec(this->ui->eventsTable->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == copyAction)
    {
        this->copyRowsToClipboard(selectedRows);
        return;
    }

    if (chosen == dismissAction)
    {
        const QString text = completedRecords.size() == 1
                                 ? tr("Are you sure you want to dismiss 1 selected completed event?")
                                 : tr("Are you sure you want to dismiss %1 selected completed events?").arg(completedRecords.size());
        this->dismissRecords(completedRecords, true, tr("Dismiss Selected Events"), text);
    }
}

void EventsPage::onEventsTableCellClicked(int row, int column)
{
    // C# Reference: HistoryPage.dataGridView_CellClick() line 330
    // Column 0 is the expander column
    if (column == 0)
    {
        this->toggleExpandedState(row);
    }
}

void EventsPage::onEventsTableCellDoubleClicked(int row, int column)
{
    // C# Reference: HistoryPage.dataGridView_CellDoubleClick() line 343
    // Double-clicking anywhere except the actions column toggles expanded state
    if (column != 5)  // Column 5 is actions
    {
        this->toggleExpandedState(row);
    }
}

void EventsPage::onEventsTableSelectionChanged()
{
    // C# Reference: HistoryPage.dataGridView_SelectionChanged() line 364
    this->updateButtons();
}

void EventsPage::onEventsTableHeaderClicked(int logicalIndex)
{
    // C# Reference: HistoryPage.dataGridView_ColumnHeaderMouseClick() line 370
    Q_UNUSED(logicalIndex);
    // Sorting is handled automatically by QTableWidget
    // but we may need to rebuild if filters are active
    this->buildRowList();
}

void EventsPage::onOperationAdded(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.History_CollectionChanged() line 110 (CollectionChangeAction.Add)
    if (!record || !this->isVisible())
        return;

    // Check if we should filter this record
    if (this->filterRecord(record))
        return;

    // Add the new record to the table
    this->createRecordRow(record);
    this->updateButtons();
}

void EventsPage::onOperationUpdated(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.action_Changed() line 122
    if (!record || !this->isVisible())
        return;

    this->updateRecordRow(record);
}

void EventsPage::onOperationRemoved(OperationManager::OperationRecord* record)
{
    // C# Reference: HistoryPage.History_CollectionChanged() line 110 (CollectionChangeAction.Remove)
    if (!record || !this->isVisible())
        return;

    this->removeRecordRow(record);
    this->updateButtons();
}

void EventsPage::onNewOperation(AsyncOperation* operation)
{
    // C# Reference: HistoryPage.Action_NewAction() line 97
    // The OperationManager already handles adding operations to its records list
    // and will emit recordAdded signal, so we don't need to do anything here
    Q_UNUSED(operation);
}

QList<OperationManager::OperationRecord*> EventsPage::selectedCompletedRecords(int fallbackRow) const
{
    QList<OperationManager::OperationRecord*> records;
    QSet<OperationManager::OperationRecord*> seen;

    auto appendFromRow = [&](int row)
    {
        if (row < 0 || row >= this->ui->eventsTable->rowCount())
            return;

        QTableWidgetItem* item = this->ui->eventsTable->item(row, 0);
        if (!item)
            return;

        auto* record = item->data(Qt::UserRole).value<OperationManager::OperationRecord*>();
        if (!record || record->state != AsyncOperation::Completed)
            return;

        if (seen.contains(record))
            return;

        seen.insert(record);
        records.append(record);
    };

    const QModelIndexList selectedRows = this->ui->eventsTable->selectionModel()->selectedRows();
    for (const QModelIndex& index : selectedRows)
        appendFromRow(index.row());

    if (records.isEmpty() && fallbackRow >= 0)
        appendFromRow(fallbackRow);

    return records;
}

void EventsPage::dismissRecords(const QList<OperationManager::OperationRecord*>& records,
                                bool confirm,
                                const QString& title,
                                const QString& text)
{
    if (records.isEmpty())
        return;

    if (confirm && !SettingsManager::instance().GetDoNotConfirmDismissEvents())
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(title);
        msgBox.setText(text);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);

        if (msgBox.exec() != QMessageBox::Yes)
            return;
    }

    OperationManager::instance()->RemoveRecords(records);
}

void EventsPage::copyRowsToClipboard(const QList<int>& rows) const
{
    if (rows.isEmpty())
        return;

    QList<int> sortedRows = rows;
    std::sort(sortedRows.begin(), sortedRows.end());

    QStringList lines;
    for (int row : sortedRows)
    {
        QStringList cells;
        // Status, Message, Location, Date (skip expander + actions columns)
        for (int col = 1; col <= 4; ++col)
        {
            QTableWidgetItem* item = this->ui->eventsTable->item(row, col);
            cells.append(item ? item->text() : QString());
        }
        lines.append(cells.join('\t'));
    }

    QApplication::clipboard()->setText(lines.join('\n'));
}
