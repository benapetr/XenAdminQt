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

#include "network.h"

Network::Network(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString Network::bridge() const
{
    return this->data().value("bridge").toString();
}

bool Network::managed() const
{
    // Default to true (managed by xapi) if not specified
    return this->data().value("managed", true).toBool();
}

qint64 Network::mtu() const
{
    return this->data().value("MTU", 1500).toLongLong();
}

QStringList Network::vifRefs() const
{
    QVariantList vifs = this->data().value("VIFs").toList();
    QStringList result;
    for (const QVariant& vif : vifs)
    {
        result.append(vif.toString());
    }
    return result;
}

QStringList Network::pifRefs() const
{
    QVariantList pifs = this->data().value("PIFs").toList();
    QStringList result;
    for (const QVariant& pif : pifs)
    {
        result.append(pif.toString());
    }
    return result;
}

QVariantMap Network::otherConfig() const
{
    return this->data().value("other_config").toMap();
}

QStringList Network::allowedOperations() const
{
    QVariantList ops = this->data().value("allowed_operations").toList();
    QStringList result;
    for (const QVariant& op : ops)
    {
        result.append(op.toString());
    }
    return result;
}

QVariantMap Network::currentOperations() const
{
    return this->data().value("current_operations").toMap();
}

QVariantMap Network::blobs() const
{
    return this->data().value("blobs").toMap();
}

QStringList Network::tags() const
{
    QVariantList tagList = this->data().value("tags").toList();
    QStringList result;
    for (const QVariant& tag : tagList)
    {
        result.append(tag.toString());
    }
    return result;
}

QString Network::defaultLockingMode() const
{
    return this->data().value("default_locking_mode", "unlocked").toString();
}

QVariantMap Network::assignedIPs() const
{
    return this->data().value("assigned_ips").toMap();
}

QStringList Network::purpose() const
{
    QVariantList purposeList = this->data().value("purpose").toList();
    QStringList result;
    for (const QVariant& p : purposeList)
    {
        result.append(p.toString());
    }
    return result;
}
