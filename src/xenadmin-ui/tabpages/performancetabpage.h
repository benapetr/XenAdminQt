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

#ifndef PERFORMANCETABPAGE_H
#define PERFORMANCETABPAGE_H

#include "basetabpage.h"
#include "controls/customdatagraph/graphhelpers.h"
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class PerformanceTabPage;
}
class QMenu;
QT_END_NAMESPACE

namespace CustomDataGraph
{
    class GraphList;
    class DataPlotNav;
    class DataEventList;
    class ArchiveMaintainer;
}

/**
 * Performance tab orchestration:
 * - refreshContent() is lightweight and marks data as needing re-init.
 * - Heavy startup (datasource fetch + archive maintainer start) is deferred to OnPageShown().
 * - Datasources load asynchronously via GetDataSourcesAction; callbacks are token-guarded to ignore stale completions.
 * - ArchiveMaintainer runs metric fetch/parse in worker-thread flow and notifies UI with ArchivesUpdated.
 * - OnPageHidden()/removeObject() stop/cancel in-flight work and detach state to avoid stale-pointer usage.
 */
class PerformanceTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit PerformanceTabPage(QWidget* parent = nullptr);
        ~PerformanceTabPage() override;

        QString GetTitle() const override { return "Performance"; }
        Type GetType() const override { return Type::Performance; }
        QString HelpID() const override { return "TabPagePerformance"; }
        bool IsApplicableForObjectType(const QString& objectType) const override;

        void OnPageShown() override;
        void OnPageHidden() override;

    protected:
        void refreshContent() override;
        void removeObject() override;

    private slots:
        void onGraphActionsClicked();
        void onZoomClicked();
        void onMoveUpClicked();
        void onMoveDownClicked();
        void onGraphSelectionChanged();
        void onArchivesUpdated();
        void onConnectionMessageReceived(const QString& messageRef, const QVariantMap& messageData);
        void onConnectionMessageRemoved(const QString& messageRef);

    private:
        Ui::PerformanceTabPage* ui;

        CustomDataGraph::GraphList* m_graphList;
        CustomDataGraph::DataPlotNav* m_dataPlotNav;
        CustomDataGraph::DataEventList* m_dataEventList;
        CustomDataGraph::ArchiveMaintainer* m_archiveMaintainer;
        QPointer<class GetDataSourcesAction> m_getDataSourcesAction;
        // Monotonic generation counter for async datasource loads; callbacks only apply if their token matches current state.
        quint64 m_dataSourcesLoadToken = 0;

        QMenu* m_graphActionsMenu;
        QMenu* m_zoomMenu;
        QList<CustomDataGraph::DataSourceItem> m_availableDataSources;

        QList<CustomDataGraph::DataSourceItem> buildAvailableDataSources() const;
        void loadDataSources();
        bool showGraphDetailsDialog(CustomDataGraph::DesignedGraph& graph, bool editMode);
        void updateButtons();
        void loadEvents();
        void checkMessageForGraphs(const QVariantMap& messageData, bool add);
        void disconnectConnectionSignals();
        void connectConnectionSignals();
        void initializeVisibleContent();

        bool m_pageVisible = false;
        bool m_needsVisibleInitialization = false;
        QString m_loadedGraphsObjectRef;
        XenObjectType m_loadedGraphsObjectType = XenObjectType::Null;
};

#endif // PERFORMANCETABPAGE_H
