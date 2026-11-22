/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "policysnapshotfrequencypage.h"
#include "ui_policysnapshotfrequencypage.h"
#include "xenlib/xen/vmss.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmssactions.h"
#include "xenlib/xencache.h"
#include <QLocale>
#include <QToolTip>
#include <algorithm>

namespace
{
    const QStringList DAY_NAMES = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};

    // C# DATEFORMAT_HM and DATEFORMAT_WDMY_HM_LONG, in the user's locale
    QString formatTime(const QTime& time)
    {
        return QLocale().toString(time, QLocale::ShortFormat);
    }

    QString formatLong(const QDateTime& time)
    {
        return QLocale().toString(time.date(), QLocale::LongFormat) + " " + formatTime(time.time());
    }
}

PolicySnapshotFrequencyPage::PolicySnapshotFrequencyPage(QWidget* parent) : IEditPage(parent), ui(new Ui::PolicySnapshotFrequencyPage)
{
    this->ui->setupUi(this);
    this->ui->mainPanel->setVisible(false);

    for (QComboBox* combo : {this->ui->comboBoxDailyTime, this->ui->comboBoxWeeklyTime})
    {
        for (int minutes = 0; minutes < 24 * 60; minutes += 15)
        {
            const QTime time(minutes / 60, minutes % 60);
            combo->addItem(formatTime(time), time);
        }
    }

    // C# constructor defaults: daily, 7 snapshots, 15 past the hour, Mondays
    this->m_updating = true;
    this->ui->radioButtonDaily->setChecked(true);
    this->onFrequencyChosen();
    this->setHourlyMinutes(15);
    this->setSelectedDays({Qt::Monday});
    this->m_updating = false;

    for (QRadioButton* radio : {this->ui->radioButtonHourly, this->ui->radioButtonDaily, this->ui->radioButtonWeekly})
    {
        connect(radio, &QRadioButton::toggled, this, [this](bool checked)
        {
            if (checked)
                this->onFrequencyChosen();
        });
    }
    connect(this->ui->comboBoxMin, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PolicySnapshotFrequencyPage::recalculateSchedule);
    connect(this->ui->comboBoxDailyTime, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PolicySnapshotFrequencyPage::recalculateSchedule);
    connect(this->ui->comboBoxWeeklyTime, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PolicySnapshotFrequencyPage::recalculateSchedule);
    connect(this->ui->spinRetention, QOverload<int>::of(&QSpinBox::valueChanged), this, &PolicySnapshotFrequencyPage::recalculateSchedule);
    for (QCheckBox* box : this->dayBoxes())
        connect(box, &QCheckBox::toggled, this, &PolicySnapshotFrequencyPage::recalculateSchedule);
}

PolicySnapshotFrequencyPage::~PolicySnapshotFrequencyPage()
{
    delete this->ui;
}

QList<QCheckBox*> PolicySnapshotFrequencyPage::dayBoxes() const
{
    // Index + 1 is the Qt::DayOfWeek
    return {this->ui->checkBoxMonday, this->ui->checkBoxTuesday, this->ui->checkBoxWednesday, this->ui->checkBoxThursday,
            this->ui->checkBoxFriday, this->ui->checkBoxSaturday, this->ui->checkBoxSunday};
}

QList<int> PolicySnapshotFrequencyPage::selectedDays() const
{
    QList<int> days;
    const QList<QCheckBox*> boxes = this->dayBoxes();
    for (int i = 0; i < boxes.size(); ++i)
    {
        if (boxes.at(i)->isChecked())
            days.append(i + 1);
    }
    return days;
}

void PolicySnapshotFrequencyPage::setSelectedDays(const QList<int>& days)
{
    const QList<QCheckBox*> boxes = this->dayBoxes();
    for (int i = 0; i < boxes.size(); ++i)
        boxes.at(i)->setChecked(days.contains(i + 1));
}

void PolicySnapshotFrequencyPage::setHourlyMinutes(int min)
{
    // C# SetHourlyMinutes
    this->ui->comboBoxMin->setCurrentIndex(qBound(0, min / 15, 3));
}

