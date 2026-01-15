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

#ifndef WLBRETRIEVEVMRECOMMENDATIONSACTION_H
#define WLBRETRIEVEVMRECOMMENDATIONSACTION_H

#include "../../asyncoperation.h"
#include <QMap>
#include <QStringList>

class VM;
class Host;
class XenConnection;

/**
 * @brief Action to retrieve WLB (Workload Balancing) recommendations for VMs
 * 
 * This action calls the XenAPI VM.retrieve_wlb_recommendations method for each VM
 * to get placement recommendations from the WLB server. The recommendations include
 * star ratings (0.0-5.0) and reasons why VMs can or cannot run on specific hosts.
 */
class WlbRetrieveVmRecommendationsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new WLB retrieve recommendations action
         * @param connection The XenServer connection
         * @param vms List of VMs to retrieve recommendations for
         * @param parent Optional parent QObject
         */
        explicit WlbRetrieveVmRecommendationsAction(XenConnection* connection, const QList<VM*>& vms, QObject* parent = nullptr);

        /**
     * @brief Get the WLB recommendations
     * @return Map of VM -> (Map of Host -> recommendation string array)
     *
     * String array format: ["WLB", "star_rating"] or ["WLB", "0.0", "reason"] or [error_code, detail, detail]
     */
    QMap<VM*, QMap<Host*, QStringList>> GetRecommendations() const;

    protected:
        /**
         * @brief Execute the action - retrieve WLB recommendations for each VM
         */
        void run() override;

    private:
        QList<VM*> m_vms;
        QMap<VM*, QMap<Host*, QStringList>> m_recommendations;
};

#endif // WLBRETRIEVEVMRECOMMENDATIONSACTION_H
