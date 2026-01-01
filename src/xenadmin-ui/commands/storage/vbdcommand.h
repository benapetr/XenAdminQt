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

#ifndef VBDCOMMAND_H
#define VBDCOMMAND_H

#include "../command.h"
#include <QSharedPointer>

class VBD;

/**
 * @brief Base class for VBD-related commands
 *
 * Provides common functionality for commands operating on VBD (Virtual Block Device) objects.
 * Similar to VMCommand and SRCommand patterns.
 */
class VBDCommand : public Command
{
    Q_OBJECT

    public:
        explicit VBDCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        ~VBDCommand() override = default;

    protected:
        /**
        * @brief Get the currently selected VBD as a typed XenObject
        * @return Shared pointer to VBD object, or nullptr if not a VBD
        */
        QSharedPointer<VBD> getVBD() const;

        /**
        * @brief Get selected VBD opaque reference
        * @return VBD reference or empty string
        */
        QString getSelectedVBDRef() const;
};

#endif // VBDCOMMAND_H
