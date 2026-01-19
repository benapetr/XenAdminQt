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

#ifndef VGPUCONFIGURATIONACTION_H
#define VGPUCONFIGURATIONACTION_H

#include "../../asyncoperation.h"
#include <QMap>
#include <QStringList>
#include <QSharedPointer>

class PGPU;

namespace XenAPI
{
    class Session;
}

/**
 * @brief Action to configure enabled VGPU types for physical GPUs
 *
 * This action allows administrators to control which VGPU types are enabled
 * on each physical GPU (PGPU) in the pool. When a VGPU type is enabled on a PGPU,
 * VMs can create virtual GPUs of that type using that physical GPU.
 *
 * The action:
 * 1. Takes a map of PGPU -> list of enabled VGPU type refs
 * 2. Calls PGPU.set_enabled_VGPU_types for each PGPU
 * 3. Updates progress as each PGPU is configured
 *
 * C# equivalent: XenAdmin.Actions.GPU.VgpuConfigurationAction
 */
class VgpuConfigurationAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new VGPU configuration action
         * @param updatedEnabledVGpuListByPGpu Map of PGPU ref -> list of enabled VGPU type refs
         * @param connection XenServer connection
         * @param parent Parent QObject
         */
        explicit VgpuConfigurationAction(const QMap<QString, QStringList>& updatedEnabledVGpuListByPGpu,
                                         XenConnection* connection,
                                         QObject* parent = nullptr);

    protected:
        /**
         * @brief Execute the VGPU configuration
         *
         * Iterates through all PGPUs and sets their enabled VGPU types.
         * Progress is updated as each PGPU is configured.
         *
         * @throws std::runtime_error if any PGPU configuration fails
         */
        void run() override;

    private:
        QMap<QString, QStringList> m_updatedEnabledVGpuListByPGpu; ///< Map of PGPU ref -> enabled VGPU type refs
};

#endif // VGPUCONFIGURATIONACTION_H
