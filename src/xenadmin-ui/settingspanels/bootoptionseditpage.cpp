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

#include "bootoptionseditpage.h"
#include "ui_bootoptionseditpage.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/asyncoperation.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xen/xenapi/xenapi_VBD.h"
#include <algorithm>

namespace
{
    QString deviceText(QChar c)
    {
        if (c == 'C')
            return QObject::tr("Hard Disk");
        if (c == 'D')
            return QObject::tr("DVD Drive");
        if (c == 'N')
            return QObject::tr("Network");
        return QString();
    }

    QListWidgetItem* createBootItem(QChar c, bool checked)
    {
        auto* item = new QListWidgetItem(QString("%1 (%2)").arg(deviceText(c), QString(c)));
        item->setData(Qt::UserRole, c);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        return item;
    }

    QList<QSharedPointer<VBD>> sortedVbds(const QList<QSharedPointer<VBD>>& vbds)
    {
        QList<QSharedPointer<VBD>> sorted = vbds;

        std::sort(sorted.begin(), sorted.end(), [](const QSharedPointer<VBD>& a, const QSharedPointer<VBD>& b)
        {
            if (!a || !b)
                return static_cast<bool>(a);

            const QString aUser = a->GetUserdevice();
            const QString bUser = b->GetUserdevice();

            if (aUser == "xvda")
                return true;
            if (bUser == "xvda")
                return false;

            return Misc::NaturalCompare(aUser, bUser) < 0;
        });

        return sorted;
    }
}

BootOptionsEditPage::BootOptionsEditPage(QWidget* parent)
    : IEditPage(parent),
      ui(new Ui::BootOptionsEditPage),
      m_origAutoBoot(false),
      m_origPVBootFromCD(false),
      m_currentPVBootFromCD(false),
      m_origIsHVM(false),
      m_currentIsHVM(false)
{
    this->ui->setupUi(this);

    this->ui->labelAutoBootInfo->setVisible(false);
    this->ui->labelAutoBootHAWarning->setVisible(false);

    connect(this->ui->buttonUp, &QPushButton::clicked, this, &BootOptionsEditPage::onMoveUpClicked);
    connect(this->ui->buttonDown, &QPushButton::clicked, this, &BootOptionsEditPage::onMoveDownClicked);
    connect(this->ui->listWidgetBootOrder, &QListWidget::currentRowChanged, this, &BootOptionsEditPage::onSelectionChanged);
    connect(this->ui->comboBoxBootDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BootOptionsEditPage::onPVBootDeviceChanged);
    connect(this->ui->buttonConvertToPV, &QPushButton::clicked, this, &BootOptionsEditPage::onConvertToPVClicked);
    connect(this->ui->buttonConvertToHVM, &QPushButton::clicked, this, &BootOptionsEditPage::onConvertToHVMClicked);
}

BootOptionsEditPage::~BootOptionsEditPage()
{
    delete this->ui;
}

QString BootOptionsEditPage::GetText() const
{
    return tr("Boot Options");
}

QString BootOptionsEditPage::GetSubText() const
{
    if (this->m_currentIsHVM)
    {
        QStringList devices;
        for (QChar c : this->getBootOrder())
        {
            const QString text = deviceText(c);
            if (!text.isEmpty())
                devices << text;
        }

        const QString order = devices.isEmpty() ? tr("Default") : devices.join(", ");
        if (this->ui->checkBoxAutoBoot->isChecked())
            return tr("Auto-start; Boot order: %1").arg(order);

        return tr("Boot order: %1").arg(order);
    }

    return tr("None defined");
}

QIcon BootOptionsEditPage::GetImage() const
{
    return QIcon(":/icons/power_on.png");
}

