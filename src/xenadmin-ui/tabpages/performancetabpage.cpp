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
#include "xenlib.h"
#include "xen/xenapi/xenapi_VM.h"
#include "xen/xenapi/xenapi_Host.h"
#include "xen/session.h"
#include "xen/network/connection.h"
#include <QDateTime>
#include <QVBoxLayout>
#include <QLabel>
#include <QRandomGenerator>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

PerformanceTabPage::PerformanceTabPage(QWidget* parent)
    : BaseTabPage(parent), ui(new Ui::PerformanceTabPage), m_updateTimer(new QTimer(this)), m_cpuChartView(nullptr), m_memoryChartView(nullptr), m_networkChartView(nullptr), m_diskChartView(nullptr), m_cpuChart(nullptr), m_memoryChart(nullptr), m_networkChart(nullptr), m_diskChart(nullptr), m_cpuSeries(nullptr), m_memorySeries(nullptr), m_networkReadSeries(nullptr), m_networkWriteSeries(nullptr), m_diskReadSeries(nullptr), m_diskWriteSeries(nullptr), m_cpuAxisX(nullptr), m_cpuAxisY(nullptr), m_memoryAxisX(nullptr), m_memoryAxisY(nullptr), m_networkAxisX(nullptr), m_networkAxisY(nullptr), m_diskAxisX(nullptr), m_diskAxisY(nullptr), m_maxDataPoints(60) // 60 data points = 1 minute at 1-second intervals
      ,
      m_startTime(QDateTime::currentMSecsSinceEpoch())
{
    this->ui->setupUi(this);

    // Setup update timer (update every second)
    this->m_updateTimer->setInterval(1000);
    connect(this->m_updateTimer, &QTimer::timeout, this, &PerformanceTabPage::updateMetrics);

    // Connect time range selector
    connect(this->ui->timeRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PerformanceTabPage::onTimeRangeChanged);

    this->setupCharts();
}

PerformanceTabPage::~PerformanceTabPage()
{
    this->m_updateTimer->stop();
    delete this->ui;
}

bool PerformanceTabPage::isApplicableForObjectType(const QString& objectType) const
{
    // Performance tab is applicable to VMs and Hosts
    return objectType == "vm" || objectType == "host";
}

void PerformanceTabPage::refreshContent()
{
    if (this->m_objectData.isEmpty())
    {
        this->m_updateTimer->stop();
        this->clearHistory();
        return;
    }

    // Start collecting metrics
    this->clearHistory();
    this->m_startTime = QDateTime::currentMSecsSinceEpoch();
    this->fetchMetrics();
    this->m_updateTimer->start();
}

