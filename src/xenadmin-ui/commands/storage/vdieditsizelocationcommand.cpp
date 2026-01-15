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

#include "vdieditsizelocationcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/vdipropertiesdialog.h"
#include "xencache.h"
#include "xen/vdi.h"

VdiEditSizeLocationCommand::VdiEditSizeLocationCommand(MainWindow* mainWindow, QObject* parent) : VDICommand(mainWindow, parent)
{
}

bool VdiEditSizeLocationCommand::CanRun() const
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return false;

    return true;
}

void VdiEditSizeLocationCommand::Run()
{
    QSharedPointer<VDI> vdi = this->getVDI();
    if (!vdi || !vdi->IsValid())
        return;

    QString vdiRef = vdi->OpaqueRef();

    // Open VDI properties dialog
    // The dialog handles size editing, name/description editing
    // For location changes, user can use "Move Virtual Disk" command
    VdiPropertiesDialog* dialog = new VdiPropertiesDialog(vdi->GetConnection(), vdiRef, this->mainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString VdiEditSizeLocationCommand::MenuText() const
{
    return "Properties...";
}
