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

#include "repairsrdialog.h"
#include "ui_repairsrdialog.h"
#include "xen/sr.h"
#include "xen/host.h"
#include "xen/pbd.h"
#include "xen/xenobject.h"
#include "xen/actions/sr/srrepairaction.h"
#include "operations/multipleaction.h"
#include "xencache.h"
#include <QFont>
#include <QBrush>
#include <QIcon>
#include <QMessageBox>
#include <algorithm>

RepairSRDialog::RepairSRDialog(QSharedPointer<SR> sr, bool runAction, QWidget* parent) : RepairSRDialog(QList<QSharedPointer<SR>>() << sr, runAction, parent)
{
}

RepairSRDialog::RepairSRDialog(QList<QSharedPointer<SR>> srs, bool runAction, QWidget* parent) : QDialog(parent),
      ui(new Ui::RepairSRDialog),
      repairAction(nullptr),
      runAction(runAction),
      succeededWithWarning(false),
      originalHeight(0),
      shrunk(false)
{
    this->ui->setupUi(this);
    
    // Sort SR list
    this->srList = srs;
    std::sort(this->srList.begin(), this->srList.end(), [](const QSharedPointer<SR>& a, const QSharedPointer<SR>& b) {
        return a->GetName() < b->GetName();
    });
    
    // Set window title
    if (this->srList.count() == 1)
    {
        this->setWindowTitle(QString("Repair Storage Repository - %1").arg(this->srList[0]->GetName()));
    } else
    {
        this->setWindowTitle("Repair Storage Repositories");
    }
    
    // Update title label to include brand name (XenCenter equivalent)
    QString titleText = this->ui->titleLabel->text();
    // For now, just use generic text
    
    // Shrink to hide progress controls
    this->shrink();
    
    // Build initial tree
    this->buildTree();
    
    // Connect signals
    connect(this->ui->repairButton, &QPushButton::clicked, this, &RepairSRDialog::onRepairButtonClicked);
    connect(this->ui->closeButton, &QPushButton::clicked, this, &RepairSRDialog::onCloseButtonClicked);
    
    // Register for SR/Host/PBD property changes
    for (const auto& sr : this->srList)
    {
        if (sr)
        {
            connect(sr.data(), &XenObject::dataChanged, this, &RepairSRDialog::onSrPropertyChanged);
            
            XenConnection* conn = sr->GetConnection();
            if (conn && conn->GetCache())
            {
                XenCache* cache = conn->GetCache();
                connect(cache, &XenCache::objectChanged, this, &RepairSRDialog::onHostCollectionChanged, Qt::UniqueConnection);
                connect(cache, &XenCache::objectRemoved, this, &RepairSRDialog::onHostCollectionChanged, Qt::UniqueConnection);
                connect(cache, &XenCache::objectChanged, this, &RepairSRDialog::onPbdCollectionChanged, Qt::UniqueConnection);
                connect(cache, &XenCache::objectRemoved, this, &RepairSRDialog::onPbdCollectionChanged, Qt::UniqueConnection);
            }
        }
    }
}

RepairSRDialog::~RepairSRDialog()
{
    // Disconnect signals
    for (const auto& sr : this->srList)
    {
        if (sr)
        {
            disconnect(sr.data(), nullptr, this, nullptr);
            
            XenConnection* conn = sr->GetConnection();
            if (conn && conn->GetCache())
            {
                XenCache* cache = conn->GetCache();
                disconnect(cache, nullptr, this, nullptr);
            }
        }
    }
    
    delete this->ui;
}

AsyncOperation* RepairSRDialog::GetRepairAction() const
{
    return this->repairAction;
}

bool RepairSRDialog::SucceededWithWarning() const
{
    return this->succeededWithWarning;
}

QString RepairSRDialog::SucceededWithWarningDescription() const
{
    return this->succeededWithWarningDescription;
}

