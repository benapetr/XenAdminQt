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

#include "performancetabpage.h"
#include "ui_performancetabpage.h"
#include "controls/customdatagraph/graphlist.h"
#include "controls/customdatagraph/dataplotnav.h"
#include "controls/customdatagraph/dataeventlist.h"
#include "controls/customdatagraph/archivemaintainer.h"
#include "controls/customdatagraph/palette.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/general/getdatasourcesaction.h"
#include "xenlib/xen/actions/general/enabledatasourceaction.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xencache.h"
#include "xenlib/utils/misc.h"
#include "../widgets/tableclipboardutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QMap>
#include <QSet>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QColorDialog>

using namespace CustomDataGraph;

namespace
{
    static QDateTime parseMessageTimestampLocal(const QVariant& timestampValue, XenConnection* connection)
    {
        QDateTime timestamp;
        if (timestampValue.canConvert<QDateTime>())
            timestamp = timestampValue.toDateTime();

        if (!timestamp.isValid())
        {
            const QString raw = timestampValue.toString().trimmed();
            if (!raw.isEmpty())
                timestamp = Misc::ParseXenDateTime(raw);
        }

        if (!timestamp.isValid())
        {
            bool ok = false;
            const qint64 epoch = timestampValue.toLongLong(&ok);
            if (ok)
            {
                // Some paths pass Unix seconds, others milliseconds.
                if (epoch > 1000000000000LL || epoch < -1000000000000LL)
                    timestamp = QDateTime::fromMSecsSinceEpoch(epoch, Qt::UTC);
                else
                    timestamp = QDateTime::fromSecsSinceEpoch(epoch, Qt::UTC);
            }
        }

        if (!timestamp.isValid())
            return QDateTime();

        timestamp = timestamp.toUTC();

        if (connection)
            timestamp = timestamp.addSecs(connection->GetServerTimeOffsetSeconds());

        return timestamp.toLocalTime();
    }

    static bool isGraphMessageType(const QString& name)
    {
        const QString type = name.toUpper();
        return type == QStringLiteral("VM_CLONED")
               || type == QStringLiteral("VM_CRASHED")
               || type == QStringLiteral("VM_REBOOTED")
               || type == QStringLiteral("VM_RESUMED")
               || type == QStringLiteral("VM_SHUTDOWN")
               || type == QStringLiteral("VM_STARTED")
               || type == QStringLiteral("VM_SUSPENDED");
    }
}

