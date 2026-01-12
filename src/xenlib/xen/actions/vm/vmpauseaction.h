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

#ifndef VMPAUSEACTION_H
#define VMPAUSEACTION_H

#include "../../asyncoperation.h"

class VM;
class Host;
class Pool;

/**
 * @brief Abstract base class for VM pause/unpause actions
 *
 * Qt port of C# VMPauseAction. Base class for pause and unpause operations.
 */
class XENLIB_EXPORT VMPauseAction : public AsyncOperation
{
    Q_OBJECT

    public:
        virtual ~VMPauseAction();

    protected:
        explicit VMPauseAction(QSharedPointer<VM> vm, const QString& title, QObject* parent = nullptr);
};

/**
 * @brief Pause a VM (VM.async_pause)
 *
 * Qt port of C# VMPause. Pauses a running VM, suspending execution
 * while keeping it in memory.
 */
class XENLIB_EXPORT VMPause : public VMPauseAction
{
    Q_OBJECT

    public:
        explicit VMPause(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

/**
 * @brief Unpause a VM (VM.async_unpause)
 *
 * Qt port of C# VMUnpause. Resumes a paused VM, continuing execution
 * from the paused state.
 */
class XENLIB_EXPORT VMUnpause : public VMPauseAction
{
    Q_OBJECT

    public:
        explicit VMUnpause(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        void run() override;
};

#endif // VMPAUSEACTION_H
