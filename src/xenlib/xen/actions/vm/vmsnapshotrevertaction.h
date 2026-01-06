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

#ifndef VMSNAPSHOTREVERTACTION_H
#define VMSNAPSHOTREVERTACTION_H

#include "../../asyncoperation.h"
#include <QtCore/QString>

/**
 * @brief Action to revert a VM to a snapshot
 *
 * Reverts a VM to a previous snapshot state by:
 * 1. Calling VM.async_revert on the snapshot
 * 2. Optionally restoring the power state if the snapshot was taken while running
 * 3. Starting/resuming the VM on the original host if possible
 *
 * Matches C# VMSnapshotRevertAction
 */
class XENLIB_EXPORT VMSnapshotRevertAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct snapshot revert action
         * @param connection XenConnection
         * @param snapshotRef Snapshot VM opaque_ref to revert to
         * @param parent Parent QObject
         */
        VMSnapshotRevertAction(XenConnection* connection,
                               const QString& snapshotRef,
                               QObject* parent = nullptr);

        QString vmRef() const
        {
            return m_vmRef;
        }

        QString snapshotRef() const
        {
            return m_snapshotRef;
        }

    protected:
        void run() override;

    private:
        /**
         * @brief Restore the VM power state after revert
         * @param vmRef VM reference
         */
        void revertPowerState(const QString& vmRef);

        /**
         * @brief Check if VM can boot on a specific host
         * @param vmRef VM reference
         * @param hostRef Host reference
         * @return true if VM can boot on host
         */
        bool vmCanBootOnHost(const QString& vmRef, const QString& hostRef);

        QString m_snapshotRef;
        QString m_snapshotName;
        QString m_vmRef;           // Parent VM reference
        QString m_previousHostRef; // Host the VM was running on before snapshot
        bool m_revertPowerState;   // Whether to restore power state
        bool m_revertFinished;     // Track when revert phase is complete
};

#endif // VMSNAPSHOTREVERTACTION_H
