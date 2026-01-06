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
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QMessageBox>

BootOptionsEditPage::BootOptionsEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::BootOptionsEditPage), m_origAutoBoot(false)
{
    this->ui->setupUi(this);

    connect(this->ui->buttonUp, &QPushButton::clicked, this, &BootOptionsEditPage::onMoveUpClicked);
    connect(this->ui->buttonDown, &QPushButton::clicked, this, &BootOptionsEditPage::onMoveDownClicked);
    connect(this->ui->listWidgetBootOrder, &QListWidget::currentRowChanged,
            this, &BootOptionsEditPage::onSelectionChanged);
}

BootOptionsEditPage::~BootOptionsEditPage()
{
    delete ui;
}

QString BootOptionsEditPage::GetText() const
{
    return tr("Boot Options");
}

QString BootOptionsEditPage::GetSubText() const
{
    if (this->isHVM())
    {
        QString order = this->getBootOrder();
        QString devices;
        for (QChar c : order)
        {
            if (!devices.isEmpty())
                devices += ", ";
            if (c == 'C')
                devices += tr("Hard Disk");
            else if (c == 'D')
                devices += tr("DVD Drive");
            else if (c == 'N')
                devices += tr("Network");
        }

        if (this->ui->checkBoxAutoBoot->isChecked())
        {
            return tr("Auto-start; Boot order: %1").arg(devices.isEmpty() ? tr("Default") : devices);
        } else
        {
            return tr("Boot order: %1").arg(devices.isEmpty() ? tr("Default") : devices);
        }
    }

    if (this->ui->checkBoxAutoBoot->isChecked())
    {
        return tr("Auto-start enabled");
    }

    return tr("No specific boot order");
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
    this->m_vmRef = objectRef;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    // Get auto-boot setting from other_config
    QVariantMap otherConfig = objectDataBefore.value("other_config").toMap();
    this->m_origAutoBoot = (otherConfig.value("auto_poweron", "false").toString() == "true");
    this->ui->checkBoxAutoBoot->setChecked(this->m_origAutoBoot);

    // Get boot order from HVM_boot_params
    QVariantMap hvmBootParams = objectDataBefore.value("HVM_boot_params").toMap();
    this->m_origBootOrder = hvmBootParams.value("order", "dc").toString().toUpper();

    // Get PV args
    this->m_origPVArgs = objectDataBefore.value("PV_args", "").toString();
    this->ui->lineEditOsParams->setText(this->m_origPVArgs);

    // Check if HVM or PV
    bool vmIsHVM = objectDataBefore.value("HVM_boot_policy", "").toString() != "";

    // Show/hide appropriate sections
    this->ui->groupBoxBootOrder->setEnabled(vmIsHVM);
    this->ui->groupBoxPVParams->setVisible(!vmIsHVM);

    if (vmIsHVM)
    {
        this->populateBootOrder(this->m_origBootOrder);
    }

    this->updateButtonStates();
} 

AsyncOperation* BootOptionsEditPage::SaveSettings()
{
    if (!this->HasChanged())
    {
        return nullptr;
    }

    // Update objectDataCopy
    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
    otherConfig["auto_poweron"] = this->ui->checkBoxAutoBoot->isChecked() ? "true" : "false";
    this->m_objectDataCopy["other_config"] = otherConfig;

    if (this->isHVM())
    {
        QVariantMap hvmBootParams = this->m_objectDataCopy.value("HVM_boot_params").toMap();
        hvmBootParams["order"] = this->getBootOrder().toLower();
        this->m_objectDataCopy["HVM_boot_params"] = hvmBootParams;
    } else
    {
        this->m_objectDataCopy["PV_args"] = this->ui->lineEditOsParams->text();
    }

    // Return inline AsyncOperation for boot order changes
    class BootOptionsOperation : public AsyncOperation
    {
        public:
            BootOptionsOperation(XenConnection* conn,
                                 const QString& vmRef,
                                 bool autoBoot,
                                 const QString& bootOrder,
                                 const QString& pvArgs,
                                 bool isHVM,
                                 QObject* parent)
                : AsyncOperation(conn, tr("Change Boot Options"),
                                 tr("Changing boot configuration..."), parent),
                  m_vmRef(vmRef), m_autoBoot(autoBoot), m_bootOrder(bootOrder), m_pvArgs(pvArgs), m_isHVM(isHVM)
            {}

        protected:
            void run() override
            {
                XenRpcAPI api(GetConnection()->GetSession());

                SetPercentComplete(10);

                // Set auto-boot (via other_config.auto_poweron)
                QVariantList getConfigParams;
                getConfigParams << GetConnection()->GetSessionId() << this->m_vmRef;
                QByteArray getConfigRequest = api.BuildJsonRpcCall("VM.get_other_config", getConfigParams);
                QByteArray getConfigResponse = GetConnection()->SendRequest(getConfigRequest);
                QVariantMap otherConfig = api.ParseJsonRpcResponse(getConfigResponse).toMap();

                otherConfig["auto_poweron"] = this->m_autoBoot ? "true" : "false";

                QVariantList setConfigParams;
                setConfigParams << GetConnection()->GetSessionId() << this->m_vmRef << otherConfig;
                QByteArray setConfigRequest = api.BuildJsonRpcCall("VM.set_other_config", setConfigParams);
                GetConnection()->SendRequest(setConfigRequest);

                SetPercentComplete(40);

                if (this->m_isHVM)
                {
                    // Set boot order (via HVM_boot_params.order)
                    QVariantList getBootParams;
                    getBootParams << GetConnection()->GetSessionId() << this->m_vmRef;
                    QByteArray getBootRequest = api.BuildJsonRpcCall("VM.get_HVM_boot_params", getBootParams);
                    QByteArray getBootResponse = GetConnection()->SendRequest(getBootRequest);
                    QVariantMap hvmBootParams = api.ParseJsonRpcResponse(getBootResponse).toMap();

                    hvmBootParams["order"] = this->m_bootOrder.toLower();

                    QVariantList setBootParams;
                    setBootParams << GetConnection()->GetSessionId() << this->m_vmRef << hvmBootParams;
                    QByteArray setBootRequest = api.BuildJsonRpcCall("VM.set_HVM_boot_params", setBootParams);
                    GetConnection()->SendRequest(setBootRequest);
                } else
                {
                    // Set PV args
                    QVariantList setPVParams;
                    setPVParams << GetConnection()->GetSessionId() << this->m_vmRef << this->m_pvArgs;
                    QByteArray setPVRequest = api.BuildJsonRpcCall("VM.set_PV_args", setPVParams);
                    GetConnection()->SendRequest(setPVRequest);
                }

                SetPercentComplete(100);
            }

        private:
            QString m_vmRef;
            bool m_autoBoot;
            QString m_bootOrder;
            QString m_pvArgs;
            bool m_isHVM;
    };

    return new BootOptionsOperation(this->m_connection, this->m_vmRef,
                                    this->ui->checkBoxAutoBoot->isChecked(),
                                    this->getBootOrder(),
                                    this->ui->lineEditOsParams->text(),
                                    this->isHVM(), nullptr);
} 

