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

#ifndef VMSNAPSHOTCREATEACTION_H
#define VMSNAPSHOTCREATEACTION_H

#include "../../asyncoperation.h"
#include <QtCore/QString>
#include <QtGui/QImage>

/**
 * @brief Action to create a VM snapshot
 *
 * Creates a snapshot of a VM using one of three methods:
 * - DISK: Disk-only snapshot (async_snapshot)
 * - DISK_AND_MEMORY: Checkpoint with memory state (async_checkpoint)
 * - QUIESCED_DISK: Quiesced disk snapshot (async_snapshot_with_quiesce)
 *
 * For DISK_AND_MEMORY snapshots of running VMs, captures and stores
 * console screenshot as JPEG blob (C#: VNC_SNAPSHOT const)
 *
 * Matches C# VMSnapshotCreateAction
 */
class XENLIB_EXPORT VMSnapshotCreateAction : public AsyncOperation
{
    Q_OBJECT

public:
    enum SnapshotType
    {
        DISK,
        DISK_AND_MEMORY,
        QUIESCED_DISK
    };

    /**
     * @brief Construct snapshot create action
     * @param connection XenConnection
     * @param vmRef VM opaque_ref to snapshot
     * @param newName Name for the snapshot
     * @param newDescription Description for the snapshot
     * @param type Type of snapshot to create
     * @param screenshot Console screenshot (optional, for DISK_AND_MEMORY)
     * @param parent Parent QObject
     */
    VMSnapshotCreateAction(XenConnection* connection,
                           const QString& vmRef,
                           const QString& newName,
                           const QString& newDescription,
                           SnapshotType type,
                           const QImage& screenshot = QImage(),
                           QObject* parent = nullptr);

    /**
     * @brief Get the opaque_ref of the created snapshot
     * @return Snapshot VM reference (available after completion)
     */
    QString snapshotRef() const
    {
        return m_snapshotRef;
    }

protected:
    void run() override;

private:
    void saveImageInBlob(const QString& snapshotRef, const QImage& image);

    QString m_vmRef;
    QString m_vmName;
    QString m_newName;
    QString m_newDescription;
    SnapshotType m_type;
    QString m_snapshotRef;
    QImage m_screenshot;
};

#endif // VMSNAPSHOTCREATEACTION_H
