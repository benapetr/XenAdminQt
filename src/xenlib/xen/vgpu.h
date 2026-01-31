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

#ifndef VGPU_H
#define VGPU_H

#include "xenobject.h"
#include <QString>
#include <QVariantMap>

class VM;
class PCI;
class GPUGroup;

/*!
 * \brief Virtual GPU device wrapper class
 * 
 * Represents a virtual GPU (vGPU) device attached to a VM.
 * Provides access to GPU group, type, physical GPU assignment, and configuration.
 * First published in XenServer 6.0.
 */
class XENLIB_EXPORT VGPU : public XenObject
{
    Q_OBJECT
    
    public:
        explicit VGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        XenObjectType GetObjectType() const override { return XenObjectType::VGPU; }

        // Basic properties
        QString GetVMRef() const;
        QString GetGPUGroupRef() const;
        QString GetDevice() const;
        bool CurrentlyAttached() const;

        // GPU configuration
        QString TypeRef() const;
        QString ResidentOnRef() const;
        QString ScheduledToBeResidentOnRef() const;
        QVariantMap CompatibilityMetadata() const;
        QString ExtraArgs() const;
        QString GetPCIRef() const;

        // Helper methods
        bool IsAttached() const;
        bool IsResident() const;
        bool HasScheduledLocation() const;

        //! Get VM that owns this vGPU
        QSharedPointer<VM> GetVM() const;

        //! Get GPU group this vGPU belongs to
        QSharedPointer<GPUGroup> GetGPUGroup() const;

        //! Get PCI device backing this vGPU
        QSharedPointer<PCI> GetPCI() const;
};

#endif // VGPU_H
