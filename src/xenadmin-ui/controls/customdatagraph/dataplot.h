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

#ifndef CUSTOMDATAGRAPH_DATAPLOT_H
#define CUSTOMDATAGRAPH_DATAPLOT_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QMap>
#include <QPointer>

class QLabel;
class QVBoxLayout;
class QEvent;

// Qt5/Qt6 compatibility for QtCharts
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: Classes are in QtCharts namespace
    namespace QtCharts
    {
        class QChartView;
        class QChart;
        class QDateTimeAxis;
        class QValueAxis;
        class QLineSeries;
        class QAreaSeries;
    }

    // Use QtCharts namespace for Qt5
    using QtCharts::QChartView;
    using QtCharts::QChart;
    using QtCharts::QDateTimeAxis;
    using QtCharts::QValueAxis;
    using QtCharts::QLineSeries;
    using QtCharts::QAreaSeries;
#else
    // Qt6: Classes are in global namespace
    class QChartView;
    class QChart;
    class QDateTimeAxis;
    class QValueAxis;
    class QLineSeries;
    class QAreaSeries;
#endif

namespace CustomDataGraph
{
    class ArchiveMaintainer;
    class DataPlotNav;
    class DataEventList;
    class DataKey;

    class DataPlot : public QWidget
    {
        Q_OBJECT

        public:
            explicit DataPlot(QWidget* parent = nullptr);

            void SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer);
            void SetDataPlotNav(DataPlotNav* dataPlotNav);
            void SetDataEventList(DataEventList* dataEventList);
            void SetDataKey(DataKey* dataKey);
            void SetDisplayName(const QString& displayName);
            void SetDataSourceUUIDsToShow(const QList<QString>& dataSourceUuids);

            QString DisplayName() const;
            void RefreshData();

        signals:
            void Clicked();

        protected:
            bool eventFilter(QObject* watched, QEvent* event) override;
            void paintEvent(QPaintEvent* event) override;

        private:
            QLabel* m_titleLabel = nullptr;
            QVBoxLayout* m_layout = nullptr;
            QChartView* m_chartView = nullptr;
            QChart* m_chart = nullptr;
            QDateTimeAxis* m_axisX = nullptr;
            QValueAxis* m_axisY = nullptr;
            QPointer<ArchiveMaintainer> m_archiveMaintainer;
            QPointer<DataPlotNav> m_dataPlotNav;
            QPointer<DataEventList> m_dataEventList;
            QPointer<DataKey> m_dataKey;
            QString m_displayName;
            bool m_isSelected = false;
            QList<QString> m_dataSourceUUIDs;
            QMap<QString, QLineSeries*> m_seriesById;
            QMap<QString, QAreaSeries*> m_areaSeriesById;
            bool m_fillAreaUnderGraphs = false;

            void syncSeries(const QList<QString>& dataSourceUuids);
            void applySelectionStyle();

        public:
            void SetIsSelected(bool selected)
            {
                if (this->m_isSelected == selected)
                    return;

                this->m_isSelected = selected;
                this->applySelectionStyle();
            }
    };
}

#endif // CUSTOMDATAGRAPH_DATAPLOT_H