bool BootOptionsEditPage::IsValidToSave() const
{
    return true;
}

void BootOptionsEditPage::ShowLocalValidationMessages()
{
    // No validation needed
}

void BootOptionsEditPage::HideLocalValidationMessages()
{
    // No validation messages to hide
}

void BootOptionsEditPage::Cleanup()
{
    // Nothing to clean up
}

bool BootOptionsEditPage::HasChanged() const
{
    bool autoBootChanged = (this->ui->checkBoxAutoBoot->isChecked() != this->m_origAutoBoot);

    if (this->isHVM())
    {
        bool bootOrderChanged = (this->getBootOrder() != this->m_origBootOrder);
        return autoBootChanged || bootOrderChanged;
    } else
    {
        bool pvArgsChanged = (this->ui->lineEditOsParams->text() != this->m_origPVArgs);
        return autoBootChanged || pvArgsChanged;
    }
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
    updateButtonStates();
}

void BootOptionsEditPage::populateBootOrder(const QString& bootOrder)
{
    this->ui->listWidgetBootOrder->clear();

    // Add devices in the specified order
    for (QChar c : bootOrder)
    {
        QListWidgetItem* item = new QListWidgetItem();
        if (c == 'C')
        {
            item->setText(tr("Hard Disk (C)"));
            item->setData(Qt::UserRole, 'C');
        } else if (c == 'D')
        {
            item->setText(tr("DVD Drive (D)"));
            item->setData(Qt::UserRole, 'D');
        } else if (c == 'N')
        {
            item->setText(tr("Network (N)"));
            item->setData(Qt::UserRole, 'N');
        }
        this->ui->listWidgetBootOrder->addItem(item);
    }

    // Add any missing devices at the end
    QString devices = "CDN";
    for (QChar device : devices)
    {
        if (!bootOrder.contains(device))
        {
            QListWidgetItem* item = new QListWidgetItem();
            if (device == 'C')
            {
                item->setText(tr("Hard Disk (C)"));
                item->setData(Qt::UserRole, 'C');
            } else if (device == 'D')
            {
                item->setText(tr("DVD Drive (D)"));
                item->setData(Qt::UserRole, 'D');
            } else if (device == 'N')
            {
                item->setText(tr("Network (N)"));
                item->setData(Qt::UserRole, 'N');
            }
            this->ui->listWidgetBootOrder->addItem(item);
        }
    }
} 

QString BootOptionsEditPage::getBootOrder() const
{
    QString order;
    for (int i = 0; i < this->ui->listWidgetBootOrder->count(); ++i)
    {
        QListWidgetItem* item = this->ui->listWidgetBootOrder->item(i);
        order += item->data(Qt::UserRole).toChar();
    }
    return order;
} 

bool BootOptionsEditPage::isHVM() const
{
    return this->m_objectDataBefore.value("HVM_boot_policy", "").toString() != "";
} 

void BootOptionsEditPage::updateButtonStates()
{
    int currentRow = this->ui->listWidgetBootOrder->currentRow();
    int count = this->ui->listWidgetBootOrder->count();

    this->ui->buttonUp->setEnabled(currentRow > 0);
    this->ui->buttonDown->setEnabled(currentRow >= 0 && currentRow < count - 1);
}
