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

#include "hostcpu.h"
#include "network/connection.h"
#include <QMap>

HostCPU::HostCPU(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

HostCPU::~HostCPU()
{
}

QString HostCPU::HostRef() const
{
    return this->stringProperty("host");
}

qint64 HostCPU::Number() const
{
    return this->longProperty("number", 0);
}

QString HostCPU::Vendor() const
{
    return this->stringProperty("vendor");
}

qint64 HostCPU::Speed() const
{
    return this->longProperty("speed", 0);
}

QString HostCPU::ModelName() const
{
    return this->stringProperty("modelname");
}

qint64 HostCPU::Family() const
{
    return this->longProperty("family", 0);
}

qint64 HostCPU::Model() const
{
    return this->longProperty("model", 0);
}

QString HostCPU::Stepping() const
{
    return this->stringProperty("stepping");
}

QString HostCPU::Flags() const
{
    return this->stringProperty("flags");
}

QString HostCPU::Features() const
{
    return this->stringProperty("features");
}

double HostCPU::Utilisation() const
{
    return this->property("utilisation").toDouble();
}

QMap<QString, QString> HostCPU::OtherConfig() const
{
    QVariantMap map = this->property("other_config").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}