void PolicySnapshotFrequencyPage::setTime(QComboBox* combo, const QTime& time)
{
    const int index = combo->findData(QTime(time.hour(), (time.minute() / 15) * 15));
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

QTime PolicySnapshotFrequencyPage::timeOf(const QComboBox* combo) const
{
    return combo->currentData().toTime();
}

qint64 PolicySnapshotFrequencyPage::BackupRetention() const
{
    return this->ui->spinRetention->value();
}

bool PolicySnapshotFrequencyPage::IsComplete() const
{
    if (this->ui->radioButtonHourly->isChecked() && this->ui->comboBoxMin->currentIndex() < 0)
        return false;
    if (this->ui->radioButtonWeekly->isChecked() && this->selectedDays().isEmpty())
        return false;
    // Without the server time there's no schedule to save
    return this->m_hasServerTime;
}

void PolicySnapshotFrequencyPage::LoadServerTime(XenConnection* connection)
{
    // C# GetServerTime
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    if (!coordinator)
        return;

    this->ui->mainPanel->setVisible(false);
    this->ui->labelLoading->setVisible(true);

    auto* action = new GetServerLocalTimeAction(coordinator);
    QPointer<PolicySnapshotFrequencyPage> self(this);
    connect(action, &AsyncOperation::completed, action, [self, action]()
    {
        // Runs on the worker thread; hand the result to the page on the GUI thread
        const bool hasTime = action->HasServerTime();
        const QDateTime serverTime = action->ServerLocalTime();
        const qint64 diff = action->ServerClientTimeZoneDiff();
        QMetaObject::invokeMethod(qApp, [self, hasTime, serverTime, diff]()
        {
            if (!self)
                return;
            self->ui->labelLoading->setVisible(false);
            self->ui->mainPanel->setVisible(true);
            self->m_hasServerTime = hasTime;
            self->m_serverLocalTime = serverTime;
            self->m_diffSeconds = diff;
            self->populate();
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    action->RunAsync(true);
}

void PolicySnapshotFrequencyPage::populate()
{
    // C# PopulateTab: show the policy's schedule in the user's time zone
    QSharedPointer<VMSS> policy = qSharedPointerDynamicCast<VMSS>(this->m_object);
    if (policy && this->m_hasServerTime)
    {
        this->m_updating = true;
        const QDateTime nextRunOnServer = policy->GetNextRunTime(this->m_serverLocalTime);
        if (nextRunOnServer.isValid())
        {
            const QDateTime nextRunOnClient = VMSS::RoundToNearestQuarter(nextRunOnServer.addSecs(this->m_diffSeconds));
            const QString frequency = policy->Frequency();
            if (frequency == "hourly")
            {
                this->setHourlyMinutes(nextRunOnClient.time().minute());
                this->ui->radioButtonHourly->setChecked(true);
            } else if (frequency == "daily")
            {
                this->setTime(this->ui->comboBoxDailyTime, nextRunOnClient.time());
                this->ui->radioButtonDaily->setChecked(true);
            } else if (frequency == "weekly")
            {
                this->setTime(this->ui->comboBoxWeeklyTime, nextRunOnClient.time());
                // C# shows the schedule's (server) days as they are
                this->setSelectedDays(VMSS::ScheduleDays(policy->Schedule()));
                this->ui->radioButtonWeekly->setChecked(true);
            }
            this->onFrequencyChosen();
            this->ui->spinRetention->setValue(int(this->m_originalRetention));
        }
        this->m_updating = false;
    }
    this->recalculateSchedule();
}

void PolicySnapshotFrequencyPage::onFrequencyChosen()
{
    // C# radioButton*_CheckedChanged: show that frequency's settings and its default retention
    const bool hourly = this->ui->radioButtonHourly->isChecked();
    const bool daily = this->ui->radioButtonDaily->isChecked();
    this->ui->panelHourly->setVisible(hourly);
    this->ui->panelDaily->setVisible(daily);
    this->ui->panelWeekly->setVisible(!hourly && !daily);
    this->m_frequency = hourly ? "hourly" : (daily ? "daily" : "weekly");

    const bool wasUpdating = this->m_updating;
    this->m_updating = true;
    this->ui->spinRetention->setValue(hourly ? 10 : (daily ? 7 : 4));
    this->m_updating = wasUpdating;
    this->recalculateSchedule();
}

void PolicySnapshotFrequencyPage::recalculateSchedule()
{
    // C# RecalculateSchedule
    if (this->m_updating)
        return;

    this->m_schedule.clear();
    this->m_formattedSchedule.clear();
    this->m_subText.clear();
    QDateTime nextRunOnClient;
    QDateTime nextRunOnServer;
    const QDateTime clientNow = GetServerLocalTimeAction::ClientLocalNow();

    if (this->ui->radioButtonHourly->isChecked())
    {
        const int min = this->ui->comboBoxMin->currentText().toInt();
        this->m_subText = this->m_formattedSchedule = tr("Hourly; at %1 minutes past each hour").arg(min);
        nextRunOnClient = VMSS::GetHourlyDate(clientNow, min);
        if (this->m_hasServerTime)
        {
            nextRunOnServer = VMSS::RoundToNearestQuarter(nextRunOnClient.addSecs(-this->m_diffSeconds));
            this->m_schedule["min"] = QString::number(nextRunOnServer.time().minute());
        }
    } else if (this->ui->radioButtonDaily->isChecked())
    {
        const QTime time = this->timeOf(this->ui->comboBoxDailyTime);
        this->m_subText = this->m_formattedSchedule = tr("Daily; at %1").arg(formatTime(time));
        nextRunOnClient = VMSS::GetDailyDate(clientNow, time.minute(), time.hour());
        if (this->m_hasServerTime)
        {
            nextRunOnServer = VMSS::RoundToNearestQuarter(nextRunOnClient.addSecs(-this->m_diffSeconds));
            this->m_schedule["hour"] = QString::number(nextRunOnServer.time().hour());
            this->m_schedule["min"] = QString::number(nextRunOnServer.time().minute());
        }
    } else if (this->ui->radioButtonWeekly->isChecked() && !this->selectedDays().isEmpty())
    {
        const QTime time = this->timeOf(this->ui->comboBoxWeeklyTime);
        const QList<int> days = this->selectedDays();
        QStringList longNames, shortNames;
        for (int d : days)
        {
            longNames.append(QLocale().dayName(d, QLocale::LongFormat));
            shortNames.append(QLocale().dayName(d, QLocale::ShortFormat));
        }
        this->m_formattedSchedule = tr("Weekly; at %1 every %2").arg(formatTime(time), longNames.join(", "));
        this->m_subText = tr("Weekly; at %1 every %2").arg(formatTime(time), shortNames.join(", "));

        const QList<QDateTime> clientRuns = VMSS::GetWeeklyDates(clientNow, time.minute(), time.hour(), days);
        if (this->m_hasServerTime && !clientRuns.isEmpty())
        {
            nextRunOnClient = clientRuns.first();
            QStringList serverDays;
            for (const QDateTime& run : clientRuns)
            {
                const QDateTime serverRun = VMSS::RoundToNearestQuarter(run.addSecs(-this->m_diffSeconds));
                if (!nextRunOnServer.isValid())
                    nextRunOnServer = serverRun;
                serverDays.append(DAY_NAMES.at(serverRun.date().dayOfWeek() - 1));
            }
            this->m_schedule["hour"] = QString::number(nextRunOnServer.time().hour());
            this->m_schedule["min"] = QString::number(nextRunOnServer.time().minute());
            this->m_schedule["days"] = serverDays.join(",");
        }
    }

    if (this->m_formattedSchedule.isEmpty())
        this->m_formattedSchedule = tr("Unknown");
    if (this->m_subText.isEmpty())
        this->m_subText = tr("Unknown");

    // C# RefreshTime
    const QString localRun = nextRunOnClient.isValid() ? formatLong(nextRunOnClient) : tr("Unknown");
    const QString serverRun = nextRunOnServer.isValid() ? formatLong(nextRunOnServer) : tr("Unknown");
    this->ui->labelClientNextRun->setText(tr("The next scheduled local run time is %1.").arg(localRun));
    this->ui->labelServerNextRun->setText(tr("The corresponding server local time is %1.").arg(serverRun));

    emit this->scheduleChanged();
    emit this->populated();
}

QString PolicySnapshotFrequencyPage::GetText() const
{
    return tr("Snapshot Schedule");
}

QString PolicySnapshotFrequencyPage::GetSubText() const
{
    return this->m_subText;
}

QIcon PolicySnapshotFrequencyPage::GetImage() const
{
    return QIcon(":/icons/notif_events_16.png");
}

void PolicySnapshotFrequencyPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    QSharedPointer<VMSS> policy = qSharedPointerDynamicCast<VMSS>(object);
    if (!policy)
        return;
    this->m_originalFrequency = policy->Frequency();
    this->m_originalSchedule = policy->Schedule();
    this->m_originalRetention = policy->RetainedSnapshots();
    this->LoadServerTime(policy->GetConnection());
}

AsyncOperation* PolicySnapshotFrequencyPage::SaveSettings()
{
    // C#: sets the copy's frequency, schedule and retention; SaveChangesAction applies them
    return nullptr;
}

QVariantMap PolicySnapshotFrequencyPage::GetModifiedObjectData() const
{
    return QVariantMap{{"frequency", this->m_frequency},
                       {"schedule", this->m_schedule},
                       {"retained_snapshots", this->BackupRetention()}};
}

bool PolicySnapshotFrequencyPage::IsValidToSave() const
{
    return this->IsComplete();
}

void PolicySnapshotFrequencyPage::ShowLocalValidationMessages()
{
    QToolTip::showText(this->ui->groupSchedule->mapToGlobal(QPoint(0, 0)), tr("Invalid or incomplete schedule"), this->ui->groupSchedule);
}

void PolicySnapshotFrequencyPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void PolicySnapshotFrequencyPage::Cleanup()
{
}

bool PolicySnapshotFrequencyPage::HasChanged() const
{
    // C# compares frequency, schedule and retention with the policy; schedule values are strings
    if (!this->m_object || !this->m_hasServerTime)
        return false;
    if (this->m_frequency != this->m_originalFrequency || this->BackupRetention() != this->m_originalRetention)
        return true;
    // Compare by meaning: the server may store "Monday" where the page builds "monday"
    const auto number = [](const QVariantMap& schedule, const char* key)
    {
        return schedule.contains(key) ? schedule.value(key).toString().toInt() : -1;
    };
    const auto days = [](const QVariantMap& schedule)
    {
        QList<int> list = VMSS::ScheduleDays(schedule);
        std::sort(list.begin(), list.end());
        return list;
    };
    // Only the keys this frequency uses (xapi may keep others from an earlier frequency)
    if (number(this->m_schedule, "min") != number(this->m_originalSchedule, "min"))
        return true;
    if (this->m_frequency != "hourly" && number(this->m_schedule, "hour") != number(this->m_originalSchedule, "hour"))
        return true;
    return this->m_frequency == "weekly" && days(this->m_schedule) != days(this->m_originalSchedule);
}