void PerformanceTabPage::setupCharts()
{
    // CPU Chart
    this->m_cpuSeries = new QLineSeries();
    this->m_cpuSeries->setName("CPU Usage");
    this->m_cpuChart = this->createChart("CPU Usage (%)");
    this->m_cpuChart->addSeries(this->m_cpuSeries);

    this->m_cpuAxisX = new QValueAxis();
    this->m_cpuAxisX->setTitleText("Time (seconds)");
    this->m_cpuAxisX->setLabelFormat("%d");
    this->m_cpuAxisX->setRange(0, 60);

    this->m_cpuAxisY = new QValueAxis();
    this->m_cpuAxisY->setTitleText("CPU %");
    this->m_cpuAxisY->setRange(0, 100);

    this->m_cpuChart->addAxis(this->m_cpuAxisX, Qt::AlignBottom);
    this->m_cpuChart->addAxis(this->m_cpuAxisY, Qt::AlignLeft);
    this->m_cpuSeries->attachAxis(this->m_cpuAxisX);
    this->m_cpuSeries->attachAxis(this->m_cpuAxisY);

    this->m_cpuChartView = new QChartView(this->m_cpuChart);
    this->m_cpuChartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout* cpuLayout = new QVBoxLayout(this->ui->cpuChartWidget);
    cpuLayout->setContentsMargins(0, 0, 0, 0);
    cpuLayout->addWidget(this->m_cpuChartView);

    // Memory Chart
    this->m_memorySeries = new QLineSeries();
    this->m_memorySeries->setName("Memory Usage");
    this->m_memoryChart = this->createChart("Memory Usage (%)");
    this->m_memoryChart->addSeries(this->m_memorySeries);

    this->m_memoryAxisX = new QValueAxis();
    this->m_memoryAxisX->setTitleText("Time (seconds)");
    this->m_memoryAxisX->setLabelFormat("%d");
    this->m_memoryAxisX->setRange(0, 60);

    this->m_memoryAxisY = new QValueAxis();
    this->m_memoryAxisY->setTitleText("Memory %");
    this->m_memoryAxisY->setRange(0, 100);

    this->m_memoryChart->addAxis(this->m_memoryAxisX, Qt::AlignBottom);
    this->m_memoryChart->addAxis(this->m_memoryAxisY, Qt::AlignLeft);
    this->m_memorySeries->attachAxis(this->m_memoryAxisX);
    this->m_memorySeries->attachAxis(this->m_memoryAxisY);

    this->m_memoryChartView = new QChartView(this->m_memoryChart);
    this->m_memoryChartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout* memLayout = new QVBoxLayout(this->ui->memoryChartWidget);
    memLayout->setContentsMargins(0, 0, 0, 0);
    memLayout->addWidget(this->m_memoryChartView);

    // Network Chart
    this->m_networkReadSeries = new QLineSeries();
    this->m_networkReadSeries->setName("Network Read");
    this->m_networkWriteSeries = new QLineSeries();
    this->m_networkWriteSeries->setName("Network Write");

    this->m_networkChart = this->createChart("Network I/O (KB/s)");
    this->m_networkChart->addSeries(this->m_networkReadSeries);
    this->m_networkChart->addSeries(this->m_networkWriteSeries);

    this->m_networkAxisX = new QValueAxis();
    this->m_networkAxisX->setTitleText("Time (seconds)");
    this->m_networkAxisX->setLabelFormat("%d");
    this->m_networkAxisX->setRange(0, 60);

    this->m_networkAxisY = new QValueAxis();
    this->m_networkAxisY->setTitleText("KB/s");
    this->m_networkAxisY->setRange(0, 1000);

    this->m_networkChart->addAxis(this->m_networkAxisX, Qt::AlignBottom);
    this->m_networkChart->addAxis(this->m_networkAxisY, Qt::AlignLeft);
    this->m_networkReadSeries->attachAxis(this->m_networkAxisX);
    this->m_networkReadSeries->attachAxis(this->m_networkAxisY);
    this->m_networkWriteSeries->attachAxis(this->m_networkAxisX);
    this->m_networkWriteSeries->attachAxis(this->m_networkAxisY);

    this->m_networkChartView = new QChartView(this->m_networkChart);
    this->m_networkChartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout* netLayout = new QVBoxLayout(this->ui->networkChartWidget);
    netLayout->setContentsMargins(0, 0, 0, 0);
    netLayout->addWidget(this->m_networkChartView);

    // Disk Chart
    this->m_diskReadSeries = new QLineSeries();
    this->m_diskReadSeries->setName("Disk Read");
    this->m_diskWriteSeries = new QLineSeries();
    this->m_diskWriteSeries->setName("Disk Write");

    this->m_diskChart = this->createChart("Disk I/O (ops/s)");
    this->m_diskChart->addSeries(this->m_diskReadSeries);
    this->m_diskChart->addSeries(this->m_diskWriteSeries);

    this->m_diskAxisX = new QValueAxis();
    this->m_diskAxisX->setTitleText("Time (seconds)");
    this->m_diskAxisX->setLabelFormat("%d");
    this->m_diskAxisX->setRange(0, 60);

    this->m_diskAxisY = new QValueAxis();
    this->m_diskAxisY->setTitleText("ops/s");
    this->m_diskAxisY->setRange(0, 100);

    this->m_diskChart->addAxis(this->m_diskAxisX, Qt::AlignBottom);
    this->m_diskChart->addAxis(this->m_diskAxisY, Qt::AlignLeft);
    this->m_diskReadSeries->attachAxis(this->m_diskAxisX);
    this->m_diskReadSeries->attachAxis(this->m_diskAxisY);
    this->m_diskWriteSeries->attachAxis(this->m_diskAxisX);
    this->m_diskWriteSeries->attachAxis(this->m_diskAxisY);

    this->m_diskChartView = new QChartView(this->m_diskChart);
    this->m_diskChartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout* diskLayout = new QVBoxLayout(this->ui->diskChartWidget);
    diskLayout->setContentsMargins(0, 0, 0, 0);
    diskLayout->addWidget(this->m_diskChartView);
}

