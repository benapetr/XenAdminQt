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

#ifndef EVENTSPAGE_H
#define EVENTSPAGE_H

#include "notificationsbasepage.h"
#include "operations/operationmanager.h"
#include <QTableWidgetItem>
#include <xen/asyncoperation.h>

namespace Ui {
class EventsPage;
}

/**
 * Events page showing action history (events) in the system.
 * 
 * C# Equivalent: xenadmin/XenAdmin/TabPages/HistoryPage.cs
 * 
 * NOTE: Renamed from HistoryPage to EventsPage to avoid conflict with
 * widgets/historypage.h (which shows OperationManager records).
 * 
 * This page displays a list of all operations from OperationManager,
 * showing their status, message, location, date, and available actions.
 * Users can filter by status (Success/Error/In Progress), location (server/pool),
 * and date range.
 */
class EventsPage : public NotificationsBasePage
{
    Q_OBJECT

public:
    explicit EventsPage(QWidget* parent = nullptr);
    ~EventsPage();

    // NotificationsBasePage overrides
    NavigationPane::NotificationsSubMode notificationsSubMode() const override
    {
        return NavigationPane::Events;
    }

    QString helpID() const override
    {
        // C# Reference: HistoryPage.HelpID line 81
        return "EventsPane";
    }

    bool filterIsOn() const override;

protected:
    // NotificationsBasePage overrides
    void refreshPage() override;
    void registerEventHandlers() override;
    void deregisterEventHandlers() override;

private slots:
    void onFilterStatusChanged();
    void onFilterLocationChanged();
    void onFilterDatesChanged();
    void onDismissAll();
    void onDismissSelected();
    void onEventsTableCellClicked(int row, int column);
    void onEventsTableCellDoubleClicked(int row, int column);
    void onEventsTableSelectionChanged();
    void onEventsTableHeaderClicked(int logicalIndex);

private:
    Ui::EventsPage* ui;

    static constexpr int MAX_HISTORY_ITEM = 1000;

    /**
     * Build the row list from OperationManager.
     * C# Reference: HistoryPage.BuildRowList() line 178
     */
    void buildRowList();

    /**
     * Sort records based on date (descending by default).
     * C# Reference: HistoryPage.SortActions() line 203
     */
    void sortRecords(QList<OperationManager::OperationRecord*>& records);

    /**
     * Returns true if the record should be filtered out.
     * C# Reference: HistoryPage.FilterAction() line 224
     */
    bool filterRecord(OperationManager::OperationRecord* record);

    /**
     * Create a table row for the given record.
     * C# Reference: HistoryPage.CreateActionRow() line 250
     */
    void createRecordRow(OperationManager::OperationRecord* record);

    /**
     * Remove the row for the given record.
     * C# Reference: HistoryPage.RemoveActionRow() line 268
     */
    void removeRecordRow(OperationManager::OperationRecord* record);

    /**
     * Update the row for the given record.
     * C# Reference: HistoryPage.action_Changed() line 122
     */
    void updateRecordRow(OperationManager::OperationRecord* record);

    /**
     * Find the row corresponding to the given record.
     * C# Reference: HistoryPage.FindRowFromAction() line 281
     */
    int findRowFromRecord(OperationManager::OperationRecord* record);

    /**
     * Get status text for operation state.
     */
    QString getStatusText(AsyncOperation::OperationState state) const;

    /**
     * Update button states based on selection.
     * C# Reference: HistoryPage.UpdateButtons() line 292
     */
    void updateButtons();

    /**
     * Toggle the expanded/collapsed state of a row.
     * C# Reference: HistoryPage.ToggleExpandedState() line 303
     */
    void toggleExpandedState(int row);

    QString buildRecordTitle(OperationManager::OperationRecord* record) const;
    QString buildRecordDescription(OperationManager::OperationRecord* record) const;
    QString buildRecordDetails(OperationManager::OperationRecord* record) const;
    QString formatElapsedTime(OperationManager::OperationRecord* record) const;

    // Slots for OperationManager events
    void onOperationAdded(OperationManager::OperationRecord* record);
    void onOperationUpdated(OperationManager::OperationRecord* record);
    void onOperationRemoved(OperationManager::OperationRecord* record);
    void onNewOperation(AsyncOperation* operation);

    // Filter state tracking
    // C# Reference: HistoryPage has toolStripDdbFilterStatus, toolStripDdbFilterLocation, toolStripDdbFilterDates
    QSet<AsyncOperation::OperationState> m_statusFilters;  // Which statuses to show
    QStringList m_locationFilters;  // Which locations to show
    QDateTime m_dateFilterFrom;     // Show operations after this date
    QDateTime m_dateFilterTo;       // Show operations before this date
    bool m_dateFilterEnabled = false;

    // Row expanded state tracking
    QSet<int> m_expandedRows;  // Set of row indices that are expanded
};

#endif // EVENTSPAGE_H
