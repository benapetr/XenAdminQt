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

#ifndef ADDVIRTUALDISKCOMMAND_H
#define ADDVIRTUALDISKCOMMAND_H

#include "../command.h"
#include <QVariantMap>

class XenCache;

class AddVirtualDiskCommand : public Command
{
    Q_OBJECT

    public:
        explicit AddVirtualDiskCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        // Inherited from Command
        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        bool isSRSelected() const;
        bool isVMSelected() const;
        QString getSelectedRef() const;
        bool canAddDisk() const;

        /**
         * @brief Get max VBDs allowed for a VM
         * @param vmData VM data record
         * @return Maximum number of VBDs allowed
         */
        int getMaxVBDsAllowed(const QVariantMap& vmData) const;

        /**
         * @brief Get current VBD count for a VM
         * @param cache Cache instance
         * @param vmRef VM reference
         * @return Number of VBDs currently attached
         */
        int getCurrentVBDCount(const QString& vmRef) const;
        int getCurrentVBDCount(XenCache* cache, const QString& vmRef) const;
};

#endif // ADDVIRTUALDISKCOMMAND_H
