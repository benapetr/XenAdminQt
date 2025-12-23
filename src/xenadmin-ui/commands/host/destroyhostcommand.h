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

#ifndef DESTROYHOSTCOMMAND_H
#define DESTROYHOSTCOMMAND_H

#include "../command.h"

/**
 * @brief Destroys the selected hosts.
 *
 * Qt port of C# DestroyHostCommand. Destroys hosts that are not live
 * and are not pool coordinators. Requires confirmation from user.
 *
 * Can run if:
 * - Single or multiple hosts selected
 * - Host is not live (not running)
 * - Host is not the pool coordinator
 * - Host belongs to a pool
 */
class DestroyHostCommand : public Command
{
    Q_OBJECT

public:
    explicit DestroyHostCommand(MainWindow* mainWindow, QObject* parent = nullptr);

    bool CanRun() const override;
    void Run() override;
    QString MenuText() const override;

private:
    bool canRunForHost(const QString& hostRef) const;
    bool isHostSelected() const;
    bool isHostCoordinator(const QString& hostRef) const;
    bool isHostLive(const QString& hostRef) const;
    QString buildConfirmationMessage() const;
    QString buildConfirmationTitle() const;
};

#endif // DESTROYHOSTCOMMAND_H
