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

#include "multipledvdisolist.h"
#include "ui_multipledvdisolist.h"
#include "widgets/cdchanger.h"
#include "xen/vm.h"
#include "xen/vbd.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/actions/vm/changevmisoaction.h"
#include "xen/actions/vm/createcddriveaction.h"
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

MultipleDvdIsoList::MultipleDvdIsoList(QWidget* parent) : QWidget(parent), ui(new Ui::MultipleDvdIsoList)
{
    this->ui->setupUi(this);
    this->setupConnections();
}

MultipleDvdIsoList::~MultipleDvdIsoList()
{
    this->deregisterEvents();
    delete this->ui;
}

void MultipleDvdIsoList::setupConnections()
{
    connect(this->ui->comboBoxDrive, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MultipleDvdIsoList::onComboBoxDriveIndexChanged);
    connect(this->ui->newCDLabel, &QLabel::linkActivated, this, &MultipleDvdIsoList::onNewCdLabelClicked);
    connect(this->ui->ejectButton, &QPushButton::clicked, this, &MultipleDvdIsoList::onLinkLabelEjectClicked);
}

void MultipleDvdIsoList::SetVM(const QSharedPointer<VM>& vm)
{
    this->deregisterEvents();
    
    this->m_vm = vm;
    qDebug() << "MultipleDvdIsoList::SetVM vm" << (this->m_vm ? this->m_vm->OpaqueRef() : QString());
    
    // Set VM on CDChanger widget
    this->ui->cdChanger->SetVM(this->m_vm);
    
    if (this->m_vm)
    {
        // Connect to VM data changes via XenObject's dataChanged signal
        this->vmConnection_ = connect(this->m_vm.data(), &XenObject::dataChanged, this, &MultipleDvdIsoList::onVmPropertyChanged);
    }
    
    this->refreshDrives();
}

QSharedPointer<VM> MultipleDvdIsoList::GetVM() const
{
    return this->ui->cdChanger->GetVM();
}

void MultipleDvdIsoList::SetLabelSingleDvdForeColor(const QColor& color)
{
    QPalette palette = this->ui->labelSingleDvd->palette();
    palette.setColor(QPalette::WindowText, color);
    this->ui->labelSingleDvd->setPalette(palette);
}

QColor MultipleDvdIsoList::GetLabelSingleDvdForeColor() const
{
    return this->ui->labelSingleDvd->palette().color(QPalette::WindowText);
}

void MultipleDvdIsoList::SetLabelNewCdForeColor(const QColor& color)
{
    QPalette palette = this->ui->newCDLabel->palette();
    palette.setColor(QPalette::WindowText, color);
    this->ui->newCDLabel->setPalette(palette);
}

QColor MultipleDvdIsoList::GetLabelNewCdForeColor() const
{
    return this->ui->newCDLabel->palette().color(QPalette::WindowText);
}

void MultipleDvdIsoList::SetLinkLabelLinkColor(const QColor& color)
{
    QPalette palette = this->ui->ejectButton->palette();
    palette.setColor(QPalette::ButtonText, color);
    this->ui->ejectButton->setPalette(palette);
}

QColor MultipleDvdIsoList::GetLinkLabelLinkColor() const
{
    return this->ui->ejectButton->palette().color(QPalette::ButtonText);
}

void MultipleDvdIsoList::deregisterEvents()
{
    if (!this->m_vm)
        return;

    // Disconnect VM listener
    if (this->vmConnection_)
    {
        disconnect(this->vmConnection_);
        this->vmConnection_ = QMetaObject::Connection();
    }

    // Disconnect cache listener
    if (this->cacheConnection_)
    {
        disconnect(this->cacheConnection_);
        this->cacheConnection_ = QMetaObject::Connection();
    }

    // Disconnect VBD listeners
    for (const auto& conn : this->vbdConnections_)
    {
        disconnect(conn);
    }
    this->vbdConnections_.clear();

    // Deregister CDChanger events
    this->ui->cdChanger->DeregisterEvents();
}

void MultipleDvdIsoList::onVmPropertyChanged()
{
    // VM data changed, refresh the drives list
    this->refreshDrives();
}

