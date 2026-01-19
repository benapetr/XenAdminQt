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

#include "vgpuconfigurationaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_PGPU.h"
#include "../../pgpu.h"
#include <QDebug>
#include <stdexcept>

VgpuConfigurationAction::VgpuConfigurationAction(const QMap<QString, QStringList>& updatedEnabledVGpuListByPGpu,
                                                 XenConnection* connection,
                                                 QObject* parent)
    : AsyncOperation(tr("Configuring VGPU Types"),
                     tr("Configuring enabled VGPU types for physical GPUs..."),
                     parent),
      m_updatedEnabledVGpuListByPGpu(updatedEnabledVGpuListByPGpu)
{
    this->m_connection = connection;

    // Register API method for RBAC checks
    AddApiMethodToRoleCheck("PGPU.set_enabled_VGPU_types");
}

void VgpuConfigurationAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session)
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    if (this->m_updatedEnabledVGpuListByPGpu.isEmpty())
    {
        SetPercentComplete(100);
        return;
    }

    try
    {
        int totalPGpus = this->m_updatedEnabledVGpuListByPGpu.size();
        int processedCount = 0;

        // Iterate through each PGPU and set its enabled VGPU types
        for (auto it = this->m_updatedEnabledVGpuListByPGpu.constBegin();
             it != this->m_updatedEnabledVGpuListByPGpu.constEnd();
             ++it)
        {
            const QString& pgpuRef = it.key();
            const QStringList& enabledVGpuTypeRefs = it.value();

            // Call PGPU.set_enabled_VGPU_types API
            XenAPI::PGPU::set_enabled_VGPU_types(session, pgpuRef, enabledVGpuTypeRefs);

            // Update progress
            processedCount++;
            int percent = (processedCount * 100) / totalPGpus;
            SetPercentComplete(percent);
        }

        SetPercentComplete(100);

    } catch (const std::exception& e)
    {
        throw; // Re-throw for AsyncOperation error handling
    }
}
