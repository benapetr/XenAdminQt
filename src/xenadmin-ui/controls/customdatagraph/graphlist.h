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

#ifndef CUSTOMDATAGRAPH_GRAPHLIST_H
#define CUSTOMDATAGRAPH_GRAPHLIST_H

#include "graphhelpers.h"
#include <QWidget>
#include <QMetaObject>
#include <QPointer>

class XenObject;

namespace CustomDataGraph
{
    class ArchiveMaintainer;
    class DataPlotNav;
    class DataEventList;
    class DataPlot;
    class DataKey;

    class GraphList : public QWidget
    {
        Q_OBJECT

        public:
            explicit GraphList(QWidget* parent = nullptr);

            void SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer);
            void SetDataPlotNav(DataPlotNav* dataPlotNav);
            void SetDataEventList(DataEventList* dataEventList);

            void SetGraphs(const QList<DesignedGraph>& items);
            void LoadGraphs(XenObject* xmo);
            void SaveGraphs(const QList<DataSourceItem>& dataSourceItems = QList<DataSourceItem>());
            void RestoreDefaultGraphs();
            void ExchangeGraphs(int index1, int index2);
            void DeleteGraph(const DesignedGraph& graph);
            void AddGraph(const DesignedGraph& graph);
            void ReplaceGraphAt(int index, const DesignedGraph& graph);

            int Count() const;
            int SelectedGraphIndex() const;
            DesignedGraph SelectedGraph() const;
            void SetSelectedGraph(const DesignedGraph& graph);

            bool ShowingDefaultGraphs() const;
            QList<QString> DisplayedUuids() const;
            QList<QString> DisplayNames() const;
            QList<DataSourceItem> AllDataSourceItems() const;
            void RefreshGraphs();

        signals:
            void SelectedGraphChanged();
            void GraphsChanged();

        private:
            QPointer<ArchiveMaintainer> m_archiveMaintainer;
            QPointer<DataPlotNav> m_dataPlotNav;
            QPointer<DataEventList> m_dataEventList;

            XenObject* m_xenObject = nullptr;
            QList<DesignedGraph> m_graphs;
            QList<DataPlot*> m_plots;
            QList<DataKey*> m_keys;
            bool m_showingDefaultGraphs = true;
            int m_selectedGraphIndex = -1;
            QList<QString> m_displayedUuids;

            void rebuildUi();
            QList<DesignedGraph> defaultGraphsFor(XenObject* xmo) const;
            void updateDisplayedUuids();
            QString objectTypeToken() const;
            QString objectUuid() const;
            QList<DesignedGraph> loadGraphsFromGuiConfig() const;
            QVariantMap buildUpdatedGuiConfig(const QList<DesignedGraph>& graphsToPersist) const;
            QString serializeGraphLayout(const DesignedGraph& graph) const;
            QList<DataSourceItem> fetchDataSources() const;
            void updateDataSources(QList<DataSourceItem>& dataSourceItems) const;
            QList<DataSourceItem> getGraphsDataSources() const;
            QList<QVariantMap> buildDataSourceStatePayload(const QList<DataSourceItem>& dataSourceItems) const;
            void applySelectionState();

            QMetaObject::Connection m_plotNavRangeChangedConnection;
    };
}

#endif // CUSTOMDATAGRAPH_GRAPHLIST_H