QChart* PerformanceTabPage::createChart(const QString& title)
{
    QChart* chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation); // Disable for better performance
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    return chart;
}

void PerformanceTabPage::updateMetrics()
{
    this->fetchMetrics();
    this->updateCpuChart();
    this->updateMemoryChart();
    this->updateNetworkChart();
    this->updateDiskChart();
}

void PerformanceTabPage::fetchMetrics()
{
    if (this->m_objectData.isEmpty() || !this->m_xenLib)
    {
        return;
    }

    // Get the object reference (opaque_ref)
    QString objRef = this->m_objectData.value("OpaqueRef").toString();
    if (objRef.isEmpty())
    {
        return;
    }

    XenAPI::Session* session = this->m_xenLib->getConnection()
        ? this->m_xenLib->getConnection()->GetSession()
        : nullptr;
    if (!session || !session->IsLoggedIn())
    {
        return;
    }

    // For VMs
    if (this->m_objectType == "vm")
    {
        // Query CPU usage data source
        double cpuUsage = XenAPI::VM::query_data_source(session, objRef, "cpu");
        if (cpuUsage >= 0.0)
        {
            this->addDataPoint("cpu", cpuUsage);
        }

        // Query memory usage - try memory_internal_free first (guest agent provides this)
        double memoryInternalFree = XenAPI::VM::query_data_source(session, objRef, "memory_internal_free");
        if (memoryInternalFree >= 0.0)
        {
            // memory_internal_free is in KB, convert to percentage
            // We need memory_target to calculate percentage
            qint64 memoryTarget = this->m_objectData.value("memory_target", 0).toLongLong();
            if (memoryTarget > 0)
            {
                // memoryInternalFree is in KB, memoryTarget is in bytes
                double memoryUsedKB = (memoryTarget / 1024.0) - memoryInternalFree;
                double memUsagePercent = (memoryUsedKB * 100.0) / (memoryTarget / 1024.0);
                this->addDataPoint("memory", qMax(0.0, qMin(100.0, memUsagePercent)));
            }
        } else
        {
            // Fallback: use static memory values from object data
            qint64 memoryActual = this->m_objectData.value("memory_actual", 0).toLongLong();
            qint64 memoryTarget = this->m_objectData.value("memory_target", 1).toLongLong();
            if (memoryTarget > 0)
            {
                qreal memUsagePercent = (memoryActual * 100.0) / memoryTarget;
                this->addDataPoint("memory", memUsagePercent);
            }
        }

        // Query network I/O - sum of all network interfaces
        // XenServer provides per-VIF data sources like "vif_0_rx", "vif_0_tx"
        // For simplicity, query generic network metrics if available
        double networkRead = XenAPI::VM::query_data_source(session, objRef, "vif_0_rx");
        double networkWrite = XenAPI::VM::query_data_source(session, objRef, "vif_0_tx");
        if (networkRead >= 0.0)
        {
            this->addDataPoint("network_read", networkRead / 1024.0); // Convert to KB/s
        }
        if (networkWrite >= 0.0)
        {
            this->addDataPoint("network_write", networkWrite / 1024.0);
        }

        // Query disk I/O operations
        double diskRead = XenAPI::VM::query_data_source(session, objRef, "vbd_0_read");
        double diskWrite = XenAPI::VM::query_data_source(session, objRef, "vbd_0_write");
        if (diskRead >= 0.0)
        {
            this->addDataPoint("disk_read", diskRead);
        }
        if (diskWrite >= 0.0)
        {
            this->addDataPoint("disk_write", diskWrite);
        }
    }
    // For Hosts
    else if (this->m_objectType == "host")
    {
        // Query host CPU usage
        double cpuUsage = XenAPI::Host::query_data_source(session, objRef, "cpu_avg");
        if (cpuUsage >= 0.0)
        {
            this->addDataPoint("cpu", cpuUsage * 100.0); // Convert to percentage
        }

        // Query host memory usage
        double memoryFree = XenAPI::Host::query_data_source(session, objRef, "memory_free_kib");
        if (memoryFree >= 0.0)
        {
            qint64 memoryTotal = this->m_objectData.value("memory_total", 1).toLongLong();
            if (memoryTotal > 0)
            {
                double memoryTotalKB = memoryTotal / 1024.0;
                double memoryUsedKB = memoryTotalKB - memoryFree;
                double memUsagePercent = (memoryUsedKB * 100.0) / memoryTotalKB;
                this->addDataPoint("memory", qMax(0.0, qMin(100.0, memUsagePercent)));
            }
        }

        // Query host network I/O (aggregate of all PIFs)
        double networkRead = XenAPI::Host::query_data_source(session, objRef, "pif_eth0_rx");
        double networkWrite = XenAPI::Host::query_data_source(session, objRef, "pif_eth0_tx");
        if (networkRead >= 0.0)
        {
            this->addDataPoint("network_read", networkRead / 1024.0); // Convert to KB/s
        }
        if (networkWrite >= 0.0)
        {
            this->addDataPoint("network_write", networkWrite / 1024.0);
        }

        // Query host disk I/O
        double diskRead = XenAPI::Host::query_data_source(session, objRef, "io_throughput_read");
        double diskWrite = XenAPI::Host::query_data_source(session, objRef, "io_throughput_write");
        if (diskRead >= 0.0)
        {
            this->addDataPoint("disk_read", diskRead);
        }
        if (diskWrite >= 0.0)
        {
            this->addDataPoint("disk_write", diskWrite);
        }
    }

    // Update current stats labels
    QList<qreal> cpuData = this->m_metricsHistory.value("cpu");
    if (!cpuData.isEmpty())
    {
        this->ui->cpuCurrentLabel->setText(QString("%1%").arg(cpuData.last(), 0, 'f', 1));
    }

    QList<qreal> memData = this->m_metricsHistory.value("memory");
    if (!memData.isEmpty())
    {
        this->ui->memoryCurrentLabel->setText(QString("%1%").arg(memData.last(), 0, 'f', 1));
    }
}

