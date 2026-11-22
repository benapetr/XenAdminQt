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

#include "vmss.h"
#include "network/connection.h"
#include "../xencache.h"
#include "../utils/misc.h"
#include <QRegularExpression>
#include <algorithm>

VMSS::VMSS(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


bool VMSS::Enabled() const
{
    return this->boolProperty("enabled");
}

QString VMSS::Type() const
{
    return this->stringProperty("type");
}

qlonglong VMSS::RetainedSnapshots() const
{
    return this->longProperty("retained_snapshots");
}

QString VMSS::Frequency() const
{
    return this->stringProperty("frequency");
}

QVariantMap VMSS::Schedule() const
{
    return this->property("schedule").toMap();
}

QDateTime VMSS::LastRunTime() const
{
    QVariant timeVariant = this->property("last_run_time");
    if (timeVariant.canConvert<QDateTime>())
        return timeVariant.toDateTime();
    
    // Try parsing as string if not already a QDateTime
    QString timeStr = timeVariant.toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = Misc::ParseXenDateTime(timeStr);
        if (dt.isValid())
            return dt;
    }
    
    // Return epoch time if parsing fails
    return QDateTime::fromSecsSinceEpoch(0);
}

QStringList VMSS::GetVMRefs() const
{
    return this->property("VMs").toStringList();
}

// Helper methods
bool VMSS::IsEnabled() const
{
    return this->Enabled();
}

int VMSS::VMCount() const
{
    return this->GetVMRefs().count();
}

namespace
{
    bool scheduleInt(const QVariantMap& schedule, const QString& key, int* result)
    {
        bool ok = false;
        *result = schedule.value(key).toString().toInt(&ok);
        return schedule.contains(key) && ok;
    }

    QDateTime at(const QDate& date, int hour, int min)
    {
        return QDateTime(date, QTime(hour, min), Qt::UTC);
    }
}

QDateTime VMSS::GetNextRunTime(const QDateTime& serverLocalTime) const
{
    const QVariantMap schedule = this->Schedule();
    int min = 0;
    if (!scheduleInt(schedule, "min", &min))
        return QDateTime();

    const QString frequency = this->Frequency();
    if (frequency == "hourly")
        return GetHourlyDate(serverLocalTime, min);

    int hour = 0;
    if (!scheduleInt(schedule, "hour", &hour))
        return QDateTime();

    if (frequency == "daily")
        return GetDailyDate(serverLocalTime, min, hour);

    if (frequency == "weekly")
    {
        const QList<QDateTime> dates = GetWeeklyDates(serverLocalTime, min, hour, ScheduleDays(schedule));
        if (!dates.isEmpty())
            return dates.first();
    }
    return QDateTime();
}

QDateTime VMSS::GetHourlyDate(const QDateTime& time, int min)
{
    QDateTime next = at(time.date(), time.time().hour(), min);
    if (time > next)
        next = next.addSecs(3600);
    return next;
}

QDateTime VMSS::GetDailyDate(const QDateTime& time, int min, int hour)
{
    QDateTime next = at(time.date(), hour, min);
    if (time > next)
        next = next.addDays(1);
    return next;
}

QList<QDateTime> VMSS::GetWeeklyDates(const QDateTime& time, int min, int hour, const QList<int>& days)
{
    const QDateTime nextRun = at(time.date(), hour, min);
    const int today = nextRun.date().dayOfWeek();
    QList<QDateTime> runs;
    for (int d : days)
    {
        if (today < d)
            runs.append(nextRun.addDays(d - today));
        else if (d < today)
            runs.append(nextRun.addDays(7 - (today - d)));
        else if (time < nextRun)
            runs.append(nextRun);
        else
            runs.append(nextRun.addDays(7));
    }
    std::sort(runs.begin(), runs.end());
    return runs;
}

QList<int> VMSS::ScheduleDays(const QVariantMap& schedule)
{
    static const QStringList names = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
    QList<int> days;
    const QStringList parts = schedule.value("days").toString().split(QRegularExpression("[, ]"), Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        const int index = names.indexOf(part.toLower());
        if (index >= 0)
            days.append(index + 1);  // Qt::Monday == 1
    }
    return days;
}

QDateTime VMSS::RoundToNearestQuarter(const QDateTime& time)
{
    const QDateTime base = at(time.date(), time.time().hour(), 0);
    const qint64 seconds = base.secsTo(time);
    if (seconds < 450)
        return base;
    if (seconds < 1350)
        return base.addSecs(900);
    if (seconds < 2250)
        return base.addSecs(1800);
    if (seconds < 3150)
        return base.addSecs(2700);
    return base.addSecs(3600);
}
