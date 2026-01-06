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

#ifndef SRREATTACHACTION_H
#define SRREATTACHACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>
#include <QSharedPointer>

class SR;

/**
 * @brief SrReattachAction - Reattach (reconfigure) an existing Storage Repository
 *
 * Qt equivalent of C# SrReattachAction (XenModel/Actions/SR/SrReattachAction.cs).
 * Reattaches an existing SR by creating new PBDs with updated device configuration
 * and plugging them. Used for changing SR connection parameters (e.g., NFS server path,
 * iSCSI target, etc.).
 *
 * From C# implementation:
 * - Creates new PBDs for each host with updated device config
 * - Plugs all PBDs
 * - Updates SR name and description
 *
 * Difference from SrIntroduceAction:
 * - SrIntroduceAction: Introduces a completely new/foreign SR by UUID
 * - SrReattachAction: Reconfigures an existing known SR with new connection params
 *
 * Use cases:
 * - Changing NFS server IP or export path
 * - Updating iSCSI target parameters
 * - Repairing broken SR connections
 * - Migrating SR storage backend
 */
class XENLIB_EXPORT SrReattachAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Reattach an existing Storage Repository
         * @param sr Existing SR to reattach
         * @param name New SR name
         * @param description New SR description
         * @param deviceConfig New device configuration map
         * @param parent Parent object
         */
        explicit SrReattachAction(QSharedPointer<SR> sr,
                                  const QString& name,
                                  const QString& description,
                                  const QVariantMap& deviceConfig,
                                  QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<SR> m_sr;
        QString m_name;
        QString m_description;
        QVariantMap m_deviceConfig;
};

#endif // SRREATTACHACTION_H
