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

#ifndef CUSTOMDATAGRAPH_DATAPLOTNAV_H
#define CUSTOMDATAGRAPH_DATAPLOTNAV_H

#include "datatimerange.h"
#include "archiveinterval.h"
#include <QWidget>
#include <QTimer>
#include <QPointer>

namespace CustomDataGraph
{
    class ArchiveMaintainer;
    class DataEventList;

    class DataPlotNav : public QWidget
    {
        Q_OBJECT

        public:
            explicit DataPlotNav(QWidget* parent = nullptr);

            void SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer);
            void SetDataEventList(DataEventList* dataEventList);
            void SetDisplayedUuids(const QList<QString>& uuids);

            void RefreshXRange(bool fromTick);
            void ZoomToRange(qint64 offsetMs, qint64 widthMs);
            void ZoomLastTenMinutes();
            void ZoomLastHour();
            void ZoomLastDay();
            void ZoomLastWeek();
            void ZoomLastMonth();
            void ZoomLastYear();

            ArchiveInterval GetCurrentArchiveInterval() const;

            DataTimeRange XRange;

        signals:
            void RangeChanged();

        private slots:
            void onTick();

        private:
            QPointer<ArchiveMaintainer> m_archiveMaintainer;
            QPointer<DataEventList> m_dataEventList;
            QList<QString> m_displayedUuids;
            QTimer m_tickTimer;
            bool m_skipTick = false;
            qint64 m_graphOffsetMs = 0;
            qint64 m_graphWidthMs = 10 * 60 * 1000 - 1000;
            qint64 m_gridSpacingMs = 2 * 60 * 1000;

            qint64 defaultGridSpacingForWidth(qint64 widthMs) const;
    };
}

#endif // CUSTOMDATAGRAPH_DATAPLOTNAV_H
