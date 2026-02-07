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

#ifndef CUSTOMDATAGRAPH_ARCHIVEMAINTAINER_H
#define CUSTOMDATAGRAPH_ARCHIVEMAINTAINER_H

#include "archiveinterval.h"
#include "dataarchive.h"
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QUrl>
#include <QThread>

class XenObject;
class XenConnection;

namespace CustomDataGraph
{
    class ArchiveMaintainer : public QObject
    {
        Q_OBJECT

        public:
            explicit ArchiveMaintainer(XenObject* xenObject, QObject* parent = nullptr);
            ~ArchiveMaintainer() override;

            void Start();
            void Stop();

            void SetDataSourceIds(const QStringList& dataSourceIds);
            QStringList DataSourceIds() const;

            QDateTime GraphNow() const;
            XenObject* GetXenObject() const;

            DataArchive& Archive(ArchiveInterval interval);
            DataArchive Archive(ArchiveInterval interval) const;
            DataSet GetDataSet(const QString& dataSourceId, ArchiveInterval interval) const;
            const DataSet* TryGetDataSet(const QString& dataSourceId, ArchiveInterval interval) const;

            static constexpr qint64 TICKS_IN_ONE_SECOND = 10000000LL;
            static constexpr qint64 TICKS_IN_FIVE_SECONDS = 50000000LL;
            static constexpr qint64 TICKS_IN_ONE_MINUTE = 600000000LL;
            static constexpr qint64 TICKS_IN_TEN_MINUTES = 6000000000LL;
            static constexpr qint64 TICKS_IN_ONE_HOUR = 36000000000LL;
            static constexpr qint64 TICKS_IN_TWO_HOURS = 72000000000LL;
            static constexpr qint64 TICKS_IN_ONE_DAY = 864000000000LL;
            static constexpr qint64 TICKS_IN_SEVEN_DAYS = 6048000000000LL;
            static constexpr qint64 TICKS_IN_ONE_YEAR = 316224000000000LL;

            static constexpr int FIVE_SECONDS_IN_TEN_MINUTES = 120;
            static constexpr int MINUTES_IN_TWO_HOURS = 120;
            static constexpr int HOURS_IN_ONE_WEEK = 168;
            static constexpr int DAYS_IN_ONE_YEAR = 366;

        signals:
            void ArchivesUpdated();

        private slots:
            void onSampleTick();

        private:
            XenObject* m_xenObject = nullptr;
            XenConnection* m_connection = nullptr;
            QMap<ArchiveInterval, DataArchive> m_archives;
            QTimer* m_timer = nullptr;
            QThread* m_workerThread = nullptr;
            QObject* m_workerContext = nullptr;
            bool m_running = false;
            bool m_initialLoadInProgress = false;
            bool m_updateInProgress = false;
            bool m_initialLoadCompleted = false;
            quint64 m_requestToken = 0;
            QStringList m_dataSourceIds;
            QDateTime m_lastFiveSecondCollection;
            QDateTime m_lastOneMinuteCollection;
            QDateTime m_lastOneHourCollection;
            QDateTime m_lastOneDayCollection;

            QDateTime serverNow() const;
            qint64 timeFromInterval(ArchiveInterval interval) const;
            QUrl buildUpdateUri(ArchiveInterval interval) const;
            QUrl buildRrdsUri() const;
            QString resolveRequestHostAddress() const;
            void ensureWorkerThread();
            void shutdownWorkerThread();
            void initialLoad();
            void collectUpdates();
            void appendPoint(DataArchive& archive, const QString& dataSourceId, qint64 timestampMs, double value);
    };
}

#endif // CUSTOMDATAGRAPH_ARCHIVEMAINTAINER_H
