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

// C# equivalent: xenadmin/XenAdmin/Wizards/NewPolicyWizard/NewPolicySnapshotFrequencyPage.cs
// (with DaysWeekCheckboxes.cs)

#ifndef POLICYSNAPSHOTFREQUENCYPAGE_H
#define POLICYSNAPSHOTFREQUENCYPAGE_H

#include "ieditpage.h"
#include <QDateTime>
#include <QPointer>

namespace Ui
{
    class PolicySnapshotFrequencyPage;
}

class QCheckBox;
class QComboBox;

/**
 * When a snapshot schedule runs and how many snapshots it keeps. The user picks times in their
 * own time zone; the schedule xapi stores is in the server's local time, so the page first
 * reads the coordinator's local time and converts both ways (rounded to quarter hours, which
 * is all xapi accepts). A page of the New Snapshot Schedule wizard and of a schedule's properties.
 */
class PolicySnapshotFrequencyPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit PolicySnapshotFrequencyPage(QWidget* parent = nullptr);
        ~PolicySnapshotFrequencyPage() override;

        //! Reads the server time for the pool on \a connection (C# GetServerTime); the wizard
        //! calls this when the page is shown
        void LoadServerTime(XenConnection* connection);

        QString Frequency() const { return this->m_frequency; }
        QVariantMap Schedule() const { return this->m_schedule; }
        qint64 BackupRetention() const;
        //! The schedule in words, for the wizard's summary (C# FormattedSchedule)
        QString FormattedSchedule() const { return this->m_formattedSchedule; }
        //! C# EnableNext: a frequency's settings are complete
        bool IsComplete() const;

        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantMap GetModifiedObjectData() const override;

    signals:
        //! The schedule or its completeness changed (C# OnPageUpdated)
        void scheduleChanged();

    private slots:
        void onFrequencyChosen();
        void recalculateSchedule();

    private:
        void populate();
        void setHourlyMinutes(int min);
        void setTime(QComboBox* combo, const QTime& time);
        QTime timeOf(const QComboBox* combo) const;
        QList<int> selectedDays() const;
        void setSelectedDays(const QList<int>& days);
        QList<QCheckBox*> dayBoxes() const;

        Ui::PolicySnapshotFrequencyPage* ui;
        bool m_updating = false;
        bool m_hasServerTime = false;
        QDateTime m_serverLocalTime;
        qint64 m_diffSeconds = 0;
        QString m_frequency;
        QVariantMap m_schedule;
        QString m_formattedSchedule;
        QString m_subText;
        // The schedule's saved values, for HasChanged (C# compares with the policy copy)
        QString m_originalFrequency;
        QVariantMap m_originalSchedule;
        qint64 m_originalRetention = 0;
};

#endif // POLICYSNAPSHOTFREQUENCYPAGE_H