PerformanceTabPage::PerformanceTabPage(QWidget* parent) : BaseTabPage(parent)
{
    this->ui = new Ui::PerformanceTabPage;
    this->m_graphList = new GraphList(this);
    this->m_dataPlotNav = new DataPlotNav(this);
    this->m_dataEventList = new DataEventList(this);
    this->m_archiveMaintainer = nullptr;
    this->m_graphActionsMenu = new QMenu(this);
    this->m_zoomMenu = new QMenu(this);

    this->ui->setupUi(this);
    if (QSplitter* splitter = this->findChild<QSplitter*>(QStringLiteral("contentSplitter")))
        splitter->setSizes({ 820, 260 });

    auto* graphLayout = new QVBoxLayout(this->ui->graphListContainer);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    graphLayout->addWidget(this->m_graphList);

    auto* navLayout = new QVBoxLayout(this->ui->plotNavContainer);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->addWidget(this->m_dataPlotNav);

    auto* eventLayout = new QVBoxLayout(this->ui->eventListContainer);
    eventLayout->setContentsMargins(0, 0, 0, 0);
    eventLayout->addWidget(this->m_dataEventList);

    this->m_dataEventList->SetPlotNav(this->m_dataPlotNav);
    this->m_graphList->SetDataPlotNav(this->m_dataPlotNav);
    this->m_graphList->SetDataEventList(this->m_dataEventList);

    connect(this->ui->graphActionsButton, &QPushButton::clicked, this, &PerformanceTabPage::onGraphActionsClicked);
    connect(this->ui->zoomButton, &QPushButton::clicked, this, &PerformanceTabPage::onZoomClicked);
    connect(this->ui->moveUpButton, &QPushButton::clicked, this, &PerformanceTabPage::onMoveUpClicked);
    connect(this->ui->moveDownButton, &QPushButton::clicked, this, &PerformanceTabPage::onMoveDownClicked);
    connect(this->m_graphList, &GraphList::SelectedGraphChanged, this, &PerformanceTabPage::onGraphSelectionChanged);

    QAction* newGraphAction = this->m_graphActionsMenu->addAction(tr("New Graph"));
    QAction* editGraphAction = this->m_graphActionsMenu->addAction(tr("Edit Graph"));
    QAction* deleteGraphAction = this->m_graphActionsMenu->addAction(tr("Delete Graph"));
    this->m_graphActionsMenu->addSeparator();
    QAction* restoreDefaultsAction = this->m_graphActionsMenu->addAction(tr("Restore Default Graphs"));

    connect(newGraphAction, &QAction::triggered, this, [this]()
    {
        DesignedGraph graph;
        graph.DisplayName = tr("New Graph");
        if (this->showGraphDetailsDialog(graph, false))
            this->m_graphList->AddGraph(graph);
    });

    connect(editGraphAction, &QAction::triggered, this, [this]()
    {
        if (this->m_graphList->SelectedGraphIndex() < 0)
            return;

        DesignedGraph graph = this->m_graphList->SelectedGraph();
        if (this->showGraphDetailsDialog(graph, true))
            this->m_graphList->ReplaceGraphAt(this->m_graphList->SelectedGraphIndex(), graph);
    });

    connect(deleteGraphAction, &QAction::triggered, this, [this]()
    {
        if (this->m_graphList->SelectedGraphIndex() < 0 || this->m_graphList->Count() <= 1)
            return;

        this->m_graphList->DeleteGraph(this->m_graphList->SelectedGraph());
    });

    connect(restoreDefaultsAction, &QAction::triggered, this->m_graphList, &GraphList::RestoreDefaultGraphs);

    QAction* lastYear = this->m_zoomMenu->addAction(tr("Last Year"));
    QAction* lastMonth = this->m_zoomMenu->addAction(tr("Last Month"));
    QAction* lastWeek = this->m_zoomMenu->addAction(tr("Last Week"));
    QAction* lastDay = this->m_zoomMenu->addAction(tr("Last Day"));
    QAction* lastHour = this->m_zoomMenu->addAction(tr("Last Hour"));
    QAction* lastTenMinutes = this->m_zoomMenu->addAction(tr("Last Ten Minutes"));

    connect(lastYear, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastYear);
    connect(lastMonth, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastMonth);
    connect(lastWeek, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastWeek);
    connect(lastDay, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastDay);
    connect(lastHour, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastHour);
    connect(lastTenMinutes, &QAction::triggered, this->m_dataPlotNav, &DataPlotNav::ZoomLastTenMinutes);

    this->updateButtons();
}

PerformanceTabPage::~PerformanceTabPage()
{
    this->disconnectConnectionSignals();

    ++this->m_dataSourcesLoadToken;
    if (this->m_getDataSourcesAction)
    {
        disconnect(this->m_getDataSourcesAction, nullptr, this, nullptr);
        this->m_getDataSourcesAction->Cancel();
        this->m_getDataSourcesAction = nullptr;
    }

    if (this->m_archiveMaintainer)
    {
        this->m_graphList->SetArchiveMaintainer(nullptr);
        this->m_dataPlotNav->SetArchiveMaintainer(nullptr);
        this->m_archiveMaintainer->Stop();
        this->m_archiveMaintainer->deleteLater();
        this->m_archiveMaintainer = nullptr;
    }

    delete this->ui;
}

bool PerformanceTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    return objectType == QStringLiteral("vm") || objectType == QStringLiteral("host");
}

