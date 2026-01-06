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

#ifndef GPUASSIGNACTION_H
#define GPUASSIGNACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QStringList>
#include <QVariantList>

/**
 * @brief Action to assign/configure virtual GPUs for a VM
 *
 * Manages VGPU assignments by:
 * - Removing VGPUs that are no longer needed
 * - Adding new VGPUs with specified GPU groups and types
 *
 * Equivalent to C# XenAdmin GpuAssignAction.
 */
class GpuAssignAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct GPU assignment action
         * @param connection XenServer connection
         * @param vmRef VM opaque reference
         * @param vgpuData List of VGPU configurations (each is a QVariantMap with keys: opaque_ref, GPU_group, type, device)
         * @param parent Parent QObject
         */
        explicit GpuAssignAction(XenConnection* connection,
                                 const QString& vmRef,
                                 const QVariantList& vgpuData,
                                 QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vmRef;
        QVariantList m_vgpuData; // List of VGPU specs

        void addGpu(const QString& gpuGroupRef, const QString& vgpuTypeRef, const QString& device);
};

#endif // GPUASSIGNACTION_H
