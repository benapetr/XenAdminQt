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

#ifndef VGPUTYPE_H
#define VGPUTYPE_H

#include "xenobject.h"

class PGPU;

/**
 * @brief VGPU_type object wrapper
 *
 * Represents the XenAPI `vgpu_type` object and exposes the properties needed by
 * GPU-related UI and actions.
 */
class XENLIB_EXPORT VGPUType : public XenObject
{
    Q_OBJECT

    public:
        explicit VGPUType(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VGPUType() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::VGPUType; }

        QString VendorName() const;
        QString ModelName() const;
        qint64 FramebufferSize() const;
        qint64 MaxHeads() const;
        qint64 MaxResolutionX() const;
        qint64 MaxResolutionY() const;
        QString Implementation() const;
        QString Identifier() const;
        bool IsExperimental() const;

        QStringList SupportedOnPGPURefs() const;
        QStringList EnabledOnPGPURefs() const;
        QStringList SupportedOnGPUGroupRefs() const;
        QStringList EnabledOnGPUGroupRefs() const;
        QStringList CompatibleTypesInVmRefs() const;

        bool IsPassthrough() const;
        qint64 Capacity() const;
        QString DisplayName() const;
        QString DisplayDescription() const;
};

#endif // VGPUTYPE_H