void PerformanceTabPage::OnPageShown()
{
    this->m_pageVisible = true;
    this->initializeVisibleContent();
}

void PerformanceTabPage::OnPageHidden()
{
    this->m_pageVisible = false;

    if (this->m_archiveMaintainer)
        this->m_archiveMaintainer->Stop();

    this->disconnectConnectionSignals();

    ++this->m_dataSourcesLoadToken;
    if (this->m_getDataSourcesAction)
    {
        disconnect(this->m_getDataSourcesAction, nullptr, this, nullptr);
        this->m_getDataSourcesAction->Cancel();
        this->m_getDataSourcesAction = nullptr;
    }
}

void PerformanceTabPage::removeObject()
{
    this->m_needsVisibleInitialization = false;
    this->m_loadedGraphsObjectRef.clear();
    this->m_loadedGraphsObjectType = XenObjectType::Null;
    this->disconnectConnectionSignals();
    this->m_availableDataSources.clear();
    ++this->m_dataSourcesLoadToken;

    if (this->m_getDataSourcesAction)
    {
        disconnect(this->m_getDataSourcesAction, nullptr, this, nullptr);
        this->m_getDataSourcesAction->Cancel();
        this->m_getDataSourcesAction = nullptr;
    }

    if (this->m_archiveMaintainer)
    {
        this->m_graphList->SetArchiveMaintainer(nullptr);
        this->m_dataPlotNav->SetArchiveMaintainer(nullptr);
        this->m_archiveMaintainer->Stop();
        this->m_archiveMaintainer->deleteLater();
        this->m_archiveMaintainer = nullptr;
    }

    this->m_dataEventList->ClearEvents();
}

void PerformanceTabPage::refreshContent()
{
    if (!this->m_object)
        return;

    const bool graphLayoutReloadNeeded = (this->m_loadedGraphsObjectRef != this->m_objectRef)
                                         || (this->m_loadedGraphsObjectType != this->m_objectType);

    if (graphLayoutReloadNeeded)
    {
        this->disconnectConnectionSignals();
        this->m_dataEventList->ClearEvents();
        this->m_availableDataSources.clear();
        this->m_needsVisibleInitialization = true;

        ++this->m_dataSourcesLoadToken;
        if (this->m_getDataSourcesAction)
        {
            disconnect(this->m_getDataSourcesAction, nullptr, this, nullptr);
            this->m_getDataSourcesAction->Cancel();
            this->m_getDataSourcesAction = nullptr;
        }

        this->m_graphList->LoadGraphs(this->m_object.data());
        this->m_loadedGraphsObjectRef = this->m_objectRef;
        this->m_loadedGraphsObjectType = this->m_objectType;
    } else
    {
        // Same object update: keep maintainer/signals alive and refresh plots in-place.
        if (this->m_archiveMaintainer)
            this->m_archiveMaintainer->SetDataSourceIds(this->m_graphList->DisplayedUuids());

        if (this->m_pageVisible)
        {
            this->m_dataPlotNav->RefreshXRange(false);
            this->m_graphList->RefreshGraphs();
        }
    }

    this->updateButtons();

    if (this->m_pageVisible && this->m_needsVisibleInitialization)
        this->initializeVisibleContent();
}

void PerformanceTabPage::onGraphActionsClicked()
{
    this->updateButtons();
    this->m_graphActionsMenu->exec(this->ui->graphActionsButton->mapToGlobal(QPoint(0, this->ui->graphActionsButton->height())));
}

void PerformanceTabPage::onZoomClicked()
{
    this->m_zoomMenu->exec(this->ui->zoomButton->mapToGlobal(QPoint(0, this->ui->zoomButton->height())));
}

void PerformanceTabPage::onMoveUpClicked()
{
    const int index = this->m_graphList->SelectedGraphIndex();
    if (index > 0)
        this->m_graphList->ExchangeGraphs(index, index - 1);
}

