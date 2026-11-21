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

// C# equivalent: xenadmin/XenAdmin/Wizards/GenericPages/NewVMGroupVMsPage.cs (for VM_appliance)

#ifndef VMAPPLIANCEVMSPAGE_H
#define VMAPPLIANCEVMSPAGE_H

#include "ieditpage.h"
#include <QSet>

namespace Ui
{
    class VMApplianceVMsPage;
}

class Pool;
class VM;
class VMAppliance;
class QTableWidgetItem;

/**
 * Chooses the VMs in a vApp: a page of the New vApp wizard and of the vApp's properties.
 */
class VMApplianceVMsPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit VMApplianceVMsPage(QWidget* parent = nullptr);
        ~VMApplianceVMsPage() override;

        //! The pool whose VMs are listed (C# Pool property); refills the list
        void SetPool(QSharedPointer<Pool> pool);
        //! Preselects VMs, e.g. the ones selected when "New vApp..." was chosen
        void SetSelectedVMs(const QList<QSharedPointer<VM>>& vms);
        QList<QSharedPointer<VM>> SelectedVMs() const;
        QStringList SelectedVMRefs() const;
        //! The new vApp's name, for the warning about VMs already in another vApp
        void SetGroupName(const QString& name) { this->m_groupName = name; }

        /**
         * C# PageLeaveCore/ValidToSave: if any chosen VM is in another vApp, asks whether to
         * move it. Returns whether to go on.
         */
        bool UserAcceptsWarning();

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

    signals:
        void selectionChanged();

    private slots:
        void onItemChanged(QTableWidgetItem* item);
        void onSelectAll();
        void onClearAll();
        void refill();

    private:
        void setAllChecked(bool checked);
        void updateCounterAndButtons();

        Ui::VMApplianceVMsPage* ui;
        QSharedPointer<Pool> m_pool;
        QSharedPointer<VMAppliance> m_appliance;
        QStringList m_selectedRefs;  //!< in the order chosen, like C#'s SelectedVMs list
        QString m_groupName;
        bool m_filling = false;
        mutable bool m_warningAccepted = false;
};

#endif // VMAPPLIANCEVMSPAGE_H
