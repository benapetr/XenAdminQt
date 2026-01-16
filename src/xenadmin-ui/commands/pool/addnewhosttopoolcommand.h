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

#ifndef ADDNEWHOSTTOPOOLCOMMAND_H
#define ADDNEWHOSTTOPOOLCOMMAND_H

#include "../command.h"

class Pool;
class XenConnection;

/**
 * @brief Command to connect a new server and add it to a pool
 * 
 * This command shows the "Add Server" dialog. When a new connection
 * is successfully established, it automatically adds the new host to
 * the specified pool.
 */
class AddNewHostToPoolCommand : public Command
{
    Q_OBJECT

    public:
        /**
        * @brief Construct command to add a new host to a pool
        * @param mainWindow Main window interface
        * @param pool Target pool for the new host
        */
        AddNewHostToPoolCommand(MainWindow* mainWindow, QSharedPointer<Pool> pool);

        bool CanRun() const override { return true; } // Can always try to add a new server
        void Run() override;
        QString MenuText() const override;

    private slots:
        /**
        * @brief Called when new connection's cache is populated
        * @param connection The newly connected server
        */
        void onCachePopulated(XenConnection* connection);

    private:
        QSharedPointer<Pool> pool_;
};

#endif // ADDNEWHOSTTOPOOLCOMMAND_H
