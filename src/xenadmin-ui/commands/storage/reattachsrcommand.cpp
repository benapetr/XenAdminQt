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
#include "xen/sr.h"
#include "xencache.h"
#include <QMessageBox>
#include <QDebug>

ReattachSRCommand::ReattachSRCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

bool ReattachSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    XenCache* cache = sr->GetConnection()->getCache();
    QVariantMap srData = sr->GetData();
    if (srData.isEmpty())
        return false;

    return this->canSRBeReattached(srData);
}

void ReattachSRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    qDebug() << "ReattachSRCommand: Opening NewSR wizard for reattaching SR" << srName << "(" << srRef << ")";

    // Open the New SR wizard which already supports reattach mode
    // The wizard will detect that this SR is detached and allow user to reattach it
    // TODO: Add constructor NewSRWizard(SR*) to pre-configure for reattach mode
    NewSRWizard* wizard = new NewSRWizard(this->mainWindow());
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->show();
}

QString ReattachSRCommand::MenuText() const
{
    return "Reattach Storage Repository";
}

QString ReattachSRCommand::getSelectedSRRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "sr")
        return QString();

    return this->getSelectedObjectRef();
}

bool ReattachSRCommand::canSRBeReattached(const QVariantMap& srData) const
{
    if (srData.isEmpty())
        return false;

    // Check if SR has any PBDs
    QVariantList pbds = srData.value("PBDs", QVariantList()).toList();
    if (pbds.isEmpty())
    {
        // SR has no PBDs - cannot reattach (SR is forgotten, not just detached)
        return false;
    }

    // Check if all PBDs are currently unplugged (SR is detached)
    bool allUnplugged = true;

    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    XenCache* cache = sr->GetConnection()->getCache();

    for (const QVariant& pbdRefVar : pbds)
    {
        QString pbdRef = pbdRefVar.toString();
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);

        if (pbdData.value("currently_attached", false).toBool())
        {
            allUnplugged = false;
            break;
        }
    }

    if (!allUnplugged)
    {
        // SR is already attached - cannot reattach
        return false;
    }

    // Get SR type
    QString srType = srData.value("type", "").toString();
    if (srType.isEmpty())
        return false;

    // Cannot reattach certain SR types
    // - "udev" (physical device) typically can't be reattached
    // - "cslg" requires special permissions (checked in C#)
    if (srType == "udev")
        return false;

    // Check if SR is locked (operation in progress)
    QVariantMap currentOps = srData.value("current_operations", QVariantMap()).toMap();
    if (!currentOps.isEmpty())
    {
        // SR has operations in progress
        return false;
    }

    // Check if SM backend exists for this SR type
    // For now, we allow common types
    QStringList supportedTypes = {"nfs", "lvmoiscsi", "lvm", "ext", "iso",
                                  "netapp", "lvmohba", "lvmofcoe", "smb", "cifs"};

    if (!supportedTypes.contains(srType))
    {
        // Unknown or unsupported SR type
        return false;
    }

    return true;
}