void PerformanceTabPage::addDataPoint(const QString& metric, qreal value)
{
    QList<qreal>& history = this->m_metricsHistory[metric];
    history.append(value);

    // Keep only the last N data points
    while (history.size() > this->m_maxDataPoints)
    {
        history.removeFirst();
    }
}

void PerformanceTabPage::updateCpuChart()
{
    QList<qreal> cpuData = this->m_metricsHistory.value("cpu");
    if (cpuData.isEmpty())
        return;

    QVector<QPointF> points;
    for (int i = 0; i < cpuData.size(); ++i)
    {
        points.append(QPointF(i, cpuData[i]));
    }

    this->m_cpuSeries->replace(points);

    // Update X-axis range
    if (cpuData.size() > 1)
    {
        this->m_cpuAxisX->setRange(0, cpuData.size() - 1);
    }
}

void PerformanceTabPage::updateMemoryChart()
{
    QList<qreal> memData = this->m_metricsHistory.value("memory");
    if (memData.isEmpty())
        return;

    QVector<QPointF> points;
    for (int i = 0; i < memData.size(); ++i)
    {
        points.append(QPointF(i, memData[i]));
    }

    this->m_memorySeries->replace(points);

    // Update X-axis range
    if (memData.size() > 1)
    {
        this->m_memoryAxisX->setRange(0, memData.size() - 1);
    }
}

