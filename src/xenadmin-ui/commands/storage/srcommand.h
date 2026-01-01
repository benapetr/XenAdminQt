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

#ifndef SRCOMMAND_H
#define SRCOMMAND_H

#include "../command.h"

class SR;

/**
 * @class SRCommand
 * @brief Base class for SR (Storage Repository) commands.
 *
 * Provides common functionality for SR-specific commands:
 * - getSR() returns typed SR object instead of raw QVariant data
 * - Helper methods for SR reference and name access
 * - Multi-connection support via sr->connection() instead of global xenLib()
 *
 * Similar to VMCommand but for storage repository operations.
 */
class SRCommand : public Command
{
    public:
        SRCommand(MainWindow* mainWindow, QObject* parent);

    protected:
        /**
         * @brief Get the selected SR as a typed object.
         * @return QSharedPointer to SR object, or null if not an SR or not found
         */
        QSharedPointer<SR> getSR() const;

        /**
         * @brief Get the opaque reference of the selected SR.
         * @return SR opaque reference, or empty string if not an SR
         */
        QString getSelectedSRRef() const;

        /**
         * @brief Get the name of the selected SR.
         * @return SR name label, or empty string if not an SR
         */
        QString getSelectedSRName() const;
};

#endif // SRCOMMAND_H
