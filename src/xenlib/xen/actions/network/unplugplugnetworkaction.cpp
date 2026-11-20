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

#include "unplugplugnetworkaction.h"
#include "../../network.h"
#include "../../pif.h"
#include "../../vif.h"
#include "../../vm.h"
#include "../../network/connection.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../xenapi/xenapi_VIF.h"
#include "../../../xencache.h"
#include <QtCore/QScopeGuard>
#include <QtCore/QDebug>
#include <exception>
#include <functional>
#include <stdexcept>

namespace
{
    bool vmRunning(const QSharedPointer<VIF>& vif)
    {
        QSharedPointer<VM> vm = vif ? vif->GetVM() : QSharedPointer<VM>();
        return vm && vm->GetPowerState() == QLatin1String("Running");
    }
}

UnplugPlugNetworkAction::UnplugPlugNetworkAction(QSharedPointer<Network> network, bool suppressHistory, QObject* parent)
    : AsyncOperation(network ? network->GetConnection() : nullptr,
                     tr("Unplugging and replugging network"),
                     tr("Unplugging and replugging the network's interfaces"),
                     suppressHistory, parent),
      m_network(network)
{
    if (!network)
        throw std::invalid_argument("Network cannot be null");

    for (const QSharedPointer<PIF>& pif : network->GetPIFs())
    {
        if (pif->IsCurrentlyAttached())
            this->m_pifRefs.append(pif->OpaqueRef());
    }

    // We only replug running VMs
    for (const QSharedPointer<VIF>& vif : network->GetVIFs())
    {
        if (vif->IsCurrentlyAttached() && vmRunning(vif))
            this->m_vifRefs.append(vif->OpaqueRef());
    }

    this->AddApiMethodToRoleCheck("pif.async_unplug");
    this->AddApiMethodToRoleCheck("pif.async_plug");
    this->AddApiMethodToRoleCheck("vif.async_plug");
    this->AddApiMethodToRoleCheck("vif.async_unplug");

    if (XenCache* cache = network->GetCache())
        this->SetPool(cache->GetPool());
}

void UnplugPlugNetworkAction::run()
{
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    if (!cache)
        throw std::runtime_error("No connection");

    QList<QSharedPointer<PIF>> pifs;
    for (const QString& ref : this->m_pifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(ref);
        if (pif && pif->IsValid())
            pifs.append(pif);
    }
    QList<QSharedPointer<VIF>> vifs;
    for (const QString& ref : this->m_vifRefs)
    {
        QSharedPointer<VIF> vif = cache->ResolveObject<VIF>(ref);
        if (vif && vif->IsValid())
            vifs.append(vif);
    }

    this->m_network->Lock();
    for (const QSharedPointer<PIF>& pif : pifs)
        pif->Lock();
    for (const QSharedPointer<VIF>& vif : vifs)
        vif->Lock();
    const auto unlock = qScopeGuard([this, &pifs, &vifs]()
    {
        this->m_network->Unlock();
        for (const QSharedPointer<PIF>& pif : pifs)
            pif->Unlock();
        for (const QSharedPointer<VIF>& vif : vifs)
            vif->Unlock();
    });

    // Note errors but carry on, so that as much as possible is plugged back
    std::exception_ptr error;
    const int steps = (pifs.size() + vifs.size()) * 2;
    const int step = steps > 0 ? 90 / steps : 0;
    int percent = 10;
    this->SetPercentComplete(percent);

    auto attempt = [&error, &percent, step, this](const char* what, const QString& ref, const std::function<void()>& call)
    {
        try
        {
            call();
        } catch (const std::exception& e)
        {
            qWarning() << "UnplugPlugNetworkAction:" << what << ref << "failed:" << e.what();
            if (!error)
                error = std::current_exception();
        }
        percent += step;
        this->SetPercentComplete(percent);
    };

    XenAPI::Session* session = this->GetSession();

    // VIFs of VMs that are no longer running are skipped (C# CA-75969: the
    // attached state is not checked, the call is attempted anyway)
    for (const QSharedPointer<VIF>& vif : vifs)
        attempt("unplug VIF", vif->OpaqueRef(), [&]() { if (vmRunning(vif)) XenAPI::VIF::unplug(session, vif->OpaqueRef()); });
    for (const QSharedPointer<PIF>& pif : pifs)
        attempt("unplug PIF", pif->OpaqueRef(), [&]() { XenAPI::PIF::unplug(session, pif->OpaqueRef()); });
    for (const QSharedPointer<PIF>& pif : pifs)
        attempt("plug PIF", pif->OpaqueRef(), [&]() { XenAPI::PIF::plug(session, pif->OpaqueRef()); });
    for (const QSharedPointer<VIF>& vif : vifs)
        attempt("plug VIF", vif->OpaqueRef(), [&]() { if (vmRunning(vif)) XenAPI::VIF::plug(session, vif->OpaqueRef()); });

    this->SetPercentComplete(100);
    this->SetDescription(error ? tr("Completed with errors") : tr("Completed"));

    if (error)
        std::rethrow_exception(error);
}