void MultipleDvdIsoList::refreshDrives()
{
    // Save previous selection
    QString prevSelectedUuid;
    QVariant selectedData = this->ui->comboBoxDrive->currentData();
    if (!selectedData.isNull())
    {
        VbdCombiItem* item = selectedData.value<VbdCombiItem*>();
        if (item && item->vbd)
        {
            prevSelectedUuid = item->vbd->GetUUID();
        }
    }

    this->m_inRefresh = true;

    // Disconnect old VBD property listeners
    for (const auto& conn : this->vbdConnections_)
    {
        disconnect(conn);
    }
    this->vbdConnections_.clear();

    // Clear combo box
    while (this->ui->comboBoxDrive->count() > 0)
    {
        QVariant data = this->ui->comboBoxDrive->itemData(0);
        if (!data.isNull())
        {
            VbdCombiItem* item = data.value<VbdCombiItem*>();
            delete item;
        }
        this->ui->comboBoxDrive->removeItem(0);
    }

    if (this->m_vm && !this->m_vm->IsControlDomain())
    {
        XenConnection* connection = this->m_vm->GetConnection();
        if (!connection)
        {
            qDebug() << "MultipleDvdIsoList::refreshDrives no connection for VM"
                     << this->m_vm->OpaqueRef();
            this->m_inRefresh = false;
            return;
        }

        XenCache* cache = connection->GetCache();
        if (!cache)
        {
            qDebug() << "MultipleDvdIsoList::refreshDrives no cache for VM"
                     << this->m_vm->OpaqueRef();
            this->m_inRefresh = false;
            return;
        }

        // Get VBDs from VM
        QStringList vbdRefs = this->m_vm->GetVBDRefs();
        if (vbdRefs.isEmpty() && !this->cacheConnection_)
        {
            this->cacheConnection_ = connect(connection, &XenConnection::CachePopulated,
                                             this, &MultipleDvdIsoList::onCachePopulated);
        }
        qDebug() << "MultipleDvdIsoList::refreshDrives VM" << this->m_vm->OpaqueRef()
                 << "VBDs" << vbdRefs.count();

        QList<QSharedPointer<VBD>> vbds;
        for (const QString& vbdRef : vbdRefs)
        {
            QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>("vbd", vbdRef);
            if (vbd && vbd->IsValid())
            {
                // Check if it's a CD-ROM or floppy drive, matching C# logic
                if (vbd->IsCD() || vbd->IsFloppyDrive())
                {
                    vbds.append(vbd);
                }
            }
        }
        qDebug() << "MultipleDvdIsoList::refreshDrives CD/floppy drives" << vbds.count();

        // Sort VBDs by userdevice field
        std::sort(vbds.begin(), vbds.end(), [](const QSharedPointer<VBD>& a, const QSharedPointer<VBD>& b)
        {
            return a->GetUserdevice() < b->GetUserdevice();
        });

        int dvdCount = 0;
        int floppyCount = 0;

        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            // Connect to VBD data changes
            auto conn = connect(vbd.data(), &XenObject::dataChanged,
                              this, &MultipleDvdIsoList::onVbdPropertyChanged);
            this->vbdConnections_.append(conn);

            VbdCombiItem* item;
            if (vbd->IsCD())
            {
                dvdCount++;
                item = new VbdCombiItem(tr("DVD Drive %1").arg(dvdCount), vbd);
            } else
            {
                floppyCount++;
                item = new VbdCombiItem(tr("Floppy Drive %1").arg(floppyCount), vbd);
            }

            this->ui->comboBoxDrive->addItem(item->toString(), QVariant::fromValue(item));
        }
    }

    // Update visibility based on drive count
    int driveCount = this->ui->comboBoxDrive->count();

    this->ui->labelSingleDvd->setVisible(driveCount == 1);
    if (driveCount == 1)
    {
        this->ui->labelSingleDvd->setText(this->ui->comboBoxDrive->itemText(0));
    }

    this->ui->comboBoxDrive->setVisible(driveCount > 1);
    this->ui->cdChanger->setVisible(driveCount > 0);
    this->ui->ejectButton->setVisible(driveCount > 0);
    this->ui->newCDLabel->setVisible(driveCount == 0 && this->m_vm && !this->m_vm->IsControlDomain());

    this->m_inRefresh = false;

    // Restore previous selection or select first item
    if (!prevSelectedUuid.isEmpty())
    {
        for (int i = 0; i < this->ui->comboBoxDrive->count(); ++i)
        {
            QVariant data = this->ui->comboBoxDrive->itemData(i);
            if (!data.isNull())
            {
                VbdCombiItem* item = data.value<VbdCombiItem*>();
                if (item && item->vbd && item->vbd->GetUUID() == prevSelectedUuid)
                {
                    this->ui->comboBoxDrive->setCurrentIndex(i);
                    this->updateCdChangerDrive(item->vbd);
                    return;
                }
            }
        }
    }

    // Select first item by default
    if (driveCount > 0)
    {
        this->ui->comboBoxDrive->setCurrentIndex(0);
        QVariant data = this->ui->comboBoxDrive->itemData(0);
        VbdCombiItem* item = data.value<VbdCombiItem*>();
        if (item)
            this->updateCdChangerDrive(item->vbd);
    }
}

