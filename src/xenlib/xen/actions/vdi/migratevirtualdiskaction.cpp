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

#include "migratevirtualdiskaction.h"
#include "xen/network/connection.h"
#include "xen/xenapi/xenapi_VDI.h"
#include "xen/xenapi/xenapi_SR.h"
#include <QDebug>

MigrateVirtualDiskAction::MigrateVirtualDiskAction(XenConnection* connection,
                                                   const QString& vdiRef,
                                                   const QString& srRef,
                                                   QObject* parent)
    : AsyncOperation(connection, QString("Migrating Virtual Disk"), QString(), parent), m_vdiRef(vdiRef), m_srRef(srRef)
{
    QString vdiName = getVDIName();
    QString oldSR = getSRName(QString()); // Get current SR from VDI
    QString newSR = getSRName(srRef);

    SetTitle(QString("Migrating Virtual Disk"));
    SetDescription(QString("Migrating '%1' from '%2' to '%3'...")
                       .arg(vdiName)
                       .arg(oldSR)
                       .arg(newSR));
}

void MigrateVirtualDiskAction::run()
{
    try
    {
        XenAPI::Session* session = GetConnection()->GetSession();
        if (!session)
        {
            throw std::runtime_error("No valid session");
        }

        qDebug() << "Live migrating VDI" << m_vdiRef << "to SR" << m_srRef;

        // Perform live migration using VDI.pool_migrate
        SetDescription(QString("Migrating '%1'...").arg(getVDIName()));

        // Empty options map (can be used for advanced migration options)
        QVariantMap options;

        QString taskRef = XenAPI::VDI::async_pool_migrate(session, m_vdiRef, m_srRef, options);
        if (taskRef.isEmpty())
        {
            throw std::runtime_error("Failed to start VDI migration task");
        }

        // Poll migration task to completion
        pollToCompletion(taskRef, 0, 100);

        SetDescription(QString("'%1' migrated successfully").arg(getVDIName()));
        SetPercentComplete(100);

        qDebug() << "VDI migrated successfully";

    } catch (const std::exception& e)
    {
        qWarning() << "MigrateVirtualDiskAction failed:" << e.what();
        throw;
    }
}

QString MigrateVirtualDiskAction::getVDIName() const
{
    try
    {
        XenAPI::Session* session = GetConnection()->GetSession();
        QString name = XenAPI::VDI::get_name_label(session, m_vdiRef);
        return name.isEmpty() ? m_vdiRef : name;
    } catch (...)
    {
        return m_vdiRef;
    }
}

QString MigrateVirtualDiskAction::getSRName(const QString& srRef) const
{
    try
    {
        XenAPI::Session* session = GetConnection()->GetSession();
        QString actualSrRef = srRef;

        // If no SR ref provided, get it from the VDI
        if (actualSrRef.isEmpty())
        {
            actualSrRef = XenAPI::VDI::get_SR(session, m_vdiRef);
        }

        QString name = XenAPI::SR::get_name_label(session, actualSrRef);
        return name.isEmpty() ? actualSrRef : name;
    } catch (...)
    {
        return srRef.isEmpty() ? "Unknown" : srRef;
    }
}
