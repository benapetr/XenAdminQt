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

#ifndef VMDESTROYACTION_H
#define VMDESTROYACTION_H

#include "../../asyncoperation.h"
#include "../../vm.h"
#include <QStringList>

/**
 * @brief VMDestroyAction - Destroys a VM and optionally its disks
 *
 * Matches C# XenAdmin.Actions.VMActions.VMDestroyAction
 *
 * This action:
 * 1. Destroys snapshots (if any)
 * 2. Destroys the VM
 * 3. Destroys specified VDIs (disks marked for deletion)
 * 4. Destroys suspend VDI if present
 *
 * C# location: XenModel/Actions/VM/VMDestroyAction.cs
 */
class VMDestroyAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection to use
     * @param vm VM object to destroy
     * @param vbdsToDelete List of VBD refs whose VDIs should be deleted
     * @param snapshotsToDelete List of snapshot VM refs to delete
     * @param parent Parent QObject
     */
    explicit VMDestroyAction(XenConnection* connection,
                             VM* vm,
                             const QStringList& vbdsToDelete,
                             const QStringList& snapshotsToDelete,
                             QObject* parent = nullptr);

    /**
     * @brief Convenience constructor - delete all owner disks
     * @param connection XenConnection to use
     * @param vm VM object to destroy
     * @param deleteAllOwnerDisks If true, delete all VDIs marked as owner
     * @param parent Parent QObject
     */
    explicit VMDestroyAction(XenConnection* connection,
                             VM* vm,
                             bool deleteAllOwnerDisks = false,
                             QObject* parent = nullptr);

protected:
    void run() override;

private:
    /**
     * @brief Internal destroy method (can be called recursively for snapshots)
     * @param vmRef VM opaque reference
     * @param vbdRefsToDelete VBD refs to delete
     * @param snapshotRefsToDelete Snapshot VM refs to delete
     */
    void destroyVM(const QString& vmRef,
                   const QStringList& vbdRefsToDelete,
                   const QStringList& snapshotRefsToDelete);

    VM* m_vm;
    QStringList m_vbdsToDelete;
    QStringList m_snapshotsToDelete;
};

#endif // VMDESTROYACTION_H
