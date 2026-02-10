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

#include "dataplot.h"
#include "archivemaintainer.h"
#include "dataplotnav.h"
#include "dataeventlist.h"
#include "datakey.h"
#include "palette.h"
#include "dataset.h"
#include "../../settingsmanager.h"
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QPen>
#include <QPainter>
#include <QSet>
#include <QVBoxLayout>
#include <cmath>

// Qt5/Qt6 compatibility for QtCharts namespace
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

namespace CustomDataGraph
{
    DataPlot::DataPlot(QWidget* parent) : QWidget(parent)
    {
        this->setMinimumHeight(150);
        this->setObjectName(QStringLiteral("DataPlot"));
        this->setAutoFillBackground(false);

        this->m_layout = new QVBoxLayout(this);
        this->m_layout->setContentsMargins(8, 4, 8, 8);
        this->m_layout->setSpacing(4);

        this->m_titleLabel = new QLabel(this);
        this->m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        this->m_layout->addWidget(this->m_titleLabel);

        this->m_chart = new QChart();
        this->m_chart->legend()->hide();
        this->m_chart->setTheme(QChart::ChartThemeLight);
        this->m_chart->setBackgroundVisible(false);
        this->m_chart->setMargins(QMargins(0, 0, 0, 0));
        this->m_chart->setPlotAreaBackgroundVisible(true);
        this->m_chart->setPlotAreaBackgroundBrush(QColor(250, 250, 250));
        this->m_chart->setPlotAreaBackgroundPen(QPen(QColor(190, 190, 190)));

        this->m_axisX = new QDateTimeAxis(this);
        this->m_axisX->setFormat(QStringLiteral("h:mm AP"));
        this->m_axisX->setLabelsColor(QColor(70, 70, 70));
        this->m_axisX->setGridLineColor(QColor(210, 210, 210));
        this->m_axisX->setLinePenColor(QColor(140, 140, 140));
        this->m_axisX->setGridLineVisible(true);

        this->m_axisY = new QValueAxis(this);
        this->m_axisY->setLabelFormat(QStringLiteral("%.2f"));
        this->m_axisY->setLabelsColor(QColor(70, 70, 70));
        this->m_axisY->setGridLineColor(QColor(210, 210, 210));
        this->m_axisY->setLinePenColor(QColor(140, 140, 140));
        this->m_axisY->setGridLineVisible(true);

        this->m_chart->addAxis(this->m_axisX, Qt::AlignBottom);
        this->m_chart->addAxis(this->m_axisY, Qt::AlignRight);

        this->m_chartView = new QChartView(this->m_chart, this);
        this->m_chartView->setRenderHint(QPainter::Antialiasing, true);
        this->m_chartView->setFrameStyle(QFrame::NoFrame);
        this->m_chartView->setRubberBand(QChartView::RectangleRubberBand);
        this->m_chartView->viewport()->installEventFilter(this);
        this->m_layout->addWidget(this->m_chartView, 1);

        this->applySelectionStyle();
    }

