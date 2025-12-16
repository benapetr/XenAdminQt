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

#include "changecdisocommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xen/api.h"
#include "xen/connection.h"
#include "xen/actions/vm/changevmisoaction.h"
#include "xencache.h"
#include <QMessageBox>
#include <QVariantMap>
#include <QVariantList>
#include <QDebug>

ChangeCDISOCommand::ChangeCDISOCommand(MainWindow* mainWindow, const QString& isoRef, QObject* parent)
    : Command(mainWindow, parent), m_isoRef(isoRef)
{
}

bool ChangeCDISOCommand::canRun() const
{
    // Need exactly one VM selected
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
    {
        return false;
    }

    // Check if it's a VM
    QString type = item->data(0, Qt::UserRole).toString();
    if (type != "vm")
    {
        return false;
    }

    // Check if VM has a CD/DVD drive
    return this->hasCD();
}

void ChangeCDISOCommand::run()
{
    if (!this->canRun())
    {
        return;
    }

    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
    {
        return;
    }

    QString vmRef = item->data(0, Qt::UserRole + 1).toString();

    // Get XenConnection from XenLib
    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(nullptr, "Error", "Not connected to XenServer");
        return;
    }

    // Create ChangeVMISOAction (matches C# ChangeVMISOAction pattern)
    // Action automatically handles VBD lookup, insert/eject logic
    ChangeVMISOAction* action = new ChangeVMISOAction(conn, vmRef, this->m_isoRef, this->mainWindow());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, this, [this, action](bool success) {
        if (success)
        {
            if (this->m_isoRef.isEmpty())
            {
                QMessageBox::information(nullptr, "Success", "ISO image ejected successfully");
            } else
            {
                QMessageBox::information(nullptr, "Success", "ISO image inserted successfully");
            }
            // Cache will be automatically refreshed via event polling
        }
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    });

    // Run action asynchronously (matches C# pattern)
    // Progress shown in status bar via OperationManager signals
    action->runAsync();
}

QString ChangeCDISOCommand::menuText() const
{
    if (this->m_isoRef.isEmpty())
    {
        return "Eject CD/DVD";
    } else
    {
        // Use cache to get ISO name
        XenLib* xenLib = this->mainWindow()->xenLib();
        if (xenLib && xenLib->isConnected())
        {
            QVariantMap vdiData = xenLib->getCache()->resolve("vdi", this->m_isoRef);
            QString name = vdiData.value("name_label").toString();
            if (!name.isEmpty())
            {
                return name;
            }
        }
        return "Insert ISO...";
    }
}

QString ChangeCDISOCommand::getVMCDROM() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
    {
        return QString();
    }

    QString vmRef = item->data(0, Qt::UserRole + 1).toString();

    XenLib* xenLib = this->mainWindow()->xenLib();
    if (!xenLib || !xenLib->isConnected())
    {
        return QString();
    }

    // Use cache to get VM record
    QVariantMap vmData = xenLib->getCache()->resolve("vm", vmRef);
    QVariantList vbds = vmData.value("VBDs").toList();

    // Find the CD/DVD drive
    XenAPI* api = xenLib->getAPI();
    for (const QVariant& vbdRef : vbds)
    {
        // Try cache first, fall back to API if needed
        QVariantMap vbdData = xenLib->getCache()->resolve("vbd", vbdRef.toString());
        if (vbdData.isEmpty() && api)
            vbdData = api->getVBDRecord(vbdRef.toString()).toMap();

        QString type = vbdData.value("type").toString();
        if (type == "CD")
        {
            return vbdRef.toString();
        }
    }

    return QString();
}

bool ChangeCDISOCommand::hasCD() const
{
    return !this->getVMCDROM().isEmpty();
}

QString ChangeCDISOCommand::getCurrentISO() const
{
    QString cdromRef = this->getVMCDROM();
    if (cdromRef.isEmpty())
    {
        return QString();
    }

    XenAPI* api = this->mainWindow()->xenLib()->getAPI();
    if (!api || !this->mainWindow()->xenLib()->isConnected())
    {
        return QString();
    }

    QVariantMap vbdData = api->getVBDRecord(cdromRef).toMap();
    bool isEmpty = vbdData.value("empty").toBool();

    if (isEmpty)
    {
        return QString();
    }

    return vbdData.value("VDI").toString();
}
