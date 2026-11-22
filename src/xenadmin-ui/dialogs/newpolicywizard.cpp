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

#include "newpolicywizard.h"
#include "ui_newpolicywizard.h"
#include "../settingspanels/vmgroupvmspage.h"
#include "../settingspanels/policysnapshottypepage.h"
#include "../settingspanels/policysnapshotfrequencypage.h"
#include "../widgets/wizardnavigationpane.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmssactions.h"
#include <QAbstractButton>
#include <QHash>
#include <QPointer>

namespace
{
    QHash<XenConnection*, QPointer<NewPolicyWizard>> openWizards;
}

NewPolicyWizard::NewPolicyWizard(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent)
    : QWizard(parent), ui(new Ui::NewPolicyWizard), m_pool(pool)
{
    this->ui->setupUi(this);
    this->setWizardStyle(QWizard::ModernStyle);

    this->setPage(Page_Name, this->ui->pageName);
    this->setPage(Page_VMs, this->ui->pageVMs);
    this->setPage(Page_Type, this->ui->pageType);
    this->setPage(Page_Frequency, this->ui->pageFrequency);
    this->setPage(Page_Finish, this->ui->pageFinish);
    this->setStartId(Page_Name);

    this->m_vmsPage = new VMGroupVMsPage(VMGroup::Kind::VMSS, this->ui->pageVMs);
    this->ui->vmsLayout->addWidget(this->m_vmsPage);
    this->m_vmsPage->SetPool(pool);
    this->m_vmsPage->SetSelectedVMs(vms);

    this->m_typePage = new PolicySnapshotTypePage(this->ui->pageType);
    this->ui->typeLayout->addWidget(this->m_typePage);

    this->m_frequencyPage = new PolicySnapshotFrequencyPage(this->ui->pageFrequency);
    this->ui->frequencyLayout->addWidget(this->m_frequencyPage);
    connect(this->m_frequencyPage, &PolicySnapshotFrequencyPage::scheduleChanged, this, &NewPolicyWizard::updateNextButton);

    connect(this->ui->textName, &QLineEdit::textChanged, this, &NewPolicyWizard::updateNextButton);

    this->m_navigationPane = new WizardNavigationPane(this);
    this->m_navigationPane->setSteps({{tr("Schedule Name"), QIcon()},
                                      {tr("Virtual Machines"), QIcon()},
                                      {tr("Snapshot Type"), QIcon()},
                                      {tr("Snapshot Schedule"), QIcon()},
                                      {tr("Finish"), QIcon()}});
    this->setSideWidget(this->m_navigationPane);
    connect(this, &QWizard::currentIdChanged, this, &NewPolicyWizard::onCurrentIdChanged);
    this->setAttribute(Qt::WA_DeleteOnClose);
}

NewPolicyWizard::~NewPolicyWizard()
{
    delete this->ui;
}

void NewPolicyWizard::ShowFor(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent)
{
    if (!pool || !pool->GetConnection())
        return;

    QPointer<NewPolicyWizard>& wizard = openWizards[pool->GetConnection()];
    if (wizard)
    {
        wizard->raise();
        wizard->activateWindow();
        return;
    }
    wizard = new NewPolicyWizard(pool, vms, parent);
    wizard->show();
}

void NewPolicyWizard::showEvent(QShowEvent* event)
{
    QWizard::showEvent(event);
    this->updateNextButton();
}

void NewPolicyWizard::onCurrentIdChanged(int id)
{
    if (id >= 0)
        this->m_navigationPane->setCurrentStep(id);

    // C# NewPolicySnapshotFrequencyPage.PageLoadedCore: read the server time on the way in
    if (id == Page_Frequency && !this->m_serverTimeRequested && this->m_pool)
    {
        this->m_serverTimeRequested = true;
        this->m_frequencyPage->LoadServerTime(this->m_pool->GetConnection());
    }
    this->updateNextButton();
}

void NewPolicyWizard::updateNextButton()
{
    QAbstractButton* next = this->button(QWizard::NextButton);
    if (this->currentId() == Page_Name)
        next->setEnabled(!this->ui->textName->text().trimmed().isEmpty());
    else if (this->currentId() == Page_Frequency)
        next->setEnabled(this->m_frequencyPage->IsComplete());
}

bool NewPolicyWizard::validateCurrentPage()
{
    // C# UpdateWizardContent passes each page's result on to the next
    switch (this->currentId())
    {
        case Page_Name:
            if (this->ui->textName->text().trimmed().isEmpty())
                return false;
            this->m_vmsPage->SetGroupName(this->ui->textName->text());
            break;
        case Page_VMs:
            if (!this->m_vmsPage->UserAcceptsWarning())
                return false;
            break;
        case Page_Frequency:
        {
            if (!this->m_frequencyPage->IsComplete())
                return false;
            QStringList names;
            for (const QSharedPointer<VM>& vm : this->m_vmsPage->SelectedVMs())
                names.append(vm->GetName());
            this->ui->textSummary->setPlainText(tr("Job name:\n\t%1\n\nSelected VMs:\n\t%2\n\nSnapshot type:\n\t%3\n\nSchedule:\n\t%4")
                                                    .arg(this->ui->textName->text(), names.join(", "),
                                                         this->m_typePage->BackupTypeToString(),
                                                         this->m_frequencyPage->FormattedSchedule()));
            // C# NewPolicyFinishPage: running now needs VMs
            const bool hasVMs = !names.isEmpty();
            this->ui->checkBoxRunNow->setEnabled(hasVMs);
            if (!hasVMs)
                this->ui->checkBoxRunNow->setChecked(false);
            break;
        }
        default:
            break;
    }
    return QWizard::validateCurrentPage();
}

void NewPolicyWizard::accept()
{
    // C# FinishWizard
    XenConnection* connection = this->m_pool ? this->m_pool->GetConnection() : nullptr;
    if (connection)
    {
        const QStringList vmRefs = this->m_vmsPage->SelectedVMRefs();
        const QVariantMap record{
            {"name_label", this->ui->textName->text()},
            {"name_description", this->ui->textDescription->toPlainText()},
            {"type", this->m_typePage->BackupType()},
            {"frequency", this->m_frequencyPage->Frequency()},
            {"schedule", this->m_frequencyPage->Schedule()},
            {"retained_snapshots", this->m_frequencyPage->BackupRetention()},
            // C#: a schedule without VMs starts disabled
            {"enabled", !vmRefs.isEmpty()}};
        (new CreateVMPolicyAction(connection, record, vmRefs, this->ui->checkBoxRunNow->isChecked()))->RunAsync(true);
    }
    QWizard::accept();
}