void BootOptionsEditPage::SetXenObjects(const QString& objectRef,
                                        const QString& objectType,
                                        const QVariantMap& objectDataBefore,
                                        const QVariantMap& objectDataCopy)
{
    if (objectType != "vm")
        return;

    this->m_vmRef = objectRef;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;
    this->m_vm.clear();

    if (this->connection() && this->connection()->GetCache())
        this->m_vm = this->connection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, objectRef);

    this->m_origAutoBoot = objectDataBefore.value("other_config").toMap().value("auto_poweron", "false").toString() == "true";
    this->m_origBootOrder = objectDataBefore.value("HVM_boot_params").toMap().value("order", "dc").toString().toUpper();
    this->m_origPVArgs = objectDataBefore.value("PV_args").toString();
    this->m_origIsHVM = !objectDataBefore.value("HVM_boot_policy").toString().isEmpty();
    this->m_currentIsHVM = this->m_origIsHVM;

    this->m_origPVBootFromCD = this->vmPVBootableDVD();
    this->m_currentPVBootFromCD = this->m_origPVBootFromCD;

    this->ui->checkBoxAutoBoot->setChecked(this->m_origAutoBoot);
    this->ui->lineEditOsParams->setText(this->m_origPVArgs);

    this->repopulate();
}

void BootOptionsEditPage::repopulate()
{
    this->ui->groupBoxBootOrder->setVisible(this->m_currentIsHVM);
    this->ui->groupBoxPVParams->setVisible(!this->m_currentIsHVM);

    if (this->m_currentIsHVM)
    {
        this->populateBootOrder(this->m_origBootOrder);
    }
    else
    {
        this->ui->comboBoxBootDevice->blockSignals(true);
        this->ui->comboBoxBootDevice->clear();
        this->ui->comboBoxBootDevice->addItem(tr("Hard Disk"));

        if (this->m_vm && this->m_vm->HasCD())
        {
            this->ui->comboBoxBootDevice->addItem(tr("DVD Drive"));
            this->ui->comboBoxBootDevice->setCurrentIndex(this->m_currentPVBootFromCD ? 1 : 0);
        }
        else
        {
            this->ui->comboBoxBootDevice->setCurrentIndex(0);
        }
        this->ui->comboBoxBootDevice->blockSignals(false);
    }

    this->updateButtonStates();
}

