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

#ifndef POOLPATCH_H
#define POOLPATCH_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QMap>

/**
 * @brief PoolPatch - Pool-wide patches
 *
 * Qt equivalent of C# Pool_patch class.
 * First published in XenServer 4.1.
 */
class XENLIB_EXPORT PoolPatch : public XenObject
{
    Q_OBJECT

    public:
        explicit PoolPatch(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~PoolPatch();

        QString GetObjectType() const override { return "pool_patch"; }

        // Property accessors
        QString Version() const;
        qint64 Size() const;
        bool PoolApplied() const;
        QStringList HostPatchRefs() const;
        QStringList AfterApplyGuidance() const;
        QString PoolUpdateRef() const;
        QMap<QString, QString> OtherConfig() const;
};

#endif // POOLPATCH_H