void PerformanceTabPage::updateNetworkChart()
{
    QList<qreal> readData = this->m_metricsHistory.value("network_read");
    QList<qreal> writeData = this->m_metricsHistory.value("network_write");

    if (!readData.isEmpty())
    {
        QVector<QPointF> readPoints;
        for (int i = 0; i < readData.size(); ++i)
        {
            readPoints.append(QPointF(i, readData[i]));
        }
        this->m_networkReadSeries->replace(readPoints);

        if (readData.size() > 1)
        {
            this->m_networkAxisX->setRange(0, readData.size() - 1);
        }
    }

    if (!writeData.isEmpty())
    {
        QVector<QPointF> writePoints;
        for (int i = 0; i < writeData.size(); ++i)
        {
            writePoints.append(QPointF(i, writeData[i]));
        }
        this->m_networkWriteSeries->replace(writePoints);
    }

    // Auto-adjust Y-axis based on max value
    qreal maxVal = 0;
    for (qreal val : readData)
        maxVal = qMax(maxVal, val);
    for (qreal val : writeData)
        maxVal = qMax(maxVal, val);
    if (maxVal > 0)
    {
        this->m_networkAxisY->setRange(0, maxVal * 1.2); // 20% padding
    }
}

void PerformanceTabPage::updateDiskChart()
{
    QList<qreal> readData = this->m_metricsHistory.value("disk_read");
    QList<qreal> writeData = this->m_metricsHistory.value("disk_write");

    if (!readData.isEmpty())
    {
        QVector<QPointF> readPoints;
        for (int i = 0; i < readData.size(); ++i)
        {
            readPoints.append(QPointF(i, readData[i]));
        }
        this->m_diskReadSeries->replace(readPoints);

        if (readData.size() > 1)
        {
            this->m_diskAxisX->setRange(0, readData.size() - 1);
        }
    }

    if (!writeData.isEmpty())
    {
        QVector<QPointF> writePoints;
        for (int i = 0; i < writeData.size(); ++i)
        {
            writePoints.append(QPointF(i, writeData[i]));
        }
        this->m_diskWriteSeries->replace(writePoints);
    }

    // Auto-adjust Y-axis based on max value
    qreal maxVal = 0;
    for (qreal val : readData)
        maxVal = qMax(maxVal, val);
    for (qreal val : writeData)
        maxVal = qMax(maxVal, val);
    if (maxVal > 0)
    {
        this->m_diskAxisY->setRange(0, maxVal * 1.2); // 20% padding
    }
}

void PerformanceTabPage::onTimeRangeChanged(int index)
{
    // Adjust max data points based on time range
    switch (index)
    {
    case 0: // 1 minute
        this->m_maxDataPoints = 60;
        break;
    case 1: // 5 minutes
        this->m_maxDataPoints = 300;
        break;
    case 2: // 10 minutes
        this->m_maxDataPoints = 600;
        break;
    case 3: // 1 hour
        this->m_maxDataPoints = 3600;
        break;
    default:
        this->m_maxDataPoints = 60;
    }

    this->clearHistory();
}

void PerformanceTabPage::clearHistory()
{
    this->m_metricsHistory.clear();

    // Clear all chart series
    if (this->m_cpuSeries)
        this->m_cpuSeries->clear();
    if (this->m_memorySeries)
        this->m_memorySeries->clear();
    if (this->m_networkReadSeries)
        this->m_networkReadSeries->clear();
    if (this->m_networkWriteSeries)
        this->m_networkWriteSeries->clear();
    if (this->m_diskReadSeries)
        this->m_diskReadSeries->clear();
    if (this->m_diskWriteSeries)
        this->m_diskWriteSeries->clear();

    // Reset axis ranges to defaults
    if (this->m_cpuAxisX)
        this->m_cpuAxisX->setRange(0, 60);
    if (this->m_memoryAxisX)
        this->m_memoryAxisX->setRange(0, 60);
    if (this->m_networkAxisX)
        this->m_networkAxisX->setRange(0, 60);
    if (this->m_diskAxisX)
        this->m_diskAxisX->setRange(0, 60);
}
