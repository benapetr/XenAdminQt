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

#ifndef VMSNAPSHOTDELETEACTION_H
#define VMSNAPSHOTDELETEACTION_H

#include "../../asyncoperation.h"
#include <QtCore/QString>
#include <QtCore/QStringList>

/**
 * @brief Action to delete a VM snapshot
 *
 * Deletes a VM snapshot by:
 * 1. Hard shutting down the snapshot if it's suspended
 * 2. Destroying owned VBDs (disks that belong to the snapshot)
 * 3. Destroying the snapshot VM itself
 *
 * Matches C# VMSnapshotDeleteAction (inherits from VMDestroyAction)
 */
class XENLIB_EXPORT VMSnapshotDeleteAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct snapshot delete action
     * @param connection XenConnection
     * @param snapshotRef Snapshot VM opaque_ref to delete
     * @param parent Parent QObject
     */
    VMSnapshotDeleteAction(XenConnection* connection,
                           const QString& snapshotRef,
                           QObject* parent = nullptr);

protected:
    void run() override;

private:
    QString m_snapshotRef;
    QString m_snapshotName;
    QStringList m_vbdsToDestroy; // VBDs owned by the snapshot
};

#endif // VMSNAPSHOTDELETEACTION_H
