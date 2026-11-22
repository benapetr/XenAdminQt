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

// C# equivalent: xenadmin/XenAdmin/Wizards/NewPolicyWizard/NewPolicySnapshotTypePage.cs

#ifndef POLICYSNAPSHOTTYPEPAGE_H
#define POLICYSNAPSHOTTYPEPAGE_H

#include "ieditpage.h"

namespace Ui
{
    class PolicySnapshotTypePage;
}

/**
 * Chooses what a snapshot schedule takes: disk-only snapshots or disk and memory snapshots.
 * A page of the New Snapshot Schedule wizard and of a schedule's properties.
 *
 * C#'s "Quiesce the VM" checkbox is deliberately left out: quiesced snapshots were removed in
 * XenServer 7.6 and C# only shows it to older servers (see "Intentional differences" in
 * docs/status.md). A schedule that still has the quiesced type keeps it, with a note, until
 * another type is chosen.
 */
class PolicySnapshotTypePage : public IEditPage
{
    Q_OBJECT

    public:
        explicit PolicySnapshotTypePage(QWidget* parent = nullptr);
        ~PolicySnapshotTypePage() override;

        //! The xapi vmss_type: "snapshot", "checkpoint" or (kept from old schedules) "snapshot_with_quiesce"
        QString BackupType() const;
        //! C# BackupTypeToString
        QString BackupTypeToString() const;
        static QString TypeToString(const QString& type);

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

    private slots:
        void onTypeChosen();

    private:
        Ui::PolicySnapshotTypePage* ui;
        QString m_originalType;
        bool m_keepQuiesce = false;  //!< an old quiesced schedule whose type wasn't changed yet
};

#endif // POLICYSNAPSHOTTYPEPAGE_H
