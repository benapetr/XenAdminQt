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

#include "dataplotnav.h"
#include "archivemaintainer.h"
#include "dataeventlist.h"
#include <QDateTime>

namespace CustomDataGraph
{
    DataPlotNav::DataPlotNav(QWidget* parent) : QWidget(parent)
    {
        this->m_tickTimer.setInterval(1000);
        connect(&this->m_tickTimer, &QTimer::timeout, this, &DataPlotNav::onTick);
        this->m_tickTimer.start();

        this->RefreshXRange(false);
    }

    void DataPlotNav::SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer)
    {
        this->m_archiveMaintainer = archiveMaintainer;
    }

    void DataPlotNav::SetDataEventList(DataEventList* dataEventList)
    {
        this->m_dataEventList = dataEventList;
    }

    void DataPlotNav::SetDisplayedUuids(const QList<QString>& uuids)
    {
        this->m_displayedUuids = uuids;
    }

    void DataPlotNav::RefreshXRange(bool fromTick)
    {
        if (this->m_skipTick && fromTick)
        {
            this->m_skipTick = false;
            return;
        }

        if (!fromTick)
            this->m_skipTick = true;

        const QDateTime now = this->m_archiveMaintainer ? this->m_archiveMaintainer->GraphNow() : QDateTime::currentDateTime();
        const QDateTime right = now.addMSecs(-this->m_graphOffsetMs);
        const QDateTime left = right.addMSecs(-this->m_graphWidthMs);

        this->m_gridSpacingMs = this->defaultGridSpacingForWidth(this->m_graphWidthMs);
        this->XRange = DataTimeRange(left, right, -this->m_gridSpacingMs);

        emit this->RangeChanged();
        this->update();
    }

    void DataPlotNav::ZoomToRange(qint64 offsetMs, qint64 widthMs)
    {
        this->m_graphOffsetMs = qMax<qint64>(0, offsetMs);
        this->m_graphWidthMs = qMax<qint64>(1000, widthMs);
        this->RefreshXRange(false);
    }

    void DataPlotNav::ZoomLastTenMinutes()
    {
        this->ZoomToRange(0, 10LL * 60LL * 1000LL - 1000LL);
    }

    void DataPlotNav::ZoomLastHour()
    {
        this->ZoomToRange(0, 60LL * 60LL * 1000LL - 1000LL);
    }

    void DataPlotNav::ZoomLastDay()
    {
        this->ZoomToRange(0, 24LL * 60LL * 60LL * 1000LL - 1000LL);
    }

    void DataPlotNav::ZoomLastWeek()
    {
        this->ZoomToRange(0, 7LL * 24LL * 60LL * 60LL * 1000LL - 1000LL);
    }

    void DataPlotNav::ZoomLastMonth()
    {
        this->ZoomToRange(0, 30LL * 24LL * 60LL * 60LL * 1000LL - 1000LL);
    }

    void DataPlotNav::ZoomLastYear()
    {
        this->ZoomToRange(0, 366LL * 24LL * 60LL * 60LL * 1000LL - 1000LL);
    }

    ArchiveInterval DataPlotNav::GetCurrentArchiveInterval() const
    {
        if (this->m_graphWidthMs <= 10LL * 60LL * 1000LL)
            return ArchiveInterval::FiveSecond;
        if (this->m_graphWidthMs <= 2LL * 60LL * 60LL * 1000LL)
            return ArchiveInterval::OneMinute;
        if (this->m_graphWidthMs <= 7LL * 24LL * 60LL * 60LL * 1000LL)
            return ArchiveInterval::OneHour;
        return ArchiveInterval::OneDay;
    }

    qint64 DataPlotNav::defaultGridSpacingForWidth(qint64 widthMs) const
    {
        if (widthMs < 60LL * 1000LL)
            return 10LL * 1000LL;
        if (widthMs < 10LL * 60LL * 1000LL)
            return 60LL * 1000LL;
        if (widthMs < 60LL * 60LL * 1000LL)
            return 5LL * 60LL * 1000LL;
        if (widthMs < 24LL * 60LL * 60LL * 1000LL)
            return 60LL * 60LL * 1000LL;
        return 24LL * 60LL * 60LL * 1000LL;
    }

    void DataPlotNav::onTick()
    {
        if (!this->isVisible())
            return;

        this->RefreshXRange(true);
    }
}
