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

#include "graphlist.h"
#include "dataplot.h"
#include "datakey.h"
#include "archivemaintainer.h"
#include "dataplotnav.h"
#include "dataeventlist.h"
#include "palette.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostcpu.h"
#include "xenlib/xen/vif.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/general/getdatasourcesaction.h"
#include "xenlib/xen/actions/general/savedatasourcestateaction.h"
#include "xenlib/xencache.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSet>

namespace CustomDataGraph
{
    GraphList::GraphList(QWidget* parent) : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
    }

    void GraphList::SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer)
    {
        this->m_archiveMaintainer = archiveMaintainer;

        for (DataPlot* plot : this->m_plots)
        {
            if (plot)
                plot->SetArchiveMaintainer(this->m_archiveMaintainer);
        }

        for (DataKey* key : this->m_keys)
        {
            if (key)
                key->SetArchiveMaintainer(this->m_archiveMaintainer);
        }

        if (this->m_archiveMaintainer)
            this->m_archiveMaintainer->SetDataSourceIds(this->m_displayedUuids);
    }

    void GraphList::SetDataPlotNav(DataPlotNav* dataPlotNav)
    {
        if (this->m_dataPlotNav)
            disconnect(this->m_plotNavRangeChangedConnection);

        this->m_dataPlotNav = dataPlotNav;

        if (this->m_dataPlotNav)
        {
            this->m_plotNavRangeChangedConnection = connect(this->m_dataPlotNav,
                                                            &DataPlotNav::RangeChanged,
                                                            this,
                                                            &GraphList::RefreshGraphs);
        }

        for (DataPlot* plot : this->m_plots)
        {
            if (plot)
                plot->SetDataPlotNav(this->m_dataPlotNav);
        }
    }

    void GraphList::SetDataEventList(DataEventList* dataEventList)
    {
        this->m_dataEventList = dataEventList;

        for (DataPlot* plot : this->m_plots)
        {
            if (plot)
                plot->SetDataEventList(this->m_dataEventList);
        }
    }

    void GraphList::SetGraphs(const QList<DesignedGraph>& items)
    {
        this->m_graphs = items;
        this->m_selectedGraphIndex = this->m_graphs.isEmpty() ? -1 : qBound(0, this->m_selectedGraphIndex, this->m_graphs.size() - 1);
        if (this->m_selectedGraphIndex < 0 && !this->m_graphs.isEmpty())
            this->m_selectedGraphIndex = 0;

        this->updateDisplayedUuids();
        this->rebuildUi();

        emit this->SelectedGraphChanged();
        emit this->GraphsChanged();
    }

    void GraphList::LoadGraphs(XenObject* xmo)
    {
        this->m_xenObject = xmo;

        QList<DesignedGraph> loaded = this->loadGraphsFromGuiConfig();
        if (loaded.isEmpty())
        {
            loaded = this->defaultGraphsFor(xmo);
            this->m_showingDefaultGraphs = true;
        } else
        {
            this->m_showingDefaultGraphs = false;
        }

        this->m_selectedGraphIndex = loaded.isEmpty() ? -1 : 0;
        this->SetGraphs(loaded);
    }

    void GraphList::SaveGraphs(const QList<DataSourceItem>& dataSourceItems)
    {
        if (!this->m_xenObject)
            return;

        XenConnection* connection = this->m_xenObject->GetConnection();
        if (!connection)
            return;

        XenCache* cache = connection->GetCache();
        if (!cache)
            return;

        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (!pool)
            return;

        QList<DataSourceItem> items = dataSourceItems;
        if (items.isEmpty())
            items = this->fetchDataSources();
        if (items.isEmpty())
            items = this->getGraphsDataSources();

        this->updateDataSources(items);

        QVariantMap newGuiConfig = this->buildUpdatedGuiConfig(this->m_showingDefaultGraphs ? QList<DesignedGraph>() : this->m_graphs);
        QList<QVariantMap> dataSourceStates = this->buildDataSourceStatePayload(items);

        auto* action = new SaveDataSourceStateAction(connection,
                                                     this->m_xenObject->GetObjectType(),
                                                     this->m_xenObject->OpaqueRef(),
                                                     dataSourceStates,
                                                     newGuiConfig,
                                                     this);
        action->RunSync(connection->GetSession());
        action->deleteLater();

        this->m_showingDefaultGraphs = this->m_graphs.isEmpty();
    }

    void GraphList::RestoreDefaultGraphs()
    {
        this->m_showingDefaultGraphs = true;
        this->SetGraphs(this->defaultGraphsFor(this->m_xenObject));
        this->SaveGraphs();
    }

    void GraphList::ExchangeGraphs(int index1, int index2)
    {
        if (index1 < 0 || index2 < 0 || index1 >= this->m_graphs.size() || index2 >= this->m_graphs.size())
            return;

        this->m_graphs.swapItemsAt(index1, index2);
        this->m_selectedGraphIndex = index2;
        this->m_showingDefaultGraphs = false;
        this->SetGraphs(this->m_graphs);
        this->SaveGraphs();
    }

    void GraphList::DeleteGraph(const DesignedGraph& graph)
    {
        const int index = this->m_graphs.indexOf(graph);
        if (index < 0)
            return;

        this->m_graphs.removeAt(index);
        this->m_selectedGraphIndex = this->m_graphs.isEmpty() ? -1 : qMin(index, this->m_graphs.size() - 1);
        this->m_showingDefaultGraphs = false;
        this->SetGraphs(this->m_graphs);
        this->SaveGraphs();
    }

    void GraphList::AddGraph(const DesignedGraph& graph)
    {
        this->m_graphs.append(graph);
        this->m_selectedGraphIndex = this->m_graphs.size() - 1;
        this->m_showingDefaultGraphs = false;
        this->SetGraphs(this->m_graphs);
        this->SaveGraphs();
    }

    void GraphList::ReplaceGraphAt(int index, const DesignedGraph& graph)
    {
        if (index < 0 || index >= this->m_graphs.size())
            return;

        this->m_graphs[index] = graph;
        this->m_showingDefaultGraphs = false;
        this->SetGraphs(this->m_graphs);
        this->SaveGraphs();
    }

    int GraphList::Count() const
    {
        return this->m_graphs.size();
    }

    int GraphList::SelectedGraphIndex() const
    {
        return this->m_selectedGraphIndex;
    }

    DesignedGraph GraphList::SelectedGraph() const
    {
        if (this->m_selectedGraphIndex < 0 || this->m_selectedGraphIndex >= this->m_graphs.size())
            return DesignedGraph();

        return this->m_graphs.at(this->m_selectedGraphIndex);
    }

    void GraphList::SetSelectedGraph(const DesignedGraph& graph)
    {
        const int index = this->m_graphs.indexOf(graph);
        if (index < 0)
            return;

        this->m_selectedGraphIndex = index;
        this->applySelectionState();
        emit this->SelectedGraphChanged();
    }

    bool GraphList::ShowingDefaultGraphs() const
    {
        return this->m_showingDefaultGraphs;
    }

    QList<QString> GraphList::DisplayedUuids() const
    {
        return this->m_displayedUuids;
    }

    QList<QString> GraphList::DisplayNames() const
    {
        QList<QString> names;
        for (const DesignedGraph& graph : this->m_graphs)
            names.append(graph.DisplayName);
        return names;
    }

    QList<DataSourceItem> GraphList::AllDataSourceItems() const
    {
        QMap<QString, DataSourceItem> dedup;
        for (const DesignedGraph& graph : this->m_graphs)
        {
            for (const DataSourceItem& item : graph.DataSourceItems)
            {
                dedup[item.Id] = item;
            }
        }
        return dedup.values();
    }

    void GraphList::RefreshGraphs()
    {
        for (DataPlot* plot : this->m_plots)
        {
            if (plot)
                plot->RefreshData();
        }

        for (DataKey* key : this->m_keys)
        {
            if (key)
                key->UpdateItems();
        }
    }

    void GraphList::rebuildUi()
    {
        auto* mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
        if (!mainLayout)
            return;

        while (QLayoutItem* item = mainLayout->takeAt(0))
        {
            if (item->widget())
                delete item->widget();
            delete item;
        }

        this->m_plots.clear();
        this->m_keys.clear();

        for (int i = 0; i < this->m_graphs.size(); ++i)
        {
            const DesignedGraph& graph = this->m_graphs.at(i);

            auto* row = new QWidget(this);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);

            auto* plot = new DataPlot(row);
            plot->SetArchiveMaintainer(this->m_archiveMaintainer);
            plot->SetDataPlotNav(this->m_dataPlotNav);
            plot->SetDataEventList(this->m_dataEventList);
            plot->SetDisplayName(graph.DisplayName);
            connect(plot, &DataPlot::Clicked, row, [this, i]()
            {
                if (i < 0 || i >= this->m_graphs.size() || i == this->m_selectedGraphIndex)
                    return;

                this->m_selectedGraphIndex = i;
                this->applySelectionState();
                emit this->SelectedGraphChanged();
            });

            auto* key = new DataKey(row);
            key->SetArchiveMaintainer(this->m_archiveMaintainer);
            key->setMinimumWidth(240);

            QList<QString> ids;
            QMap<QString, QString> friendlyById;
            for (const DataSourceItem& item : graph.DataSourceItems)
            {
                ids.append(item.Id);
                friendlyById.insert(item.Id, item.FriendlyName);
            }

            plot->SetDataSourceUUIDsToShow(ids);
            key->SetDataSourceUUIDsToShow(ids);
            key->SetFriendlyNames(friendlyById);
            key->UpdateItems();

            rowLayout->addWidget(plot, 1);
            rowLayout->addWidget(key);

            mainLayout->addWidget(row);
            this->m_plots.append(plot);
            this->m_keys.append(key);
        }

        this->applySelectionState();
        mainLayout->addStretch();
    }

    void GraphList::applySelectionState()
    {
        for (int i = 0; i < this->m_plots.size(); ++i)
        {
            DataPlot* plot = this->m_plots.at(i);
            if (!plot)
                continue;

            plot->SetIsSelected(i == this->m_selectedGraphIndex);
        }
    }

    QList<DesignedGraph> GraphList::defaultGraphsFor(XenObject* xmo) const
    {
        QList<DesignedGraph> graphs;
        if (!xmo)
            return graphs;

        XenConnection* connection = xmo->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return graphs;

        if (xmo->GetObjectType() == XenObjectType::Host)
        {
            QSharedPointer<Host> host = cache->ResolveObject<Host>(xmo->OpaqueRef());
            if (!host)
                return graphs;

            DesignedGraph cpuGraph;
            cpuGraph.DisplayName = tr("CPU");
            QSet<QString> usedCpuMetrics;
            int cpuIndex = 0;
            for (const QString& hostCpuRef : host->GetHostCPURefs())
            {
                QSharedPointer<HostCPU> cpu = cache->ResolveObject<HostCPU>(XenObjectType::HostCPU, hostCpuRef);
                if (!cpu)
                {
                    ++cpuIndex;
                    continue;
                }

                QString metricName = QStringLiteral("cpu%1").arg(cpu->Number());
                if (usedCpuMetrics.contains(metricName))
                    metricName = QStringLiteral("cpu%1").arg(cpuIndex);
                usedCpuMetrics.insert(metricName);

                const QString dsId = Palette::GetUuid(metricName, xmo);
                DataSourceDescriptor source;
                source.NameLabel = metricName;
                cpuGraph.DataSourceItems.append(DataSourceItem(source,
                                                               DataSourceItemList::GetFriendlyDataSourceName(metricName, xmo),
                                                               Palette::GetColour(dsId),
                                                               dsId));
                ++cpuIndex;
            }

            DesignedGraph memoryGraph;
            memoryGraph.DisplayName = tr("Memory");
            {
                const QString total = QStringLiteral("memory_total_kib");
                const QString free = QStringLiteral("memory_free_kib");
                DataSourceDescriptor totalDs; totalDs.NameLabel = total;
                DataSourceDescriptor freeDs; freeDs.NameLabel = free;
                memoryGraph.DataSourceItems.append(DataSourceItem(totalDs,
                                                                  DataSourceItemList::GetFriendlyDataSourceName(total, xmo),
                                                                  Palette::GetColour(Palette::GetUuid(total, xmo)),
                                                                  Palette::GetUuid(total, xmo)));
                memoryGraph.DataSourceItems.append(DataSourceItem(freeDs,
                                                                  DataSourceItemList::GetFriendlyDataSourceName(free, xmo),
                                                                  Palette::GetColour(Palette::GetUuid(free, xmo)),
                                                                  Palette::GetUuid(free, xmo)));
            }

            DesignedGraph networkGraph;
            networkGraph.DisplayName = tr("Network");
            for (const QSharedPointer<PIF>& pif : host->GetPIFs())
            {
                if (!pif)
                    continue;
                const QString tx = QStringLiteral("pif_%1_tx").arg(pif->GetDevice());
                const QString rx = QStringLiteral("pif_%1_rx").arg(pif->GetDevice());
                DataSourceDescriptor txDs; txDs.NameLabel = tx;
                DataSourceDescriptor rxDs; rxDs.NameLabel = rx;
                networkGraph.DataSourceItems.append(DataSourceItem(txDs,
                                                                   DataSourceItemList::GetFriendlyDataSourceName(tx, xmo),
                                                                   Palette::GetColour(Palette::GetUuid(tx, xmo)),
                                                                   Palette::GetUuid(tx, xmo)));
                networkGraph.DataSourceItems.append(DataSourceItem(rxDs,
                                                                   DataSourceItemList::GetFriendlyDataSourceName(rx, xmo),
                                                                   Palette::GetColour(Palette::GetUuid(rx, xmo)),
                                                                   Palette::GetUuid(rx, xmo)));
            }

            graphs.append(cpuGraph);
            graphs.append(memoryGraph);
            graphs.append(networkGraph);
        } else if (xmo->GetObjectType() == XenObjectType::VM)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(xmo->OpaqueRef());
            if (!vm)
                return graphs;

            DesignedGraph cpuGraph;
            cpuGraph.DisplayName = tr("CPU");
            for (int i = 0; i < vm->VCPUsAtStartup(); ++i)
            {
                const QString metric = QStringLiteral("cpu%1").arg(i);
                DataSourceDescriptor ds; ds.NameLabel = metric;
                cpuGraph.DataSourceItems.append(DataSourceItem(ds,
                                                               DataSourceItemList::GetFriendlyDataSourceName(metric, xmo),
                                                               Palette::GetColour(Palette::GetUuid(metric, xmo)),
                                                               Palette::GetUuid(metric, xmo)));
            }

            DesignedGraph memoryGraph;
            memoryGraph.DisplayName = tr("Memory");
            {
                const QString total = QStringLiteral("memory");
                const QString free = QStringLiteral("memory_internal_free");
                DataSourceDescriptor totalDs; totalDs.NameLabel = total;
                DataSourceDescriptor freeDs; freeDs.NameLabel = free;
                memoryGraph.DataSourceItems.append(DataSourceItem(totalDs,
                                                                  DataSourceItemList::GetFriendlyDataSourceName(total, xmo),
                                                                  Palette::GetColour(Palette::GetUuid(total, xmo)),
                                                                  Palette::GetUuid(total, xmo)));
                memoryGraph.DataSourceItems.append(DataSourceItem(freeDs,
                                                                  DataSourceItemList::GetFriendlyDataSourceName(free, xmo),
                                                                  Palette::GetColour(Palette::GetUuid(free, xmo)),
                                                                  Palette::GetUuid(free, xmo)));
            }

            DesignedGraph networkGraph;
            networkGraph.DisplayName = tr("Network");
            for (const QSharedPointer<VIF>& vif : vm->GetVIFs())
            {
                if (!vif)
                    continue;
                const QString tx = QStringLiteral("vif_%1_tx").arg(vif->GetDevice());
                const QString rx = QStringLiteral("vif_%1_rx").arg(vif->GetDevice());
                DataSourceDescriptor txDs; txDs.NameLabel = tx;
                DataSourceDescriptor rxDs; rxDs.NameLabel = rx;
                networkGraph.DataSourceItems.append(DataSourceItem(txDs,
                                                                   DataSourceItemList::GetFriendlyDataSourceName(tx, xmo),
                                                                   Palette::GetColour(Palette::GetUuid(tx, xmo)),
                                                                   Palette::GetUuid(tx, xmo)));
                networkGraph.DataSourceItems.append(DataSourceItem(rxDs,
                                                                   DataSourceItemList::GetFriendlyDataSourceName(rx, xmo),
                                                                   Palette::GetColour(Palette::GetUuid(rx, xmo)),
                                                                   Palette::GetUuid(rx, xmo)));
            }

            DesignedGraph diskGraph;
            diskGraph.DisplayName = tr("Disk");
            for (const QSharedPointer<VBD>& vbd : vm->GetVBDs())
            {
                if (!vbd)
                    continue;
                const QString read = QStringLiteral("vbd_%1_read").arg(vbd->GetDevice());
                const QString write = QStringLiteral("vbd_%1_write").arg(vbd->GetDevice());
                DataSourceDescriptor readDs; readDs.NameLabel = read;
                DataSourceDescriptor writeDs; writeDs.NameLabel = write;
                diskGraph.DataSourceItems.append(DataSourceItem(readDs,
                                                                DataSourceItemList::GetFriendlyDataSourceName(read, xmo),
                                                                Palette::GetColour(Palette::GetUuid(read, xmo)),
                                                                Palette::GetUuid(read, xmo)));
                diskGraph.DataSourceItems.append(DataSourceItem(writeDs,
                                                                DataSourceItemList::GetFriendlyDataSourceName(write, xmo),
                                                                Palette::GetColour(Palette::GetUuid(write, xmo)),
                                                                Palette::GetUuid(write, xmo)));
            }

            graphs.append(cpuGraph);
            graphs.append(memoryGraph);
            graphs.append(networkGraph);
            graphs.append(diskGraph);
        }

        return graphs;
    }

    void GraphList::updateDisplayedUuids()
    {
        QSet<QString> unique;
        this->m_displayedUuids.clear();

        for (const DesignedGraph& graph : this->m_graphs)
        {
            for (const DataSourceItem& item : graph.DataSourceItems)
            {
                if (!unique.contains(item.Id))
                {
                    unique.insert(item.Id);
                    this->m_displayedUuids.append(item.Id);
                }
            }
        }

        if (this->m_dataPlotNav)
            this->m_dataPlotNav->SetDisplayedUuids(this->m_displayedUuids);

        if (this->m_archiveMaintainer)
            this->m_archiveMaintainer->SetDataSourceIds(this->m_displayedUuids);
    }

    QString GraphList::objectTypeToken() const
    {
        if (!this->m_xenObject)
            return QString();

        return this->m_xenObject->GetObjectType() == XenObjectType::Host
                   ? QStringLiteral("host")
                   : QStringLiteral("vm");
    }

    QString GraphList::objectUuid() const
    {
        if (!this->m_xenObject)
            return QString();

        return this->m_xenObject->GetUUID();
    }

    QList<DesignedGraph> GraphList::loadGraphsFromGuiConfig() const
    {
        QList<DesignedGraph> result;

        if (!this->m_xenObject)
            return result;

        XenConnection* connection = this->m_xenObject->GetConnection();
        if (!connection)
            return result;

        XenCache* cache = connection->GetCache();
        if (!cache)
            return result;

        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (!pool)
            return result;

        const QVariantMap guiConfig = pool->GUIConfig();
        for (int i = 0;; ++i)
        {
            const QString layoutKey = Palette::GetLayoutKey(i, this->m_xenObject);
            if (!guiConfig.contains(layoutKey))
                break;

            const QString layout = guiConfig.value(layoutKey).toString();
            const QStringList dataSources = layout.split(',', Qt::SkipEmptyParts);

            DesignedGraph graph;
            for (const QString& sourceName : dataSources)
            {
                const QString trimmed = sourceName.trimmed();
                if (trimmed.isEmpty())
                    continue;

                const QString id = Palette::GetUuid(trimmed, this->m_xenObject);
                DataSourceDescriptor source;
                source.NameLabel = trimmed;
                graph.DataSourceItems.append(DataSourceItem(source, trimmed, Palette::GetColour(id), id));
            }

            const QString nameKey = Palette::GetGraphNameKey(i, this->m_xenObject);
            graph.DisplayName = guiConfig.value(nameKey).toString();

            if (graph.DisplayName.isEmpty())
                graph.DisplayName = tr("Graph %1").arg(i + 1);

            result.append(graph);
        }

        return result;
    }

    QVariantMap GraphList::buildUpdatedGuiConfig(const QList<DesignedGraph>& graphsToPersist) const
    {
        QVariantMap updated;

        if (!this->m_xenObject)
            return updated;

        XenConnection* connection = this->m_xenObject->GetConnection();
        if (!connection)
            return updated;

        XenCache* cache = connection->GetCache();
        if (!cache)
            return updated;

        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (!pool)
            return updated;

        const QVariantMap current = pool->GUIConfig();
        const QString uuid = this->objectUuid();

        for (auto it = current.constBegin(); it != current.constEnd(); ++it)
        {
            const QString key = it.key();
            const bool isLayout = key.startsWith(QStringLiteral("XenCenter.GraphLayout."));
            const bool isName = key.startsWith(QStringLiteral("XenCenter.GraphName."));

            if ((isLayout || isName) && key.contains(uuid))
                continue;

            updated.insert(key, it.value());
        }

        for (int i = 0; i < graphsToPersist.size(); ++i)
        {
            const DesignedGraph& graph = graphsToPersist.at(i);
            const QString layoutKey = Palette::GetLayoutKey(i, this->m_xenObject);
            updated.insert(layoutKey, this->serializeGraphLayout(graph));

            if (!graph.DisplayName.isEmpty())
            {
                const QString nameKey = Palette::GetGraphNameKey(i, this->m_xenObject);
                updated.insert(nameKey, graph.DisplayName);
            }
        }

        QList<DataSourceItem> dataSourceItems = this->getGraphsDataSources();
        for (const DataSourceItem& item : dataSourceItems)
        {
            if (!item.ColorChanged)
                continue;

            const QString key = Palette::GetColorKey(item.GetDataSource(), this->m_xenObject);
            const int value = static_cast<int>(item.Color.rgba());
            updated.insert(key, QString::number(value));
        }

        return updated;
    }

    QString GraphList::serializeGraphLayout(const DesignedGraph& graph) const
    {
        QStringList names;
        for (const DataSourceItem& item : graph.DataSourceItems)
            names.append(item.GetDataSource());
        return names.join(',');
    }

    QList<DataSourceItem> GraphList::fetchDataSources() const
    {
        if (!this->m_xenObject)
            return {};

        XenConnection* connection = this->m_xenObject->GetConnection();
        if (!connection)
            return {};

        auto* action = new GetDataSourcesAction(connection,
                                                this->m_xenObject->GetObjectType(),
                                                this->m_xenObject->OpaqueRef(),
                                                nullptr);
        action->RunSync(connection->GetSession());
        const QList<QVariantMap> dataSources = action->DataSources();
        action->deleteLater();
        return DataSourceItemList::BuildList(this->m_xenObject, dataSources);
    }

    void GraphList::updateDataSources(QList<DataSourceItem>& dataSourceItems) const
    {
        for (DataSourceItem& item : dataSourceItems)
        {
            bool found = false;
            for (const DesignedGraph& graph : this->m_graphs)
            {
                found = graph.DataSourceItems.contains(item);
                if (found)
                {
                    if (!Palette::HasCustomColour(item.Id))
                    {
                        item.ColorChanged = true;
                        Palette::SetCustomColor(item.Id, item.Color);
                    }
                    break;
                }
            }

            if (!item.DataSource.Standard)
                item.Enabled = found;
        }
    }

    QList<DataSourceItem> GraphList::getGraphsDataSources() const
    {
        QList<DataSourceItem> dataSourceItems;
        for (const DesignedGraph& graph : this->m_graphs)
        {
            for (DataSourceItem item : graph.DataSourceItems)
            {
                const QString dataSourceName = item.GetDataSource();
                if (dataSourceName == QStringLiteral("memory_total_kib") || dataSourceName == QStringLiteral("memory"))
                    continue;

                if (!Palette::HasCustomColour(item.Id))
                {
                    item.DataSource.NameLabel = dataSourceName;
                    item.ColorChanged = true;
                    Palette::SetCustomColor(item.Id, item.Color);
                    dataSourceItems.append(item);
                }
            }
        }
        return dataSourceItems;
    }

    QList<QVariantMap> GraphList::buildDataSourceStatePayload(const QList<DataSourceItem>& dataSourceItems) const
    {
        QList<QVariantMap> result;
        for (const DataSourceItem& item : dataSourceItems)
        {
            QVariantMap state;
            state.insert(QStringLiteral("name_label"), item.DataSource.NameLabel);
            state.insert(QStringLiteral("current_enabled"), item.DataSource.Enabled);
            state.insert(QStringLiteral("desired_enabled"), item.Enabled);
            result.append(state);
        }
        return result;
    }
}
