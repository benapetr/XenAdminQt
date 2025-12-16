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

#ifndef VMCROSSPOOLMIGRATEACTION_H
#define VMCROSSPOOLMIGRATEACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QVariantMap>

/**
 * @brief Action to migrate or copy a VM across pools
 *
 * Performs cross-pool VM migration or copy using:
 * 1. Host.migrate_receive on destination to prepare
 * 2. VM.async_migrate_send with storage/network mappings
 *
 * Supports both migration (move) and copy operations.
 *
 * Equivalent to C# XenAdmin VMCrossPoolMigrateAction.
 */
class VMCrossPoolMigrateAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct cross-pool migration/copy action
     * @param connection Source XenServer connection
     * @param destConnection Destination XenServer connection
     * @param vmRef VM opaque reference (source)
     * @param destinationHostRef Destination host opaque reference
     * @param transferNetworkRef Network opaque reference for data transfer (destination)
     * @param vdiMap VDI to SR mapping (source VDI ref -> destination SR ref)
     * @param vifMap VIF to Network mapping (VIF MAC -> destination network ref)
     * @param copy true for copy operation, false for migrate
     * @param parent Parent QObject
     */
    explicit VMCrossPoolMigrateAction(XenConnection* connection,
                                      XenConnection* destConnection,
                                      const QString& vmRef,
                                      const QString& destinationHostRef,
                                      const QString& transferNetworkRef,
                                      const QVariantMap& vdiMap,
                                      const QVariantMap& vifMap,
                                      bool copy = false,
                                      QObject* parent = nullptr);

protected:
    void run() override;

private:
    XenConnection* m_destConnection;
    QString m_vmRef;
    QString m_destinationHostRef;
    QString m_transferNetworkRef;
    QVariantMap m_vdiMap; // VDI ref -> SR ref
    QVariantMap m_vifMap; // VIF MAC -> Network ref
    bool m_copy;

    QVariantMap convertVifMapToRefs(const QVariantMap& macToNetworkMap);
};

#endif // VMCROSSPOOLMIGRATEACTION_H
