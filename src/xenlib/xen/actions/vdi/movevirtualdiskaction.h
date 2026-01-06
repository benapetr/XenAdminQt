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

#ifndef MOVEVIRTUALDISKACTION_H
#define MOVEVIRTUALDISKACTION_H

#include "xen/asyncoperation.h"
#include <QString>
#include <QVariantMap>

class XenConnection;

/**
 * @brief Move a VDI from one SR to another (offline copy)
 *
 * This action performs an offline copy of a VDI to a new SR, recreates all VBDs
 * pointing to the new VDI, and destroys the original VDI. This is used when
 * live migration is not available or not desired.
 *
 * Steps:
 * 1. Copy VDI to target SR (VDI.async_copy)
 * 2. Update suspend_VDI reference if this is a suspend VDI
 * 3. Recreate all VBDs pointing to the new VDI
 * 4. Destroy original VDI
 *
 * C# reference: XenModel/Actions/VDI/MoveVirtualDiskAction.cs
 */
class MoveVirtualDiskAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a move VDI action
         * @param connection The connection to the XenServer
         * @param vdiRef The VDI to move
         * @param srRef The target SR
         * @param parent Parent QObject
         */
        MoveVirtualDiskAction(XenConnection* connection, const QString& vdiRef,
                              const QString& srRef, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vdiRef;
        QString m_srRef;

        QString getVDIName() const;
        QString getSRName(const QString& srRef) const;
};

#endif // MOVEVIRTUALDISKACTION_H