void RepairSRDialog::buildTree()
{
    this->ui->treeWidget->clear();
    this->treeNodes.clear();
    
    this->ui->repairButton->setEnabled(false);
    
    bool anythingBroken = false;
    bool hostsAvailable = false;
    
    for (const auto& sr : this->srList)
    {
        if (!sr || !sr->GetConnection())
            continue;
        
        XenCache* cache = sr->GetCache();
        
        const bool srBroken = sr->IsBroken();
        const bool multipathOk = sr->MultipathAOK();
        
        if (srBroken || !multipathOk)
            anythingBroken = true;
        
        // Create SR node
        RepairTreeNode srNode;
        srNode.sr = sr;
        srNode.item = new QTreeWidgetItem(this->ui->treeWidget);
        srNode.item->setText(0, sr->GetName());
        srNode.item->setExpanded(true);
        
        // Set SR icon
        QIcon srIcon(":/images/storage.png");
        srNode.item->setIcon(0, srIcon);
        
        this->treeNodes.append(srNode);
        
        QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
        std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
            return a->GetName().toLower() < b->GetName().toLower();
        });
        
        // Find storage host if not shared
        QString storageHostRef;
        bool isShared = sr->IsShared();
        QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        
        if (!isShared && !pbds.isEmpty())
        {
            // For non-shared SRs, find the storage host from existing PBD
            for (const QSharedPointer<PBD>& pbd : pbds)
            {
                if (pbd && pbd->IsValid())
                {
                    storageHostRef = pbd->GetHostRef();
                    if (!storageHostRef.isEmpty())
                        break;
                }
            }
        }
        
        // Add host nodes
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;
            
            const QString hostRef = host->OpaqueRef();

            // For non-shared SRs, only show the storage host
            if (!isShared && !storageHostRef.isEmpty() && hostRef != storageHostRef)
                continue;
            
            // Find PBD for this host
            QSharedPointer<PBD> pbdForHost;
            for (const QSharedPointer<PBD>& pbd : pbds)
            {
                if (!pbd || !pbd->IsValid())
                    continue;

                if (pbd->GetHostRef() == hostRef)
                {
                    pbdForHost = pbd;
                    break;
                }
            }
            
            // Create host node
            RepairTreeNode hostNode;
            hostNode.sr = sr;
            hostNode.host = host;
            hostNode.pbd = pbdForHost;
            hostNode.item = new QTreeWidgetItem(srNode.item);
            hostNode.item->setText(0, host->GetName());
            
            // Set host icon
            QIcon hostIcon(":/images/host.png");
            hostNode.item->setIcon(0, hostIcon);
            
            // Set status text and color
            QString status;
            QColor statusColor;
            QFont font = hostNode.item->font(1);
            
            if (!pbdForHost || !pbdForHost->IsValid())
            {
                status = "Connection missing";
                statusColor = Qt::red;
                font.setBold(true);
            } else
            {
                bool currentlyAttached = pbdForHost->IsCurrentlyAttached();
                if (currentlyAttached)
                {
                    status = "Connected";
                    statusColor = Qt::darkGreen;
                    font.setBold(false);
                } else
                {
                    status = "Unplugged";
                    statusColor = Qt::red;
                    font.setBold(true);
                }
            }
            
            hostNode.item->setText(1, status);
            hostNode.item->setForeground(1, QBrush(statusColor));
            hostNode.item->setFont(1, font);
            
            this->treeNodes.append(hostNode);
            hostsAvailable = true;
        }
    }
    
    // Enable repair button if there's something broken and we have hosts
    this->ui->repairButton->setEnabled(anythingBroken && hostsAvailable && !this->actionInProgress());
    
    // Adjust column widths
    this->ui->treeWidget->resizeColumnToContents(0);
    this->ui->treeWidget->resizeColumnToContents(1);
}

bool RepairSRDialog::actionInProgress() const
{
    return this->repairAction != nullptr && !this->repairAction->IsCompleted();
}

