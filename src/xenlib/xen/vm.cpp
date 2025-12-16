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

#include "vm.h"
#include "connection.h"
#include "../xenlib.h"

VM::VM(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VM::powerState() const
{
    return stringProperty("power_state");
}

bool VM::isTemplate() const
{
    return boolProperty("is_a_template", false);
}

bool VM::isDefaultTemplate() const
{
    return boolProperty("is_default_template", false);
}

bool VM::isSnapshot() const
{
    return boolProperty("is_a_snapshot", false);
}

QString VM::residentOnRef() const
{
    return stringProperty("resident_on");
}

QString VM::affinityRef() const
{
    return stringProperty("affinity");
}

QStringList VM::vbdRefs() const
{
    return stringListProperty("VBDs");
}

QStringList VM::vifRefs() const
{
    return stringListProperty("VIFs");
}

QStringList VM::consoleRefs() const
{
    return stringListProperty("consoles");
}

QString VM::snapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

QStringList VM::snapshotRefs() const
{
    return stringListProperty("snapshots");
}

QString VM::suspendVDIRef() const
{
    return stringProperty("suspend_VDI");
}

qint64 VM::memoryTarget() const
{
    return longProperty("memory_target", 0);
}

qint64 VM::memoryStaticMax() const
{
    return longProperty("memory_static_max", 0);
}

qint64 VM::memoryDynamicMax() const
{
    return longProperty("memory_dynamic_max", 0);
}

qint64 VM::memoryDynamicMin() const
{
    return longProperty("memory_dynamic_min", 0);
}

qint64 VM::memoryStaticMin() const
{
    return longProperty("memory_static_min", 0);
}

int VM::vcpusMax() const
{
    return intProperty("VCPUs_max", 0);
}

int VM::vcpusAtStartup() const
{
    return intProperty("VCPUs_at_startup", 0);
}

QVariantMap VM::otherConfig() const
{
    return property("other_config").toMap();
}

QVariantMap VM::platform() const
{
    return property("platform").toMap();
}

QStringList VM::tags() const
{
    return stringListProperty("tags");
}

QStringList VM::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap VM::currentOperations() const
{
    return property("current_operations").toMap();
}

QString VM::homeRef() const
{
    // Return affinity host if set, otherwise resident host
    QString affinity = affinityRef();
    if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
        return affinity;

    return residentOnRef();
}
