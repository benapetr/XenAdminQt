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

#include "srrefreshaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../../xencache.h"
#include <QVariant>

using namespace XenAPI;

SrRefreshAction::SrRefreshAction(XenConnection* connection, const QString& srRef,
                                 QObject* parent)
    : AsyncOperation(connection, QString("Refreshing Storage Repository"), QString(), parent), m_srRef(srRef)
{
    QString srName = getSRName();
    SetTitle(QString("Refreshing storage repository '%1'").arg(srName));
    SetDescription(QString("Scanning '%1' for changes...").arg(srName));
}

QString SrRefreshAction::getSRName() const
{
    if (!GetConnection())
        return "Storage Repository";

    // Try to get SR name from cache
    if (GetConnection()->GetCache())
    {
        QVariantMap srData = GetConnection()->GetCache()->ResolveObjectData("sr", m_srRef);
        QString name = srData.value("name_label", "").toString();
        if (!name.isEmpty())
            return name;
    }

    return "Storage Repository";
}

void SrRefreshAction::run()
{
    if (!GetConnection() || m_srRef.isEmpty())
    {
        setError("Invalid connection or SR reference");
        return;
    }

    try
    {
        Session* session = GetConnection()->GetSession();
        if (!session)
        {
            throw std::runtime_error("No valid session");
        }

        // Call SR.scan() to refresh the SR
        // C# equivalent: SR.scan(Session, SR.opaque_ref);
        XenAPI::SR::scan(session, m_srRef);

        SetDescription("Completed");
    } catch (const std::exception& e)
    {
        setError(QString("Failed to scan SR: %1").arg(e.what()));
        throw;
    }
}
