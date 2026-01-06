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

#ifndef VMREBOOTACTION_H
#define VMREBOOTACTION_H

#include "../../asyncoperation.h"
#include "../../api.h"
#include <QSharedPointer>

class VM;
class Host;
class Pool;

/**
 * @brief Abstract base class for VM reboot actions
 *
 * Qt port of C# VMRebootAction. Base class for clean reboot
 * and hard reboot operations.
 */
class XENLIB_EXPORT VMRebootAction : public AsyncOperation
{
    Q_OBJECT

    public:
        virtual ~VMRebootAction();

    protected:
        explicit VMRebootAction(QSharedPointer<VM> vm, const QString& title, QObject* parent = nullptr);
};

/**
 * @brief Clean reboot of a VM (VM.async_clean_reboot)
 *
 * Qt port of C# VMCleanReboot. Performs a graceful reboot
 * by signaling the VM's guest OS to restart.
 */
class XENLIB_EXPORT VMCleanReboot : public VMRebootAction
{
    Q_OBJECT

    public:
        explicit VMCleanReboot(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

/**
 * @brief Hard reboot of a VM (VM.async_hard_reboot)
 *
 * Qt port of C# VMHardReboot. Forces immediate reboot
 * without signaling guest OS (power cycle).
 */
class XENLIB_EXPORT VMHardReboot : public VMRebootAction
{
    Q_OBJECT

    public:
        explicit VMHardReboot(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

#endif // VMREBOOTACTION_H
