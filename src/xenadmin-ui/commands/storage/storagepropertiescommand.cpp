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

#include "storagepropertiescommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/storagepropertiesdialog.h"
#include "xenlib.h"
#include "xen/sr.h"

StoragePropertiesCommand::StoragePropertiesCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

void StoragePropertiesCommand::setTargetSR(const QString& srRef)
{
    this->m_overrideSRRef = srRef;
}

bool StoragePropertiesCommand::CanRun() const
{
    QString srRef = this->m_overrideSRRef.isEmpty() ? this->getSelectedSRRef() : this->m_overrideSRRef;
    return !srRef.isEmpty() && this->mainWindow()->xenLib()->isConnected();
}

void StoragePropertiesCommand::Run()
{
    QString srRef = this->m_overrideSRRef.isEmpty() ? this->getSelectedSRRef() : this->m_overrideSRRef;
    if (srRef.isEmpty())
        return;

    // Get connection from SR object for multi-connection support
    QSharedPointer<SR> sr = this->getSR();
    XenConnection* connection = nullptr;
    
    if (sr && sr->isValid())
    {
        connection = sr->connection();
    } else
    {
        // Fallback to main connection if no SR selected (shouldn't happen)
        connection = this->xenLib()->getConnection();
    }

    if (!connection)
    {
        qWarning() << "StoragePropertiesCommand: No connection available";
        return;
    }

    StoragePropertiesDialog dialog(connection, srRef, this->mainWindow());
    dialog.exec();
}

QString StoragePropertiesCommand::MenuText() const
{
    return "P&roperties";
}
