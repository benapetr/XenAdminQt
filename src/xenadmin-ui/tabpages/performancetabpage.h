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
#include <QTimer>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class PerformanceTabPage;
}
QT_END_NAMESPACE

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// Qt5 requires actual includes for QtCharts classes, not forward declarations
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE
#else
QT_FORWARD_DECLARE_CLASS(QChartView)
QT_FORWARD_DECLARE_CLASS(QLineSeries)
QT_FORWARD_DECLARE_CLASS(QChart)
QT_FORWARD_DECLARE_CLASS(QValueAxis)
#endif

class PerformanceTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit PerformanceTabPage(QWidget* parent = nullptr);
        ~PerformanceTabPage();

        QString GetTitle() const override
        {
            return "Performance";
        }
        bool IsApplicableForObjectType(const QString& objectType) const override;

    protected:
        void refreshContent() override;

    private slots:
        void updateMetrics();
        void onTimeRangeChanged(int index);

    private:
        Ui::PerformanceTabPage* ui;
        QTimer* m_updateTimer;

        // Chart components
        QChartView* m_cpuChartView;
        QChartView* m_memoryChartView;
        QChartView* m_networkChartView;
        QChartView* m_diskChartView;

        QChart* m_cpuChart;
        QChart* m_memoryChart;
        QChart* m_networkChart;
        QChart* m_diskChart;

        QLineSeries* m_cpuSeries;
        QLineSeries* m_memorySeries;
        QLineSeries* m_networkReadSeries;
        QLineSeries* m_networkWriteSeries;
        QLineSeries* m_diskReadSeries;
        QLineSeries* m_diskWriteSeries;

        QValueAxis* m_cpuAxisX;
        QValueAxis* m_cpuAxisY;
        QValueAxis* m_memoryAxisX;
        QValueAxis* m_memoryAxisY;
        QValueAxis* m_networkAxisX;
        QValueAxis* m_networkAxisY;
        QValueAxis* m_diskAxisX;
        QValueAxis* m_diskAxisY;

        // Chart data
        QMap<QString, QList<qreal>> m_metricsHistory;
        int m_maxDataPoints;
        qint64 m_startTime;

        void setupCharts();
        void updateCpuChart();
        void updateMemoryChart();
        void updateNetworkChart();
        void updateDiskChart();
        void fetchMetrics();
        void clearHistory();

        QChart* createChart(const QString& title);
        void updateChart(QChart* chart, QLineSeries* series, const QList<qreal>& data,
                         QValueAxis* axisX, QValueAxis* axisY, const QString& yLabel);

        // Helper to add data point
        void addDataPoint(const QString& metric, qreal value);
};

#endif // PERFORMANCETABPAGE_H