void PerformanceTabPage::onMoveDownClicked()
{
    const int index = this->m_graphList->SelectedGraphIndex();
    if (index >= 0 && index < this->m_graphList->Count() - 1)
        this->m_graphList->ExchangeGraphs(index, index + 1);
}

void PerformanceTabPage::onGraphSelectionChanged()
{
    this->updateButtons();
}

void PerformanceTabPage::onArchivesUpdated()
{
    this->m_dataPlotNav->RefreshXRange(false);
    this->m_graphList->RefreshGraphs();
}

void PerformanceTabPage::onConnectionMessageReceived(const QString& messageRef, const QVariantMap& messageData)
{
    Q_UNUSED(messageRef);
    this->checkMessageForGraphs(messageData, true);
}

void PerformanceTabPage::onConnectionMessageRemoved(const QString& messageRef)
{
    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    const QVariantMap messageData = cache->ResolveObjectData(XenObjectType::Message, messageRef);
    if (messageData.isEmpty())
        return;

    this->checkMessageForGraphs(messageData, false);
}

QList<DataSourceItem> PerformanceTabPage::buildAvailableDataSources() const
{
    if (!this->m_availableDataSources.isEmpty())
        return this->m_availableDataSources;

    if (!this->m_graphList)
        return {};

    return this->m_graphList->AllDataSourceItems();
}

void PerformanceTabPage::loadDataSources()
{
    if (!this->m_object || !this->m_connection)
        return;

    ++this->m_dataSourcesLoadToken;
    const quint64 loadToken = this->m_dataSourcesLoadToken;

    if (this->m_getDataSourcesAction)
    {
        disconnect(this->m_getDataSourcesAction, nullptr, this, nullptr);
        this->m_getDataSourcesAction->Cancel();
        this->m_getDataSourcesAction = nullptr;
    }

    auto* action = new GetDataSourcesAction(this->m_connection, this->m_objectType, this->m_objectRef, this);
    this->m_getDataSourcesAction = action;

    connect(action, &GetDataSourcesAction::completed, this, [this, action, loadToken]()
    {
        if (this->m_getDataSourcesAction != action || loadToken != this->m_dataSourcesLoadToken || !this->m_object)
        {
            action->deleteLater();
            return;
        }

        this->m_availableDataSources = DataSourceItemList::BuildList(this->m_object.data(), action->DataSources());
        this->m_getDataSourcesAction = nullptr;
        action->deleteLater();
    });

    connect(action, &GetDataSourcesAction::failed, this, [this, action](const QString& error)
    {
        Q_UNUSED(error);
        if (this->m_getDataSourcesAction == action)
            this->m_getDataSourcesAction = nullptr;
        action->deleteLater();
    });

    connect(action, &GetDataSourcesAction::cancelled, this, [this, action]()
    {
        if (this->m_getDataSourcesAction == action)
            this->m_getDataSourcesAction = nullptr;
        action->deleteLater();
    });

    action->RunAsync();
}

