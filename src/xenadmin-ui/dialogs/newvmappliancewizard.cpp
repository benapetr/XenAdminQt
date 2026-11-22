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

#include "newvmappliancewizard.h"
#include "ui_newvmappliancewizard.h"
#include "../settingspanels/vmgroupvmspage.h"
#include "../settingspanels/vmappliancestartorderpage.h"
#include "../widgets/wizardnavigationpane.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmapplianceactions.h"
#include "xenlib/xen/actions/vm/setvmstartupoptionsaction.h"
#include <QAbstractButton>
#include <QHash>
#include <QPointer>

namespace
{
    QHash<XenConnection*, QPointer<NewVMApplianceWizard>> openWizards;
}

NewVMApplianceWizard::NewVMApplianceWizard(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent)
    : QWizard(parent), ui(new Ui::NewVMApplianceWizard), m_pool(pool)
{
    this->ui->setupUi(this);
    this->setWizardStyle(QWizard::ModernStyle);

    this->setPage(Page_Name, this->ui->pageName);
    this->setPage(Page_VMs, this->ui->pageVMs);
    this->setPage(Page_Order, this->ui->pageOrder);
    this->setPage(Page_Finish, this->ui->pageFinish);
    this->setStartId(Page_Name);
    // C# NewVMApplianceNamePage.EnableNext: a name that isn't blank
    connect(this->ui->textName, &QLineEdit::textChanged, this, &NewVMApplianceWizard::updateNextButton);

    this->m_vmsPage = new VMGroupVMsPage(VMGroup::Kind::VMAppliance, this->ui->pageVMs);
    this->ui->vmsLayout->addWidget(this->m_vmsPage);
    this->m_vmsPage->SetPool(pool);
    this->m_vmsPage->SetSelectedVMs(vms);

    this->m_orderPage = new VMApplianceStartOrderPage(this->ui->pageOrder);
    this->ui->orderLayout->addWidget(this->m_orderPage);

    this->m_navigationPane = new WizardNavigationPane(this);
    this->m_navigationPane->setSteps({{tr("Name"), QIcon()},
                                      {tr("Virtual Machines"), QIcon()},
                                      {tr("VM Startup Sequence"), QIcon()},
                                      {tr("Finish"), QIcon()}});
    this->setSideWidget(this->m_navigationPane);
    connect(this, &QWizard::currentIdChanged, this, &NewVMApplianceWizard::onCurrentIdChanged);
    this->setAttribute(Qt::WA_DeleteOnClose);
}

NewVMApplianceWizard::~NewVMApplianceWizard()
{
    delete this->ui;
}

void NewVMApplianceWizard::ShowFor(QSharedPointer<Pool> pool, const QList<QSharedPointer<VM>>& vms, QWidget* parent)
{
    if (!pool || !pool->GetConnection())
        return;

    QPointer<NewVMApplianceWizard>& wizard = openWizards[pool->GetConnection()];
    if (wizard)
    {
        wizard->raise();
        wizard->activateWindow();
        return;
    }
    wizard = new NewVMApplianceWizard(pool, vms, parent);
    wizard->show();
}

void NewVMApplianceWizard::onCurrentIdChanged(int id)
{
    if (id >= 0)
        this->m_navigationPane->setCurrentStep(id);
    this->updateNextButton();
}

void NewVMApplianceWizard::updateNextButton()
{
    if (this->currentId() == Page_Name)
        this->button(QWizard::NextButton)->setEnabled(!this->ui->textName->text().trimmed().isEmpty());
}

void NewVMApplianceWizard::showEvent(QShowEvent* event)
{
    QWizard::showEvent(event);
    this->updateNextButton();
}

bool NewVMApplianceWizard::validateCurrentPage()
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
            this->m_orderPage->SetSelectedVMs(this->m_vmsPage->SelectedVMs());
            break;
        case Page_Order:
            this->ui->textSummary->setPlainText(this->summary());
            break;
        default:
            break;
    }
    return QWizard::validateCurrentPage();
}

QString NewVMApplianceWizard::summary() const
{
    QStringList names;
    for (const QSharedPointer<VM>& vm : this->m_vmsPage->SelectedVMs())
        names.append(vm->GetName());
    return tr("vApp name:\n    %1\n\nSelected VMs:\n    %2").arg(this->ui->textName->text(), names.join(", "));
}

void NewVMApplianceWizard::accept()
{
    // C# FinishWizard
    XenConnection* connection = this->m_pool ? this->m_pool->GetConnection() : nullptr;
    if (connection)
    {
        const QVariantMap record{{"name_label", this->ui->textName->text()},
                                 {"name_description", this->ui->textDescription->toPlainText()}};
        (new CreateVMApplianceAction(connection, record, this->m_vmsPage->SelectedVMRefs()))->RunAsync(true);

        const QMap<QString, QVariantMap> settings = this->m_orderPage->ChangedSettings();
        if (!settings.isEmpty())
            (new SetVMStartupOptionsAction(connection, this->m_pool->OpaqueRef(), settings))->RunAsync(true);
    }
    QWizard::accept();
}