void RepairSRDialog::onRepairButtonClicked()
{
    this->ui->repairButton->setEnabled(false);
    this->ui->closeButton->setText("Close");
    
    if (this->srList.count() == 1)
    {
        // Single SR repair
        this->repairAction = new SrRepairAction(this->srList[0], false, this);
    } else
    {
        // Multiple SR repair
        QList<AsyncOperation*> subActions;
        
        for (const auto& sr : this->srList)
        {
            if (sr)
            {
                SrRepairAction* action = new SrRepairAction(sr, false, this);
                subActions.append(action);
            }
        }
        
        this->repairAction = new MultipleAction(
            nullptr,
            "Repair Storage Repositories",
            "Repairing storage repositories...",
            "Repair complete",
            subActions,
            true,
            false,
            false,
            this);
    }
    
    if (this->runAction)
    {
        connect(this->repairAction, &AsyncOperation::progressChanged, this, &RepairSRDialog::onActionChanged);
        connect(this->repairAction, &AsyncOperation::completed, this, &RepairSRDialog::onActionCompleted);
        connect(this->repairAction, &AsyncOperation::failed, this, &RepairSRDialog::onActionCompleted);
        
        this->grow();
        this->repairAction->RunAsync();
    }
}

void RepairSRDialog::onCloseButtonClicked()
{
    this->close();
}

void RepairSRDialog::onSrPropertyChanged()
{
    this->buildTree();
}

void RepairSRDialog::onHostCollectionChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    if (type != "host")
        return;

    for (const auto& sr : this->srList)
    {
        if (sr && sr->GetConnection() == connection)
        {
            this->buildTree();
            return;
        }
    }
}

void RepairSRDialog::onPbdCollectionChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    if (type != "pbd")
        return;

    for (const auto& sr : this->srList)
    {
        if (sr && sr->GetConnection() == connection)
        {
            this->buildTree();
            return;
        }
    }
}

void RepairSRDialog::onActionChanged()
{
    this->updateProgressControls();
}

void RepairSRDialog::onActionCompleted()
{
    // Check for multipath warnings
    for (const auto& sr : this->srList)
    {
        if (sr)
        {
            if (!sr->MultipathAOK())
            {
                this->succeededWithWarning = true;
                this->succeededWithWarningDescription = "Some multipaths are down";
                break;
            }
        }
    }
    
    // Rebuild tree for MultipleOperation case
    if (qobject_cast<MultipleAction*>(this->repairAction))
    {
        this->buildTree();
    }
    
    this->finalizeProgressControls();
}

void RepairSRDialog::shrink()
{
    if (this->shrunk)
        return;
    
    this->shrunk = true;
    
    // Hide progress controls
    this->ui->progressBar->hide();
    this->ui->statusLabel->hide();
    this->ui->separator->hide();
    
    // Calculate height difference
    int progressHeight = this->ui->progressBar->height() +
                        this->ui->statusLabel->height() +
                        this->ui->separator->height() + 30; // margins
    
    // Store original height
    if (this->originalHeight == 0)
    {
        this->originalHeight = this->height();
    }
    
    // Shrink window
    this->resize(this->width(), this->height() - progressHeight);
    this->setMinimumHeight(this->height());
}

void RepairSRDialog::grow()
{
    if (!this->shrunk)
        return;
    
    this->shrunk = false;
    
    // Show progress controls
    this->ui->progressBar->show();
    this->ui->statusLabel->show();
    this->ui->separator->show();
    
    // Restore original height
    if (this->originalHeight > 0)
    {
        this->resize(this->width(), this->originalHeight);
    }
    
    this->setMinimumHeight(0);
}

void RepairSRDialog::updateProgressControls()
{
    if (!this->repairAction)
        return;
    
    int progress = this->repairAction->GetPercentComplete();
    QString description = this->repairAction->GetDescription();
    
    this->ui->progressBar->setValue(progress);
    this->ui->statusLabel->setText(description);
}

void RepairSRDialog::finalizeProgressControls()
{
    if (!this->repairAction)
        return;
    
    bool success = !this->repairAction->HasError();
    QString description = this->repairAction->GetDescription();
    
    this->ui->progressBar->setValue(100);
    this->ui->statusLabel->setText(description);
    
    if (!success)
    {
        QString error = this->repairAction->GetErrorMessage();
        QMessageBox::warning(this, "Repair Failed", 
                           QString("Failed to repair storage repository: %1").arg(error));
    } else if (this->succeededWithWarning)
    {
        QMessageBox::warning(this, "Repair Completed with Warnings",
                           this->succeededWithWarningDescription);
    }
}