    void DataPlot::SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer)
    {
        this->m_archiveMaintainer = archiveMaintainer;
        this->RefreshData();
    }

    void DataPlot::SetDataPlotNav(DataPlotNav* dataPlotNav)
    {
        this->m_dataPlotNav = dataPlotNav;
        this->RefreshData();
    }

    void DataPlot::SetDataEventList(DataEventList* dataEventList)
    {
        this->m_dataEventList = dataEventList;
        Q_UNUSED(this->m_dataEventList);
    }

    void DataPlot::SetDataKey(DataKey* dataKey)
    {
        this->m_dataKey = dataKey;
        Q_UNUSED(this->m_dataKey);
    }

    void DataPlot::SetDisplayName(const QString& displayName)
    {
        this->m_displayName = displayName;
        if (this->m_titleLabel)
            this->m_titleLabel->setText(displayName);
    }

    void DataPlot::SetDataSourceUUIDsToShow(const QList<QString>& dataSourceUuids)
    {
        this->m_dataSourceUUIDs = dataSourceUuids;
        this->syncSeries(this->m_dataSourceUUIDs);
        this->RefreshData();
    }

    QString DataPlot::DisplayName() const
    {
        return this->m_displayName;
    }

    void DataPlot::RefreshData()
    {
        const bool fillAreas = SettingsManager::instance().GetFillAreaUnderGraphs();
        if (fillAreas != this->m_fillAreaUnderGraphs)
        {
            this->m_fillAreaUnderGraphs = fillAreas;

            for (QAreaSeries* areaSeries : this->m_areaSeriesById)
            {
                if (areaSeries)
                {
                    this->m_chart->removeSeries(areaSeries);
                    areaSeries->deleteLater();
                }
            }
            this->m_areaSeriesById.clear();

            for (QLineSeries* lineSeries : this->m_seriesById)
            {
                if (lineSeries)
                {
                    this->m_chart->removeSeries(lineSeries);
                    lineSeries->deleteLater();
                }
            }
            this->m_seriesById.clear();
        }

        this->syncSeries(this->m_dataSourceUUIDs);

        if (!this->m_archiveMaintainer || !this->m_dataPlotNav || this->m_dataSourceUUIDs.isEmpty())
        {
            for (QLineSeries* series : this->m_seriesById)
            {
                if (series)
                    series->clear();
            }
            return;
        }

        const qint64 startMs = this->m_dataPlotNav->XRange.Start.toMSecsSinceEpoch();
        const qint64 endMs = this->m_dataPlotNav->XRange.End.toMSecsSinceEpoch();
        if (endMs <= startMs)
            return;

        const ArchiveInterval interval = this->m_dataPlotNav->GetCurrentArchiveInterval();
        QMap<QString, QVector<QPointF>> pointsById;

        bool hasValue = false;
        double minY = 0.0;
        double maxY = 0.0;

        for (const QString& id : this->m_dataSourceUUIDs)
        {
            const DataSet* set = this->m_archiveMaintainer->TryGetDataSet(id, interval);
            if (!set)
                continue;

            const QVector<DataPoint>& points = set->Points();
            QVector<QPointF> chartPoints;
            chartPoints.reserve(points.size());

            for (int i = points.size() - 1; i >= 0; --i)
            {
                const DataPoint& point = points.at(i);
                if (point.X < startMs || point.X > endMs || !std::isfinite(point.Y))
                    continue;

                chartPoints.append(QPointF(static_cast<qreal>(point.X), point.Y));

                if (!hasValue)
                {
                    minY = point.Y;
                    maxY = point.Y;
                    hasValue = true;
                } else
                {
                    minY = qMin(minY, point.Y);
                    maxY = qMax(maxY, point.Y);
                }
            }

            if (!chartPoints.isEmpty())
            {
                pointsById.insert(id, chartPoints);
            }
        }

        for (const QString& id : this->m_dataSourceUUIDs)
        {
            QLineSeries* series = this->m_seriesById.value(id, nullptr);
            if (!series)
                continue;

            QPen pen(Palette::GetColour(id));
            pen.setWidthF(1.5);
            series->setPen(pen);
            const QColor lineColor = Palette::GetColour(id);

            if (pointsById.contains(id))
                series->replace(pointsById.value(id));
            else
                series->clear();

            QAreaSeries* areaSeries = this->m_areaSeriesById.value(id, nullptr);
            if (areaSeries)
            {
                QColor fillColor = lineColor;
                fillColor.setAlpha(70);
                areaSeries->setBrush(fillColor);
                areaSeries->setPen(pen);
            }
        }

        this->m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(startMs), QDateTime::fromMSecsSinceEpoch(endMs));
        const qint64 spanMs = endMs - startMs;
        if (spanMs <= 10LL * 60LL * 1000LL)
            this->m_axisX->setFormat(QStringLiteral("h:mm:ss AP"));
        else
            this->m_axisX->setFormat(QStringLiteral("h:mm AP"));

        if (!hasValue)
        {
            this->m_axisY->setRange(0.0, 1.0);
        } else
        {
            if (qFuzzyCompare(minY, maxY))
            {
                minY -= 1.0;
                maxY += 1.0;
            }
            this->m_axisY->setRange(minY, maxY);
            this->m_axisY->applyNiceNumbers();
        }
    }

    bool DataPlot::eventFilter(QObject* watched, QEvent* event)
    {
        if (this->m_chartView && watched == this->m_chartView->viewport()
            && event && event->type() == QEvent::MouseButtonPress)
        {
            emit this->Clicked();
        }

        return QObject::eventFilter(watched, event);
    }

    void DataPlot::paintEvent(QPaintEvent* event)
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QColor borderColor = this->m_isSelected ? this->palette().highlight().color() : QColor(165, 165, 165);
        QPen borderPen(borderColor);
        borderPen.setWidth(1);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(this->rect().adjusted(0, 0, -1, -1));
    }

    void DataPlot::syncSeries(const QList<QString>& dataSourceUuids)
    {
        QSet<QString> wanted = QSet<QString>(dataSourceUuids.begin(), dataSourceUuids.end());

        QList<QString> existingArea = this->m_areaSeriesById.keys();
        for (const QString& id : existingArea)
        {
            if (wanted.contains(id))
                continue;

            QAreaSeries* areaSeries = this->m_areaSeriesById.take(id);
            if (areaSeries)
            {
                this->m_chart->removeSeries(areaSeries);
                areaSeries->deleteLater();
            }
        }

        QList<QString> existing = this->m_seriesById.keys();
        for (const QString& id : existing)
        {
            if (wanted.contains(id))
                continue;

            QLineSeries* series = this->m_seriesById.take(id);
            if (series)
            {
                this->m_chart->removeSeries(series);
                series->deleteLater();
            }
        }

        for (const QString& id : dataSourceUuids)
        {
            if (this->m_seriesById.contains(id))
                continue;

            auto* series = new QLineSeries(this);
            QPen pen(Palette::GetColour(id));
            pen.setWidthF(1.5);
            series->setPen(pen);
            if (this->m_fillAreaUnderGraphs)
            {
                auto* areaSeries = new QAreaSeries(series, nullptr);
                areaSeries->setParent(this);
                QColor fillColor = Palette::GetColour(id);
                fillColor.setAlpha(70);
                areaSeries->setBrush(fillColor);
                areaSeries->setPen(pen);
                this->m_chart->addSeries(areaSeries);
                areaSeries->attachAxis(this->m_axisX);
                areaSeries->attachAxis(this->m_axisY);
                this->m_areaSeriesById.insert(id, areaSeries);
            } else
            {
                this->m_chart->addSeries(series);
                series->attachAxis(this->m_axisX);
                series->attachAxis(this->m_axisY);
            }
            this->m_seriesById.insert(id, series);
        }
    }

    void DataPlot::applySelectionStyle()
    {
        this->update();
    }
}
