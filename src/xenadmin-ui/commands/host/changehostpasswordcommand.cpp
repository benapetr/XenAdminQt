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

#include "changehostpasswordcommand.h"
#include "../../dialogs/changeserverpassworddialog.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"

ChangeHostPasswordCommand::ChangeHostPasswordCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool ChangeHostPasswordCommand::CanRun() const
{
    QSharedPointer<Host> host;
    QSharedPointer<Pool> pool;
    if (!this->getTarget(&host, &pool))
        return false;

    XenConnection* connection = host ? host->GetConnection() : pool->GetConnection();
    if (!connection)
        return false;

    XenAPI::Session* session = connection->GetSession();
    if (!session || !session->IsLocalSuperuser())
        return false;

    if (host)
        return host->IsLive();

    return true;
}

void ChangeHostPasswordCommand::Run()
{
    QSharedPointer<Host> host;
    QSharedPointer<Pool> pool;
    if (!this->getTarget(&host, &pool))
        return;

    if (host)
    {
        ChangeServerPasswordDialog dialog(host, nullptr);
        dialog.exec();
        return;
    }

    if (pool)
    {
        ChangeServerPasswordDialog dialog(pool, nullptr);
        dialog.exec();
    }
}

QString ChangeHostPasswordCommand::MenuText() const
{
    return tr("Change Server Password...");
}

bool ChangeHostPasswordCommand::getTarget(QSharedPointer<Host>* host, QSharedPointer<Pool>* pool) const
{
    const QSharedPointer<XenObject> object = this->getSelectedObject();
    if (!object)
        return false;

    if (object->GetObjectType() == XenObjectType::Host)
    {
        if (host)
            *host = qSharedPointerDynamicCast<Host>(object);
        return host && !host->isNull();
    }

    if (object->GetObjectType() == XenObjectType::Pool)
    {
        if (pool)
            *pool = qSharedPointerDynamicCast<Pool>(object);
        return pool && !pool->isNull();
    }

    return false;
}
