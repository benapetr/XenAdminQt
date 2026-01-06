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

#ifndef VMSHUTDOWNACTION_H
#define VMSHUTDOWNACTION_H

#include "../../asyncoperation.h"
#include "../../api.h"
#include <QSharedPointer>

class VM;
class Host;
class Pool;

/**
 * @brief Abstract base class for VM shutdown/suspend actions
 *
 * Qt port of C# VMShutdownAction. Base class for clean shutdown,
 * hard shutdown, and suspend operations.
 */
class XENLIB_EXPORT VMShutdownAction : public AsyncOperation
{
    Q_OBJECT

    public:
        virtual ~VMShutdownAction();

    protected:
        explicit VMShutdownAction(QSharedPointer<VM> vm, const QString& title, QObject* parent = nullptr);
};

/**
 * @brief Clean shutdown of a VM (VM.async_clean_shutdown)
 *
 * Qt port of C# VMCleanShutdown. Performs a graceful shutdown
 * by signaling the VM's guest OS.
 */
class XENLIB_EXPORT VMCleanShutdown : public VMShutdownAction
{
    Q_OBJECT

    public:
        explicit VMCleanShutdown(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

/**
 * @brief Hard shutdown of a VM (VM.async_hard_shutdown)
 *
 * Qt port of C# VMHardShutdown. Forces immediate shutdown
 * without signaling guest OS (power off).
 */
class XENLIB_EXPORT VMHardShutdown : public VMShutdownAction
{
    Q_OBJECT

    public:
        explicit VMHardShutdown(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

/**
 * @brief Suspend a VM (VM.async_suspend)
 *
 * Qt port of C# VMSuspendAction. Suspends a VM to disk,
 * allowing it to be resumed later.
 */
class XENLIB_EXPORT VMSuspendAction : public VMShutdownAction
{
    Q_OBJECT

    public:
        explicit VMSuspendAction(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

#endif // VMSHUTDOWNACTION_H
