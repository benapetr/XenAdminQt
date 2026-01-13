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

#include "pbd.h"
#include "host.h"
#include "sr.h"
#include "network/connection.h"
#include "../xencache.h"

PBD::PBD(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString PBD::GetObjectType() const
{
    return "pbd";
}

QString PBD::GetHostRef() const
{
    return this->stringProperty("host");
}

QSharedPointer<Host> PBD::GetHost()
{
    QString host_ref = this->GetHostRef();
    if (!this->GetConnection() || host_ref.isEmpty() || host_ref == XENOBJECT_NULL)
        return QSharedPointer<Host>();

    return this->GetCache()->ResolveObject<Host>("host", host_ref);
}

QString PBD::GetSRRef() const
{
    return this->stringProperty("SR");
}

QSharedPointer<SR> PBD::GetSR()
{
    QString sr_ref = this->GetSRRef();
    if (!this->GetConnection() || sr_ref.isEmpty() || sr_ref == XENOBJECT_NULL)
        return QSharedPointer<SR>();

    return this->GetCache()->ResolveObject<SR>("sr", sr_ref);
}

QVariantMap PBD::DeviceConfig() const
{
    return this->property("device_config").toMap();
}

bool PBD::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached", false);
}

QString PBD::GetDeviceConfigValue(const QString& key) const
{
    QVariantMap config = this->DeviceConfig();
    return config.value(key, QString()).toString();
}

QString PBD::GetOtherConfigValue(const QString& key) const
{
    QVariantMap config = this->GetOtherConfig();
    return config.value(key, QString()).toString();
}

bool PBD::HasDeviceConfigKey(const QString& key) const
{
    QVariantMap config = this->DeviceConfig();
    return config.contains(key);
}

bool PBD::HasOtherConfigKey(const QString& key) const
{
    QVariantMap config = this->GetOtherConfig();
    return config.contains(key);
}
