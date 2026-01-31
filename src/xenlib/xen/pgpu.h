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

#ifndef PGPU_H
#define PGPU_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QSharedPointer>

class PCI;
class GPUGroup;
class Host;
class VGPU;

/*!
 * \brief Physical GPU device wrapper class
 * 
 * Represents a physical GPU (pGPU) device on a XenServer host.
 * Provides access to GPU group, supported VGPU types, resident VGPUs, and capabilities.
 * First published in XenServer 6.0.
 */
class PGPU : public XenObject
{
    Q_OBJECT
    
    public:
        explicit PGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        XenObjectType GetObjectType() const override { return XenObjectType::PGPU; }

        // Basic properties
        QString GetPCIRef() const;
        QString GetGPUGroupRef() const;
        QString GetHostRef() const;

        // VGPU type support
        QStringList SupportedVGPUTypeRefs() const;
        QStringList EnabledVGPUTypeRefs() const;
        QStringList ResidentVGPURefs() const;
        QVariantMap SupportedVGPUMaxCapacities() const;

        // Device status
        QString Dom0Access() const;
        bool IsSystemDisplayDevice() const;
        QVariantMap CompatibilityMetadata() const;

        // Helper methods
        bool SupportsVGPUs() const;
        bool HasResidentVGPUs() const;
        int ResidentVGPUCount() const;
        bool IsAccessibleFromDom0() const;

        // Object resolution getters
        QSharedPointer<PCI> GetPCI() const;
        QSharedPointer<GPUGroup> GetGPUGroup() const;
        QSharedPointer<Host> GetHost() const;
        // TODO: Add GetSupportedVGPUTypes() and GetEnabledVGPUTypes() when VGPUType class is ported
        QList<QSharedPointer<VGPU>> GetResidentVGPUs() const;
};

#endif // PGPU_H
