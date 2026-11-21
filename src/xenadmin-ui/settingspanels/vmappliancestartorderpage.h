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

// C# equivalent: xenadmin/XenAdmin/Wizards/NewVMApplianceWizard/NewVMApplianceVMOrderAndDelaysPage.cs

#ifndef VMAPPLIANCESTARTORDERPAGE_H
#define VMAPPLIANCESTARTORDERPAGE_H

#include "ieditpage.h"
#include <QMap>

namespace Ui
{
    class VMApplianceStartOrderPage;
}

class VM;

/**
 * Sets the start order and delay of a vApp's VMs: a page of the New vApp wizard and of the
 * vApp's properties.
 */
class VMApplianceStartOrderPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit VMApplianceStartOrderPage(QWidget* parent = nullptr);
        ~VMApplianceStartOrderPage() override;

        //! The VMs to list (the VMs page's choice); keeps edits already made to VMs still listed
        void SetSelectedVMs(const QList<QSharedPointer<VM>>& vms);

        //! C# GetChangedSettings: VM ref -> {order, start_delay} for the rows that differ from the VM
        QMap<QString, QVariantMap> ChangedSettings() const;

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

    private slots:
        void onSelectionChanged();
        void onOrderChanged(int value);
        void onStartDelayChanged(int value);

    private:
        struct Settings
        {
            qint64 order = 0;
            qint64 startDelay = 0;
        };

        void refill();
        void updateRow(int row);
        QList<int> selectedRows() const;
        void setSpin(class QSpinBox* spin, bool same, qint64 value);

        Ui::VMApplianceStartOrderPage* ui;
        QList<QSharedPointer<VM>> m_vms;
        QMap<QString, Settings> m_settings;  //!< by VM ref, for the listed VMs
        bool m_changesMade = false;
        bool m_updating = false;
};

#endif // VMAPPLIANCESTARTORDERPAGE_H
