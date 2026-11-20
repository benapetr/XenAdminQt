/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "deletevusbaction.h"
#include "../../pusb.h"
#include "../../usbgroup.h"
#include "../../vm.h"
#include "../../vusb.h"
#include "../../xenapi/xenapi_VUSB.h"

namespace
{
    //! C# shows VUSB.Name(), which is always empty; name the passed-through device instead
    QString deviceName(const QSharedPointer<VUSB>& vusb)
    {
        const QSharedPointer<USBGroup> group = vusb->GetUSBGroup();
        const QList<QSharedPointer<PUSB>> pusbs = group ? group->GetPUSBs() : QList<QSharedPointer<PUSB>>();
        return pusbs.isEmpty() || !pusbs.first() ? QString() : pusbs.first()->Description();
    }
}

DeleteVUSBAction::DeleteVUSBAction(QSharedPointer<VUSB> vusb, QSharedPointer<VM> vm, QObject* parent)
    : AsyncOperation(vusb->GetConnection(),
                     tr("Detaching USB device '%1' from VM '%2'").arg(deviceName(vusb), vm->GetName()),
                     QString(), parent),
      m_vusbRef(vusb->OpaqueRef()),
      m_attached(vusb->CurrentlyAttached())
{
    this->SetVM(vm);

    if (this->m_attached)
    {
        this->AddApiMethodToRoleCheck("VUSB.get_allowed_operations");
        this->AddApiMethodToRoleCheck("VUSB.async_unplug");
    }
    this->AddApiMethodToRoleCheck("VUSB.async_destroy");
}

void DeleteVUSBAction::run()
{
    try
    {
        // C# unplugs in a try block and destroys in its finally block, so the VUSB is
        // destroyed even when the unplug fails; the unplug error is still reported
        QString unplugError;
        try
        {
            if (this->m_attached && XenAPI::VUSB::get_allowed_operations(this->GetSession(), this->m_vusbRef).contains("unplug"))
                this->pollToCompletion(XenAPI::VUSB::async_unplug(this->GetSession(), this->m_vusbRef), 0, 50);
        } catch (const std::exception& e)
        {
            unplugError = QString::fromUtf8(e.what());
        }

        this->SetPercentComplete(50);
        this->pollToCompletion(XenAPI::VUSB::async_destroy(this->GetSession(), this->m_vusbRef), 51, 100);
        if (!unplugError.isEmpty())
        {
            this->setError(unplugError);
            return;
        }
        this->SetDescription(tr("USB device detached"));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
