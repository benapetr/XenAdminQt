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
#include "xenlib/xen/api.h"
#include "xenlib/xen/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/actions/vm/changevmisoaction.h"
#include <QPointer>
#include <QMessageBox>
#include <QVariantMap>
#include <QVariantList>
#include <QDebug>

ChangeCDISOCommand::ChangeCDISOCommand(MainWindow* mainWindow, const QString& isoRef, QObject* parent) : VMCommand(mainWindow, parent), m_isoRef(isoRef)
{
}

bool ChangeCDISOCommand::canRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    // Check if VM has a CD/DVD drive
    return this->hasCD();
}

void ChangeCDISOCommand::run()
{
    if (!this->canRun())
        return;

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmRef = vm->opaqueRef();

    // Get XenConnection
    XenConnection* conn = vm->connection();
    if (!conn || !conn->isConnected())
    {
        QMessageBox::warning(nullptr, "Error", "Not connected to XenServer");
        return;
    }

    // Create ChangeVMISOAction (matches C# ChangeVMISOAction pattern)
    // Action automatically handles VBD lookup, insert/eject logic
    ChangeVMISOAction* action = new ChangeVMISOAction(vmObj, this->m_isoRef, "" /*vbdRef*/, MainWindow::instance());

    // Register with OperationManager for history tracking (matches C# ConnectionsManager.History.Add)
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    QString isoRef = this->m_isoRef;
    QPointer<MainWindow> mainWindow = MainWindow::instance();
    if (!mainWindow)
    {
        action->deleteLater();
        return;
    }

    connect(action, &AsyncOperation::completed, mainWindow, [isoRef, action, mainWindow](bool success) {
        if (success)
        {
            if (isoRef.isEmpty())
            {
                QMessageBox::information(mainWindow, "Success", "ISO image ejected successfully");
            } else
            {
                QMessageBox::information(mainWindow, "Success", "ISO image inserted successfully");
            }
            // Cache will be automatically refreshed via event polling
        }
        // Auto-delete when complete (matches C# GC behavior)
        action->deleteLater();
    }, Qt::QueuedConnection);

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
        XenLib* xenLib = MainWindow::instance()->xenLib();
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
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return QString();

    QVariantMap vmData = vm->data();
    QVariantList vbds = vmData.value("VBDs").toList();

    XenCache* cache = vm->connection()->cache();
    if (!cache)
        return QString();

    // Find the CD/DVD drive
    for (const QVariant& vbdRef : vbds)
    {
        QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef.toString());
        QString type = vbdData.value("type").toString();
        if (type == "CD")
            return vbdRef.toString();
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

    XenAPI* api = MainWindow::instance()->xenLib()->getAPI();
    if (!api || !MainWindow::instance()->xenLib()->isConnected())
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
