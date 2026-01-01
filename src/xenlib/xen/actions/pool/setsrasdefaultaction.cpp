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

#include "setsrasdefaultaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"

SetSrAsDefaultAction::SetSrAsDefaultAction(XenConnection* connection,
                                           const QString& poolRef,
                                           const QString& srRef,
                                           QObject* parent)
    : AsyncOperation(connection,
                     QString("Setting default storage repository"),
                     QString("Updating default SR"),
                     parent),
      m_poolRef(poolRef),
      m_srRef(srRef)
{
}

void SetSrAsDefaultAction::run()
{
    if (!connection() || m_poolRef.isEmpty() || m_srRef.isEmpty())
    {
        setError("Invalid connection or references");
        return;
    }

    XenAPI::Session* session = connection()->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        setError("No valid session");
        return;
    }

    try
    {
        XenAPI::Pool::set_default_SR(session, m_poolRef, m_srRef);
        XenAPI::Pool::set_suspend_image_SR(session, m_poolRef, m_srRef);
        XenAPI::Pool::set_crash_dump_SR(session, m_poolRef, m_srRef);
        setDescription("Completed");
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to set default SR: %1").arg(e.what()));
        throw;
    }
}
