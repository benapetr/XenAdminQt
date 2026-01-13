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

#ifndef HOSTCPU_H
#define HOSTCPU_H

#include "xenobject.h"
#include <QString>
#include <QMap>
#include <QSharedPointer>

class Host;

/**
 * @brief HostCPU - A physical CPU
 *
 * Qt equivalent of C# Host_cpu class.
 * First published in XenServer 4.0.
 */
class XENLIB_EXPORT HostCPU : public XenObject
{
    Q_OBJECT

    public:
        explicit HostCPU(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~HostCPU();

        QString GetObjectType() const override { return "host_cpu"; }

        // Property accessors
        QString GetHostRef() const;
        qint64 Number() const;
        QString Vendor() const;
        qint64 Speed() const;
        QString ModelName() const;
        qint64 Family() const;
        qint64 Model() const;
        QString Stepping() const;
        QString Flags() const;
        QString Features() const;
        double Utilisation() const;

        // Object resolution getters
        QSharedPointer<Host> GetHost() const;
};

#endif // HOSTCPU_H