AsyncOperation* BootOptionsEditPage::SaveSettings()
{
    if (!this->HasChanged() || !this->m_vm)
        return nullptr;

    const bool autoBoot = this->ui->checkBoxAutoBoot->isChecked();
    const QString hvmOrder = this->getBootOrder();
    const QString pvArgs = this->ui->lineEditOsParams->text();
    const bool saveAsHvm = this->m_currentIsHVM;
    const bool bootFromCd = this->m_currentPVBootFromCD;
    const bool modeChanged = (this->m_currentIsHVM != this->m_origIsHVM);

    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
    otherConfig["auto_poweron"] = autoBoot ? "true" : "false";
    this->m_objectDataCopy["other_config"] = otherConfig;

    if (saveAsHvm)
    {
        QVariantMap hvmBootParams = this->m_objectDataCopy.value("HVM_boot_params").toMap();
        hvmBootParams["order"] = hvmOrder.toLower();
        this->m_objectDataCopy["HVM_boot_params"] = hvmBootParams;
        this->m_objectDataCopy["HVM_boot_policy"] = "BIOS Order";
    }
    else
    {
        this->m_objectDataCopy["PV_args"] = pvArgs;
        this->m_objectDataCopy["HVM_boot_policy"] = "";
        if (modeChanged)
            this->m_objectDataCopy["PV_bootloader"] = "pygrub";
    }

    class BootOptionsOperation : public AsyncOperation
    {
        public:
            BootOptionsOperation(QSharedPointer<VM> vm,
                                 bool autoBoot,
                                 const QString& hvmOrder,
                                 const QString& pvArgs,
                                 bool saveAsHvm,
                                 bool bootFromCd,
                                 bool modeChanged,
                                 QObject* parent)
                : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                                 QObject::tr("Change Boot Options"),
                                 QObject::tr("Changing boot configuration..."),
                                 true,
                                 parent),
                  m_vm(vm),
                  m_autoBoot(autoBoot),
                  m_hvmOrder(hvmOrder),
                  m_pvArgs(pvArgs),
                  m_saveAsHvm(saveAsHvm),
                  m_bootFromCd(bootFromCd),
                  m_modeChanged(modeChanged)
            {
                AddApiMethodToRoleCheck("VM.set_other_config");
                AddApiMethodToRoleCheck("VM.set_HVM_boot_policy");
                AddApiMethodToRoleCheck("VM.set_HVM_boot_params");
                AddApiMethodToRoleCheck("VM.set_PV_args");
                AddApiMethodToRoleCheck("VM.set_PV_bootloader");
                AddApiMethodToRoleCheck("VBD.set_bootable");
            }

        protected:
            void run() override
            {
                if (!m_vm || !m_vm->IsValid())
                {
                    this->setError(QObject::tr("VM is no longer available."));
                    return;
                }

                XenAPI::Session* session = this->GetSession();
                if (!session || !session->IsLoggedIn())
                {
                    this->setError(QObject::tr("No valid session."));
                    return;
                }

                SetPercentComplete(10);

                QVariantMap otherConfig = m_vm->GetOtherConfig();
                otherConfig["auto_poweron"] = m_autoBoot ? "true" : "false";
                XenAPI::VM::set_other_config(session, m_vm->OpaqueRef(), otherConfig);

                SetPercentComplete(30);

                if (m_saveAsHvm)
                {
                    XenAPI::VM::set_HVM_boot_policy(session, m_vm->OpaqueRef(), QStringLiteral("BIOS Order"));
                    QVariantMap bootParams = XenAPI::VM::get_HVM_boot_params(session, m_vm->OpaqueRef());
                    bootParams["order"] = m_hvmOrder.toLower();
                    XenAPI::VM::set_HVM_boot_params(session, m_vm->OpaqueRef(), bootParams);
                }
                else
                {
                    XenAPI::VM::set_HVM_boot_policy(session, m_vm->OpaqueRef(), QString());
                    XenAPI::VM::set_PV_args(session, m_vm->OpaqueRef(), m_pvArgs);

                    if (m_modeChanged)
                    {
                        XenAPI::VM::set_PV_bootloader(session, m_vm->OpaqueRef(), QStringLiteral("pygrub"));
                    }
                }

                SetPercentComplete(65);

                QList<QSharedPointer<VBD>> vbds = sortedVbds(m_vm->GetVBDs());
                if (m_bootFromCd)
                {
                    for (const QSharedPointer<VBD>& vbd : vbds)
                    {
                        if (!vbd || !vbd->IsValid())
                            continue;
                        XenAPI::VBD::set_bootable(session, vbd->OpaqueRef(), vbd->IsCD());
                    }
                }
                else
                {
                    bool foundSystemDisk = false;
                    for (const QSharedPointer<VBD>& vbd : vbds)
                    {
                        if (!vbd || !vbd->IsValid())
                            continue;

                        bool bootable = !foundSystemDisk && !vbd->IsCD();
                        if (bootable)
                            foundSystemDisk = true;

                        XenAPI::VBD::set_bootable(session, vbd->OpaqueRef(), bootable);
                    }
                }

                SetPercentComplete(100);
            }

        private:
            QSharedPointer<VM> m_vm;
            bool m_autoBoot;
            QString m_hvmOrder;
            QString m_pvArgs;
            bool m_saveAsHvm;
            bool m_bootFromCd;
            bool m_modeChanged;
    };

    return new BootOptionsOperation(this->m_vm,
                                    autoBoot,
                                    hvmOrder,
                                    pvArgs,
                                    saveAsHvm,
                                    bootFromCd,
                                    modeChanged,
                                    this);
}

bool BootOptionsEditPage::IsValidToSave() const
{
    return true;
}

void BootOptionsEditPage::ShowLocalValidationMessages()
{
}

void BootOptionsEditPage::HideLocalValidationMessages()
{
}

void BootOptionsEditPage::Cleanup()
{
}

bool BootOptionsEditPage::HasChanged() const
{
    const bool autoBootChanged = (this->ui->checkBoxAutoBoot->isChecked() != this->m_origAutoBoot);
    const bool modeChanged = (this->m_currentIsHVM != this->m_origIsHVM);
    const bool pvBootChanged = (this->m_currentPVBootFromCD != this->m_origPVBootFromCD);

    if (this->m_currentIsHVM)
    {
        const bool bootOrderChanged = (this->getBootOrder() != this->m_origBootOrder);
        return autoBootChanged || modeChanged || bootOrderChanged || pvBootChanged;
    }

    const bool pvArgsChanged = (this->ui->lineEditOsParams->text() != this->m_origPVArgs);
    return autoBootChanged || modeChanged || pvArgsChanged || pvBootChanged;
}

