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

#include "sm.h"
#include "network/connection.h"
#include <QMap>

SM::SM(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

SM::~SM()
{
}

QString SM::NameLabel() const
{
    return this->stringProperty("name_label");
}

QString SM::NameDescription() const
{
    return this->stringProperty("name_description");
}

QString SM::Type() const
{
    return this->stringProperty("type");
}

QString SM::Vendor() const
{
    return this->stringProperty("vendor");
}

QString SM::Copyright() const
{
    return this->stringProperty("copyright");
}

QString SM::Version() const
{
    return this->stringProperty("version");
}

QString SM::RequiredApiVersion() const
{
    return this->stringProperty("required_api_version");
}

QMap<QString, QString> SM::Configuration() const
{
    QVariantMap map = this->property("configuration").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}

QStringList SM::Capabilities() const
{
    return this->property("capabilities").toStringList();
}

QMap<QString, qint64> SM::Features() const
{
    QVariantMap map = this->property("features").toMap();
    QMap<QString, qint64> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toLongLong();
    return result;
}

QMap<QString, QString> SM::OtherConfig() const
{
    QVariantMap map = this->property("other_config").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}

QString SM::DriverFilename() const
{
    return this->stringProperty("driver_filename");
}

QStringList SM::RequiredClusterStack() const
{
    return this->property("required_cluster_stack").toStringList();
}
