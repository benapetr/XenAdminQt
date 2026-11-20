/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNPLUGPLUGNETWORKACTION_H
#define UNPLUGPLUGNETWORKACTION_H

#include "../../asyncoperation.h"
#include <QSharedPointer>
#include <QStringList>

class Network;

/**
 * @brief Unplugs and replugs a network's PIFs and the VIFs of its running VMs
 *
 * Used after an MTU change so that it takes effect. Errors are noted but the
 * action carries on, so as much as possible is plugged back; the first error is
 * thrown at the end.
 *
 * C# path: XenModel/Actions/Network/UnplugPlugNetworkAction.cs
 */
class XENLIB_EXPORT UnplugPlugNetworkAction : public AsyncOperation
{
    Q_OBJECT

    public:
        UnplugPlugNetworkAction(QSharedPointer<Network> network, bool suppressHistory, QObject* parent = nullptr);

        //! PIFs and VIFs the action will replug (C#: the attached PIFs, VIFs of running VMs)
        QStringList GetPIFRefs() const { return this->m_pifRefs; }
        QStringList GetVIFRefs() const { return this->m_vifRefs; }

    protected:
        void run() override;

    private:
        QSharedPointer<Network> m_network;
        QStringList m_pifRefs;
        QStringList m_vifRefs;
};

#endif // UNPLUGPLUGNETWORKACTION_H