void BootOptionsEditPage::onMoveUpClicked()
{
    int currentRow = this->ui->listWidgetBootOrder->currentRow();
    if (currentRow > 0)
    {
        QListWidgetItem* item = this->ui->listWidgetBootOrder->takeItem(currentRow);
        this->ui->listWidgetBootOrder->insertItem(currentRow - 1, item);
        this->ui->listWidgetBootOrder->setCurrentRow(currentRow - 1);
    }
    this->updateButtonStates();
}

void BootOptionsEditPage::onMoveDownClicked()
{
    int currentRow = this->ui->listWidgetBootOrder->currentRow();
    if (currentRow >= 0 && currentRow < this->ui->listWidgetBootOrder->count() - 1)
    {
        QListWidgetItem* item = this->ui->listWidgetBootOrder->takeItem(currentRow);
        this->ui->listWidgetBootOrder->insertItem(currentRow + 1, item);
        this->ui->listWidgetBootOrder->setCurrentRow(currentRow + 1);
    }
    this->updateButtonStates();
}

void BootOptionsEditPage::onSelectionChanged()
{
    this->updateButtonStates();
}

void BootOptionsEditPage::onPVBootDeviceChanged()
{
    this->m_currentPVBootFromCD = this->ui->comboBoxBootDevice->currentIndex() == 1;
}

void BootOptionsEditPage::onConvertToPVClicked()
{
    this->convert(true);
}

void BootOptionsEditPage::onConvertToHVMClicked()
{
    this->convert(false);
}

void BootOptionsEditPage::populateBootOrder(const QString& bootOrder)
{
    this->ui->listWidgetBootOrder->clear();

    for (QChar c : bootOrder)
    {
        if (c == 'C' || c == 'D' || c == 'N')
            this->ui->listWidgetBootOrder->addItem(createBootItem(c, true));
    }

    const QString all = QStringLiteral("CDN");
    for (QChar c : all)
    {
        if (!bootOrder.contains(c))
            this->ui->listWidgetBootOrder->addItem(createBootItem(c, false));
    }

    if (this->ui->listWidgetBootOrder->count() > 0)
        this->ui->listWidgetBootOrder->setCurrentRow(0);
}

QString BootOptionsEditPage::getBootOrder() const
{
    QString order;
    for (int i = 0; i < this->ui->listWidgetBootOrder->count(); ++i)
    {
        const QListWidgetItem* item = this->ui->listWidgetBootOrder->item(i);
        if (!item || item->checkState() != Qt::Checked)
            continue;
        order += item->data(Qt::UserRole).toChar();
    }
    return order;
}

bool BootOptionsEditPage::vmPVBootableDVD() const
{
    if (!this->m_vm)
        return false;

    for (const QSharedPointer<VBD>& vbd : this->m_vm->GetVBDs())
    {
        if (vbd && vbd->IsValid() && vbd->IsCD() && vbd->IsBootable())
            return true;
    }

    return false;
}

void BootOptionsEditPage::convert(bool toPV)
{
    if (toPV)
    {
        this->m_currentIsHVM = false;
        this->ui->lineEditOsParams->setText(QStringLiteral("console=tty0 xencons=tty"));
    }
    else
    {
        this->m_currentIsHVM = true;
    }

    this->repopulate();
}

void BootOptionsEditPage::updateButtonStates()
{
    const int currentRow = this->ui->listWidgetBootOrder->currentRow();
    const int count = this->ui->listWidgetBootOrder->count();

    this->ui->buttonUp->setEnabled(this->m_currentIsHVM && currentRow > 0);
    this->ui->buttonDown->setEnabled(this->m_currentIsHVM && currentRow >= 0 && currentRow < count - 1);

    const bool vmHalted = this->m_vm && this->m_vm->GetPowerState() == QStringLiteral("Halted");
    this->ui->buttonConvertToPV->setEnabled(vmHalted && this->m_currentIsHVM);
    this->ui->buttonConvertToHVM->setEnabled(vmHalted && !this->m_currentIsHVM);
}
