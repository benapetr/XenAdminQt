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

// C# equivalent: xenadmin/XenAdmin/Wizards/NewVMApplianceWizard/NewVMApplianceWizard.cs

#ifndef NEWVMAPPLIANCEWIZARD_H
#define NEWVMAPPLIANCEWIZARD_H

#include <QWizard>
#include <QSharedPointer>

namespace Ui
{
    class NewVMApplianceWizard;
}

class Pool;
class VM;
class VMGroupVMsPage;
class VMApplianceStartOrderPage;
class WizardNavigationPane;

//! Creates a vApp: its name, its VMs and their start order and delays
class NewVMApplianceWizard : public QWizard
{
    Q_OBJECT

    public:
        enum PageIds
        {
            Page_Name,
            Page_VMs,
            Page_Order,
            Page_Finish
        };

        NewVMApplianceWizard(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent = nullptr);
        ~NewVMApplianceWizard() override;

        //! Shows the pool's wizard, raising it if it's already open (C# ShowPerConnectionWizard)
        static void ShowFor(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent);

        bool validateCurrentPage() override;
        void accept() override;

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void onCurrentIdChanged(int id);
        void updateNextButton();

    private:
        QString summary() const;

        Ui::NewVMApplianceWizard* ui;
        QSharedPointer<Pool> m_pool;
        VMGroupVMsPage* m_vmsPage;
        VMApplianceStartOrderPage* m_orderPage;
        WizardNavigationPane* m_navigationPane;
};

#endif // NEWVMAPPLIANCEWIZARD_H
