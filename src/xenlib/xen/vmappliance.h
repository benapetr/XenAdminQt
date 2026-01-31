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

#ifndef VMAPPLIANCE_H
#define VMAPPLIANCE_H

#include "xenobject.h"

class VM;

/**
 * @brief VM appliance - Group of VMs with coordinated start/shutdown
 *
 * Qt equivalent of C# XenAPI.VM_appliance class.
 * VM appliances allow grouping VMs for coordinated operations like startup order.
 *
 * C# equivalent: xenadmin/XenModel/XenAPI/VM_appliance.cs
 *                xenadmin/XenModel/XenAPI-Extensions/VM_appliance.cs
 *
 * First published in XenServer 6.0.
 */
class XENLIB_EXPORT VMAppliance : public XenObject
{
    Q_OBJECT

    public:
        explicit VMAppliance(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~VMAppliance() override = default;

        // XenObject overrides
        XenObjectType GetObjectType() const override { return XenObjectType::VMAppliance; }

        // Core properties
        QStringList VMRefs() const;
        QStringList AllowedOperations() const;
        
        // Extension methods (from VM_appliance.cs extensions)
        
        //! @brief Get VMs that share storage with VMs in this appliance
        //!
        //! Finds VMs not in this appliance that share SRs with VMs in the appliance.
        //! These are "fate sharing" VMs that could be affected by operations on the appliance.
        //!
        //! @return List of VM opaque refs
        QStringList GetFateSharingVMs() const;

        //! @brief Check if the appliance is running
        //!
        //! Returns true if any VM in the appliance is in Running, Paused, or Suspended state.
        //!
        //! @return true if at least one VM is running
        bool IsRunning() const;
};

#endif // VMAPPLIANCE_H