bool PerformanceTabPage::showGraphDetailsDialog(DesignedGraph& graph, bool editMode)
{
    QDialog dialog(this);
    dialog.setWindowTitle(editMode ? tr("Edit Graph") : tr("New Graph"));
    dialog.resize(900, 560);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(graph.DisplayName);
    form->addRow(tr("Name"), nameEdit);
    layout->addLayout(form);

    auto* searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(tr("Search data sources..."));
    auto* showHidden = new QCheckBox(tr("Show Hidden"), &dialog);
    auto* showDisabled = new QCheckBox(tr("Show Disabled"), &dialog);

    auto* filterRow = new QWidget(&dialog);
    auto* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->addWidget(searchEdit, 1);
    filterLayout->addWidget(showHidden);
    filterLayout->addWidget(showDisabled);
    layout->addWidget(filterRow);

    auto* sourceTable = new QTableWidget(&dialog);
    sourceTable->setColumnCount(4);
    sourceTable->setHorizontalHeaderLabels({ tr("Display"), tr("Color"), tr("Data Source"), tr("Description") });
    sourceTable->horizontalHeader()->setStretchLastSection(true);
    sourceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    sourceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sourceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    sourceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    sourceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sourceTable->setSortingEnabled(true);

    auto* enableButton = new QPushButton(tr("Enable Selected Data Source"), &dialog);
    enableButton->setEnabled(false);

    layout->addWidget(sourceTable, 1);
    layout->addWidget(enableButton, 0, Qt::AlignLeft);

    const QList<DataSourceItem> availableRaw = this->buildAvailableDataSources();
    QMap<QString, DataSourceItem> itemById;
    for (const DataSourceItem& item : availableRaw)
        itemById.insert(item.Id, item);

    QSet<QString> selectedIds;
    for (const DataSourceItem& item : graph.DataSourceItems)
        selectedIds.insert(item.Id);

    auto updateEnableButtonState = [&]()
    {
        const QList<QTableWidgetSelectionRange> ranges = sourceTable->selectedRanges();
        if (ranges.isEmpty())
        {
            enableButton->setEnabled(false);
            return;
        }

        const int row = ranges.first().topRow();
        QTableWidgetItem* item = sourceTable->item(row, 2);
        if (!item)
        {
            enableButton->setEnabled(false);
            return;
        }

        const QString id = item->data(Qt::UserRole).toString();
        enableButton->setEnabled(itemById.contains(id) && !itemById.value(id).Enabled);
    };

    auto repopulateTable = [&]()
    {
        const TableClipboardUtils::SortState sortState = TableClipboardUtils::CaptureSortState(sourceTable);
        sourceTable->setSortingEnabled(false);
        sourceTable->setRowCount(0);

        const QString needle = searchEdit->text().trimmed();
        for (const DataSourceItem& item : itemById)
        {
            if (!showHidden->isChecked() && item.Hidden)
                continue;
            if (!showDisabled->isChecked() && !item.Enabled)
                continue;

            const QString displayName = item.FriendlyName.isEmpty() ? item.GetDataSource() : item.FriendlyName;
            const QString searchable = (displayName + QStringLiteral(" ") + item.GetDataSource()).toLower();
            if (!needle.isEmpty() && !searchable.contains(needle.toLower()))
                continue;

            const int row = sourceTable->rowCount();
            sourceTable->insertRow(row);

            auto* displayItem = new QTableWidgetItem();
            displayItem->setFlags(displayItem->flags() | Qt::ItemIsUserCheckable);
            displayItem->setCheckState(selectedIds.contains(item.Id) ? Qt::Checked : Qt::Unchecked);
            displayItem->setData(Qt::UserRole, item.Id);
            sourceTable->setItem(row, 0, displayItem);

            auto* colorItem = new QTableWidgetItem(QStringLiteral("    "));
            colorItem->setData(Qt::UserRole, item.Id);
            colorItem->setBackground(item.Color);
            sourceTable->setItem(row, 1, colorItem);

            auto* nameItem = new QTableWidgetItem(displayName);
            nameItem->setData(Qt::UserRole, item.Id);
            if (!item.Enabled)
                nameItem->setForeground(Qt::gray);
            sourceTable->setItem(row, 2, nameItem);

            auto* descItem = new QTableWidgetItem(item.GetDataSource());
            if (!item.Enabled)
                descItem->setForeground(Qt::gray);
            sourceTable->setItem(row, 3, descItem);
        }

        TableClipboardUtils::RestoreSortState(sourceTable, sortState, 0, Qt::AscendingOrder);
        updateEnableButtonState();
    };

    repopulateTable();

    connect(searchEdit, &QLineEdit::textChanged, &dialog, [repopulateTable]() { repopulateTable(); });
    connect(showHidden, &QCheckBox::toggled, &dialog, [repopulateTable]() { repopulateTable(); });
    connect(showDisabled, &QCheckBox::toggled, &dialog, [repopulateTable]() { repopulateTable(); });
    connect(sourceTable, &QTableWidget::itemSelectionChanged, &dialog, updateEnableButtonState);
    connect(sourceTable, &QTableWidget::itemChanged, &dialog, [&](QTableWidgetItem* item)
    {
        if (!item || item->column() != 0)
            return;

        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty())
            return;

        if (item->checkState() == Qt::Checked)
            selectedIds.insert(id);
        else
            selectedIds.remove(id);
    });
    connect(sourceTable, &QTableWidget::itemDoubleClicked, &dialog, [&](QTableWidgetItem* item)
    {
        if (!item || item->column() != 1)
            return;

        const QString id = item->data(Qt::UserRole).toString();
        if (!itemById.contains(id))
            return;

        const QColor picked = QColorDialog::getColor(itemById.value(id).Color, &dialog, tr("Select Data Source Color"));
        if (!picked.isValid())
            return;

        DataSourceItem value = itemById.value(id);
        value.Color = picked;
        value.ColorChanged = true;
        itemById[id] = value;
        item->setBackground(picked);
    });
    connect(enableButton, &QPushButton::clicked, &dialog, [&]()
    {
        const QList<QTableWidgetSelectionRange> ranges = sourceTable->selectedRanges();
        if (ranges.isEmpty())
            return;

        const int row = ranges.first().topRow();
        QTableWidgetItem* nameItem = sourceTable->item(row, 2);
        if (!nameItem)
            return;

        const QString id = nameItem->data(Qt::UserRole).toString();
        if (!itemById.contains(id))
            return;

        const DataSourceItem source = itemById.value(id);
        if (source.Enabled)
            return;

        auto* action = new EnableDataSourceAction(this->m_connection,
                                                  this->m_objectType,
                                                  this->m_objectRef,
                                                  source.DataSource.NameLabel,
                                                  source.FriendlyName.isEmpty() ? source.GetDataSource() : source.FriendlyName,
                                                  &dialog);
        action->RunSync(this->m_connection->GetSession());
        const QList<QVariantMap> reloaded = action->DataSources();
        action->deleteLater();

        if (!reloaded.isEmpty())
        {
            const QList<DataSourceItem> refreshed = DataSourceItemList::BuildList(this->m_object.data(), reloaded);
            this->m_availableDataSources = refreshed;
            QMap<QString, DataSourceItem> refreshedById;
            for (const DataSourceItem& item : refreshed)
            {
                DataSourceItem updated = item;
                if (itemById.contains(item.Id) && itemById.value(item.Id).ColorChanged)
                {
                    updated.Color = itemById.value(item.Id).Color;
                    updated.ColorChanged = true;
                }
                refreshedById[item.Id] = updated;
            }
            itemById = refreshedById;
            repopulateTable();
        }
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    graph.DisplayName = nameEdit->text().trimmed();
    graph.DataSourceItems.clear();
    for (const QString& id : selectedIds)
    {
        if (itemById.contains(id))
            graph.DataSourceItems.append(itemById.value(id));
    }

    return !graph.DataSourceItems.isEmpty();
}

void PerformanceTabPage::updateButtons()
{
    const int index = this->m_graphList->SelectedGraphIndex();
    this->ui->moveUpButton->setEnabled(index > 0);
    this->ui->moveDownButton->setEnabled(index >= 0 && index < this->m_graphList->Count() - 1);
}

void PerformanceTabPage::loadEvents()
{
    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();
    if (!cache)
        return;

    const QList<QVariantMap> messages = cache->GetAllData(XenObjectType::Message);
    for (const QVariantMap& messageData : messages)
        this->checkMessageForGraphs(messageData, true);
}

void PerformanceTabPage::checkMessageForGraphs(const QVariantMap& messageData, bool add)
{
    const QString messageType = messageData.value(QStringLiteral("name")).toString();
    if (!isGraphMessageType(messageType))
        return;

    if (messageData.value(QStringLiteral("cls")).toString().toLower() != QStringLiteral("vm"))
        return;

    const QString messageVmUuid = messageData.value(QStringLiteral("obj_uuid")).toString();
    if (messageVmUuid.isEmpty())
        return;

    bool applies = false;
    QString vmName;

    if (this->m_objectType == XenObjectType::VM)
    {
        applies = (this->m_object->GetUUID() == messageVmUuid);
        if (applies && this->m_object)
            vmName = this->m_object->GetName();
    } else if (this->m_objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = this->m_connection->GetCache()->ResolveObject<Host>(this->m_objectRef);
        if (host)
        {
            const QList<QSharedPointer<VM>> resident = host->GetResidentVMs();
            for (const QSharedPointer<VM>& vm : resident)
            {
                if (vm && vm->GetUUID() == messageVmUuid)
                {
                    applies = true;
                    vmName = vm->GetName();
                    break;
                }
            }
        }
    }

    if (!applies)
        return;

    if (vmName.isEmpty())
        vmName = messageVmUuid;

    const QDateTime timestamp = parseMessageTimestampLocal(messageData.value(QStringLiteral("timestamp")), this->m_connection);
    const qint64 ticks = timestamp.isValid() ? timestamp.toMSecsSinceEpoch() : 0;

    DataEvent event(ticks, 0, messageType, messageVmUuid, vmName);
    if (add)
        this->m_dataEventList->AddEvent(event);
    else
        this->m_dataEventList->RemoveEvent(event);
}

void PerformanceTabPage::disconnectConnectionSignals()
{
    if (!this->m_connection)
        return;

    disconnect(this->m_connection, &XenConnection::MessageReceived, this, &PerformanceTabPage::onConnectionMessageReceived);
    disconnect(this->m_connection, &XenConnection::MessageRemoved, this, &PerformanceTabPage::onConnectionMessageRemoved);
}

void PerformanceTabPage::connectConnectionSignals()
{
    if (!this->m_connection)
        return;

    connect(this->m_connection, &XenConnection::MessageReceived, this, &PerformanceTabPage::onConnectionMessageReceived, Qt::UniqueConnection);
    connect(this->m_connection, &XenConnection::MessageRemoved, this, &PerformanceTabPage::onConnectionMessageRemoved, Qt::UniqueConnection);
}

void PerformanceTabPage::initializeVisibleContent()
{
    if (!this->m_pageVisible || !this->m_object)
        return;

    if (!this->m_needsVisibleInitialization && this->m_archiveMaintainer)
    {
        this->m_archiveMaintainer->SetDataSourceIds(this->m_graphList->DisplayedUuids());
        this->m_archiveMaintainer->Start();
        this->m_dataEventList->ClearEvents();
        this->loadEvents();
        this->connectConnectionSignals();
        return;
    }

    if (this->m_archiveMaintainer)
    {
        this->m_graphList->SetArchiveMaintainer(nullptr);
        this->m_dataPlotNav->SetArchiveMaintainer(nullptr);
        this->m_archiveMaintainer->Stop();
        this->m_archiveMaintainer->deleteLater();
        this->m_archiveMaintainer = nullptr;
    }

    this->loadDataSources();
    this->m_dataEventList->ClearEvents();
    this->loadEvents();
    this->connectConnectionSignals();

    this->m_archiveMaintainer = new ArchiveMaintainer(this->m_object.data(), this);
    this->m_graphList->SetArchiveMaintainer(this->m_archiveMaintainer);
    this->m_dataPlotNav->SetArchiveMaintainer(this->m_archiveMaintainer);
    connect(this->m_archiveMaintainer, &ArchiveMaintainer::ArchivesUpdated, this, &PerformanceTabPage::onArchivesUpdated, Qt::UniqueConnection);
    this->m_archiveMaintainer->SetDataSourceIds(this->m_graphList->DisplayedUuids());
    this->m_archiveMaintainer->Start();

    this->m_needsVisibleInitialization = false;
}
