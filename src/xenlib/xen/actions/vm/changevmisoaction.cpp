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

#include "changevmisoaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../api.h"
#include <QDebug>

ChangeVMISOAction::ChangeVMISOAction(XenConnection* connection,
                                     const QString& vmRef,
                                     const QString& vdiRef,
                                     const QString& vbdRef,
                                     QObject* parent)
    : AsyncOperation(connection,
                     vdiRef.isEmpty() ? "Ejecting ISO" : "Loading ISO",
                     vdiRef.isEmpty() ? "Unloading ISO from VM" : "Loading ISO into VM",
                     parent),
      m_vmRef(vmRef), m_vdiRef(vdiRef), m_vbdRef(vbdRef), m_isEmpty(false)
{
    if (m_vmRef.isEmpty())
        qWarning() << "ChangeVMISOAction: VM reference is empty";

    if (m_vbdRef.isEmpty())
        qWarning() << "ChangeVMISOAction: VBD reference is empty";

    // Add RBAC methods to check
    // Check if we need to eject first by getting VBD status
    // For now, we'll always try to eject if VBD is not empty
    addApiMethodToRoleCheck("VBD.eject");

    if (!m_vdiRef.isEmpty())
        addApiMethodToRoleCheck("VBD.insert");
}

ChangeVMISOAction::~ChangeVMISOAction()
{
}

void ChangeVMISOAction::run()
{
    if (m_vbdRef.isEmpty())
    {
        setError("Invalid VBD reference");
        return;
    }

    XenSession* session = this->session();
    if (!session || !session->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Get the VBD record to check if it's empty
    QVariantList params;
    params << session->getSessionId() << m_vbdRef;

    XenRpcAPI api(session);
    QByteArray jsonRequest = api.buildJsonRpcCall("VBD.get_record", params);
    QByteArray response = session->sendApiRequest(jsonRequest);

    if (response.isEmpty())
    {
        setError("Failed to get VBD record");
        return;
    }

    QVariant vbdData = api.parseJsonRpcResponse(response);
    QVariantMap vbdRecord = vbdData.toMap();
    m_isEmpty = vbdRecord.value("empty", true).toBool();

    // Step 1: Eject if not empty
    if (!m_isEmpty)
    {
        setDescription("Ejecting current ISO...");

        params.clear();
        params << session->getSessionId() << m_vbdRef;

        // Use async eject
        QByteArray jsonRequest = api.buildJsonRpcCall("VBD.async_eject", params);
        response = session->sendApiRequest(jsonRequest);

        if (response.isEmpty())
        {
            setError("Failed to eject ISO");
            return;
        }

        QVariant taskRef = api.parseJsonRpcResponse(response);
        QString taskRefStr = taskRef.toString();

        if (taskRefStr.isEmpty())
        {
            setError("Failed to get task reference for eject");
            return;
        }

        setRelatedTaskRef(taskRefStr);

        // Poll task from 0% to 50% if we're going to insert after
        pollToCompletion(taskRefStr, 0, m_vdiRef.isEmpty() ? 100 : 50);

        // Check if task succeeded
        if (hasError())
            return;
    }

    // Step 2: Insert new ISO if specified
    if (!m_vdiRef.isEmpty())
    {
        setDescription("Inserting ISO...");

        params.clear();
        params << session->getSessionId() << m_vbdRef << m_vdiRef;

        // Use async insert
        QByteArray jsonRequest = api.buildJsonRpcCall("VBD.async_insert", params);
        response = session->sendApiRequest(jsonRequest);

        if (response.isEmpty())
        {
            setError("Failed to insert ISO");
            return;
        }

        QVariant taskRef = api.parseJsonRpcResponse(response);
        QString taskRefStr = taskRef.toString();

        if (taskRefStr.isEmpty())
        {
            setError("Failed to get task reference for insert");
            return;
        }

        setRelatedTaskRef(taskRefStr);

        // Poll task from 50% (or 0% if we didn't eject) to 100%
        pollToCompletion(taskRefStr, m_isEmpty ? 0 : 50, 100);

        // Check if task succeeded
        if (hasError())
            return;

        setDescription("ISO loaded successfully");
    } else
    {
        setDescription("ISO ejected successfully");
    }

    setPercentComplete(100);
}
