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

#ifndef SRINTRODUCEACTION_H
#define SRINTRODUCEACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>

/**
 * @brief SrIntroduceAction - Introduce (attach) an existing Storage Repository
 *
 * Qt equivalent of C# SrIntroduceAction (XenModel/Actions/SR/SrIntroduceAction.cs).
 * Introduces an existing SR to the pool by UUID, creates PBDs for all hosts,
 * and plugs them. Used for attaching existing SRs that were previously detached
 * or moving SRs between pools.
 *
 * From C# implementation:
 * - First performs preemptive SR.forget() in case SR is in broken state
 * - Calls SR.introduce() with existing SR UUID
 * - Creates PBDs for each host in the pool with provided device config
 * - Plugs all PBDs
 * - Sets as default SR if first shared non-ISO SR
 *
 * Use cases:
 * - Reattaching a detached SR
 * - Attaching shared storage to a new pool
 * - Recovering from broken SR state
 */
class XENLIB_EXPORT SrIntroduceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Introduce an existing Storage Repository
         * @param connection XenServer connection
         * @param srUuid UUID of existing SR
         * @param srName SR name
         * @param srDescription SR description
         * @param srType SR type (e.g., "nfs", "lvmoiscsi", "ext")
         * @param srContentType Content type (e.g., "user", "iso")
         * @param deviceConfig Device configuration map for creating PBDs
         * @param parent Parent object
         */
        explicit SrIntroduceAction(XenConnection* connection,
                                   const QString& srUuid,
                                   const QString& srName,
                                   const QString& srDescription,
                                   const QString& srType,
                                   const QString& srContentType,
                                   const QVariantMap& deviceConfig,
                                   QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Check if this is the first shared non-ISO SR in the pool
         * @return true if no other shared non-ISO SRs exist
         *
         * Used to determine whether to set this SR as the default SR.
         * Queries the cache for all existing SRs.
         */
        bool isFirstSharedNonISOSR() const;

        QString m_srUuid;
        QString m_srName;
        QString m_srDescription;
        QString m_srType;
        QString m_srContentType;
        bool m_srIsShared;
        QVariantMap m_deviceConfig;
};

#endif // SRINTRODUCEACTION_H
