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

#ifndef VDICOMMAND_H
#define VDICOMMAND_H

#include "../command.h"
#include <QSharedPointer>

class VDI;

/**
 * @brief Base class for VDI-related commands
 *
 * Provides common functionality for commands operating on VDI (Virtual Disk Image) objects.
 * Similar to VMCommand, SRCommand, and VBDCommand patterns.
 */
class VDICommand : public Command
{
    Q_OBJECT

    public:
        explicit VDICommand(MainWindow* mainWindow, QObject* parent = nullptr);
        ~VDICommand() override = default;

    protected:
        /**
         * @brief Get the currently selected VDI as a typed XenObject
         * @return Shared pointer to VDI object, or nullptr if not a VDI
         */
        QSharedPointer<VDI> getVDI() const;

        /**
         * @brief Get selected VDI opaque reference
         * @return VDI reference or empty string
         */
        QString getSelectedVDIRef() const;

        /**
         * @brief Get selected VDI name
         * @return VDI name label or empty string
         */
        QString getSelectedVDIName() const;
};

#endif // VDICOMMAND_H
