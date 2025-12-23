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

#include "pool.h"
#include "network/connection.h"
#include "../xenlib.h"

Pool::Pool(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString Pool::objectType() const
{
    return "pool";
}

QString Pool::masterRef() const
{
    return stringProperty("master");
}

QString Pool::defaultSRRef() const
{
    return stringProperty("default_SR");
}

bool Pool::haEnabled() const
{
    return boolProperty("ha_enabled", false);
}

QVariantMap Pool::haConfiguration() const
{
    return property("ha_configuration").toMap();
}

QVariantMap Pool::otherConfig() const
{
    return property("other_config").toMap();
}

QStringList Pool::hostRefs() const
{
    // In XenAPI, we need to query the cache for all hosts in this pool
    // For now, return empty list - this will be populated when we integrate with cache properly
    // TODO: Query cache for all hosts where host.pool == this->opaqueRef()
    return QStringList();
}

bool Pool::isPoolOfOne() const
{
    return hostRefs().count() == 1;
}

QStringList Pool::tags() const
{
    return stringListProperty("tags");
}

bool Pool::wlbEnabled() const
{
    return boolProperty("wlb_enabled", false);
}

bool Pool::livePatchingDisabled() const
{
    return boolProperty("live_patching_disabled", false);
}
