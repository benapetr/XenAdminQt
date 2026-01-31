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

#include "reattachsrcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/newsrwizard.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include <QMessageBox>
#include <QDebug>

ReattachSRCommand::ReattachSRCommand(MainWindow* mainWindow, QObject* parent) : SRCommand(mainWindow, parent)
{
}

bool ReattachSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    return this->canSRBeReattached(sr);
}

void ReattachSRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    if (!this->canSRBeReattached(sr))
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    qDebug() << "ReattachSRCommand: Opening NewSR wizard for reattaching SR" << srName << "(" << srRef << ")";

    QSharedPointer<XenObject> ob = this->GetObject();
    XenConnection* connection = ob ? ob->GetConnection() : nullptr;
    NewSRWizard* wizard = new NewSRWizard(connection, sr, this->mainWindow());
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->show();
}

QString ReattachSRCommand::MenuText() const
{
    return "Reattach Storage Repository";
}

QString ReattachSRCommand::getSelectedSRRef() const
{
    if (this->getSelectedObjectType() != XenObjectType::SR)
        return QString();

    return this->getSelectedObjectRef();
}

bool ReattachSRCommand::canSRBeReattached(const QSharedPointer<SR>& sr) const
{
    if (!sr)
        return false;

    if (sr->HasPBDs())
        return false;

    if (sr->IsLocked() || !sr->CurrentOperations().isEmpty())
        return false;

    XenConnection* connection = sr->GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();

    const QString srType = sr->GetType();
    if (srType.isEmpty())
        return false;

    // Cannot reattach certain SR types
    // - "udev" (physical device) typically can't be reattached
    // - "cslg" requires special permissions (checked in C#)
    if (srType == "udev")
        return false;

    // TODO: Feature check for cslg (C# uses Helpers.FeatureForbidden)

    // Check if SM backend exists for this SR type (C#: SM.GetByType)
    const QList<QVariantMap> smRecords = cache->GetAllData(XenObjectType::SM);
    bool smFound = false;
    for (const QVariantMap& smData : smRecords)
    {
        if (smData.value("type").toString() == srType)
        {
            smFound = true;
            break;
        }
    }
    if (!smFound)
        return false;

    return true;
}