void MultipleDvdIsoList::onVbdPropertyChanged()
{
    this->refreshDrives();
}

void MultipleDvdIsoList::onCachePopulated()
{
    if (this->cacheConnection_)
    {
        disconnect(this->cacheConnection_);
        this->cacheConnection_ = QMetaObject::Connection();
    }
    qDebug() << "MultipleDvdIsoList::onCachePopulated";
    this->refreshDrives();
}

void MultipleDvdIsoList::onComboBoxDriveIndexChanged(int index)
{
    if (this->m_inRefresh)
        return;

    if (index >= 0)
    {
        QVariant data = this->ui->comboBoxDrive->itemData(index);
        if (!data.isNull())
        {
            VbdCombiItem* item = data.value<VbdCombiItem*>();
            if (item)
            {
                this->updateCdChangerDrive(item->vbd);
            }
        }
    }
}

void MultipleDvdIsoList::updateCdChangerDrive(const QSharedPointer<VBD>& drive)
{
    this->ui->cdChanger->SetDrive(drive);
}

void MultipleDvdIsoList::onNewCdLabelClicked()
{
    // Reference: MultipleDvdIsoList.cs lines 243-265 and VNCTabView.cpp onCreateNewCdDrive()
    if (!this->m_vm)
        return;

    QString vmName = this->m_vm->GetName();
    
    if (this->m_vm->IsHVM())
    {
        QString message = tr("Your VM has more than one DVD drive. All drives must be created before you can start it. "
                           "Attempting to start it now will create a drive.\n\n"
                           "Do you want to create the drive now?");
        
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("Create DVD Drive"),
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        
        if (result != QMessageBox::Yes)
            return;
    }

    // Create CD drive using CreateCdDriveAction (matches C# CreateCdDriveAction)
    QSharedPointer<CreateCdDriveAction> action(new CreateCdDriveAction(this->m_vm));
    
    connect(action.data(), &AsyncOperation::completed, this, [this]() {
        qDebug() << "MultipleDvdIsoList: CD/DVD drive created successfully";
        // Refresh drives list after creation
        QTimer::singleShot(500, this, [this]() {
            this->refreshDrives();
        });
    });
    
    connect(action.data(), &AsyncOperation::failed, this, [this, vmName](const QString& error) {
        qWarning() << "MultipleDvdIsoList: Failed to create CD/DVD drive:" << error;
        QMessageBox::warning(this, tr("Create DVD Drive"), 
                           tr("Failed to create CD/DVD drive for VM '%1': %2").arg(vmName, error));
    });
    
    // Forward user instructions (e.g., "Please reboot the VM")
    connect(action.data(), &CreateCdDriveAction::showUserInstruction, this, [this](const QString& instruction) {
        QMessageBox::information(this, tr("DVD Drive"), instruction);
    });
    
    action->RunAsync();
}

void MultipleDvdIsoList::onLinkLabelEjectClicked()
{
    // Eject current CD = mount empty VDI (nullptr)
    // Reference: VNCTabView.cpp onCdEject() and CDChanger.cs ChangeCD()
    if (!this->m_vm || !this->m_vm->GetConnection())
        return;
    
    QVariant selectedData = this->ui->comboBoxDrive->currentData();
    if (selectedData.isNull())
        return;
    
    VbdCombiItem* item = selectedData.value<VbdCombiItem*>();
    if (!item || !item->vbd)
        return;
    
    QString vbdRef = item->vbd->OpaqueRef();
    
    // Use ChangeVMISOAction to eject (mount empty - pass empty string for VDI)
    ChangeVMISOAction* action = new ChangeVMISOAction(
        this->m_vm,
        QString(),  // Empty VDI ref = eject
        vbdRef,
        this
    );
    
    connect(action, &AsyncOperation::completed, action, [this, action]()
    {
        qDebug() << "MultipleDvdIsoList: CD/DVD eject operation completed";
        // Refresh drives list to show updated state
        QTimer::singleShot(500, this, [this]() {
            this->refreshDrives();
        });
        action->deleteLater();
    });
    
    connect(action, &AsyncOperation::failed, action, [action](const QString& error)
    {
        qWarning() << "MultipleDvdIsoList: Failed to eject CD/DVD:" << error;
        action->deleteLater();
    });
    
    action->RunAsync();
}

void MultipleDvdIsoList::onCreateDriveActionCompleted()
{
    // Handle completion of CreateCdDriveAction
    this->refreshDrives();
}
