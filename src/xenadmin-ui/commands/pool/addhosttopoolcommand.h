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

#ifndef ADDHOSTTOPOOLCOMMAND_H
#define ADDHOSTTOPOOLCOMMAND_H

#include "../command.h"
#include <QList>

class Host;
class Pool;

/**
 * @brief Command for adding one or more hosts to a pool
 * 
 * This command validates pool join rules, shows confirmation dialogs,
 * and executes the AddHostToPoolAction for each host.
 */
class AddHostToPoolCommand : public Command
{
    Q_OBJECT

    public:
        /**
        * @brief Construct command to add hosts to a pool
        * @param mainWindow Main window interface
        * @param hosts List of hosts to add to the pool
        * @param pool Target pool (must be valid)
        * @param confirm If true, shows confirmation dialog before executing
        */
        AddHostToPoolCommand(MainWindow* mainWindow, const QList<QSharedPointer<Host>>& hosts,
                            QSharedPointer<Pool> pool, bool confirm);

        bool CanRun() const override;
        void Run() override;
        
        QString MenuText() const override { return QString(); }

    private:
        QList<QSharedPointer<Host>> hosts_;
        QSharedPointer<Pool> m_pool;
        bool confirm_;
        
        /**
        * @brief Check if hosts can join the pool according to PoolJoinRules
        * @return Map of hosts to error messages (empty if all can join)
        */
        QMap<QString, QString> checkPoolJoinRules() const;
        
        /**
        * @brief Show confirmation dialog for adding hosts to pool
        * @return true if user confirmed, false if cancelled
        */
        bool showConfirmationDialog();
        
        /**
        * @brief Check for supplemental pack differences and warn user
        * @return true if user wants to proceed, false if cancelled
        */
        bool checkSuppPacksAndWarn();
        
        /**
        * @brief Get permission for licensing changes (free hosts -> paid coordinator)
        * @param coordinator Pool coordinator host
        * @return true if user granted permission, false if cancelled
        */
        bool getPermissionForLicensing(QSharedPointer<Host> coordinator);
        
        /**
        * @brief Get permission for CPU masking changes
        * @param coordinator Pool coordinator host
        * @return true if user granted permission, false if cancelled
        */
        bool getPermissionForCpuMasking(QSharedPointer<Host> coordinator);
        
        /**
        * @brief Get permission for AD configuration changes
        * @param coordinator Pool coordinator host
        * @return true if user granted permission, false if cancelled
        */
        bool getPermissionForAdConfig(QSharedPointer<Host> coordinator);
};

#endif // ADDHOSTTOPOOLCOMMAND_H
