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

#ifndef TRIMSRCOMMAND_H
#define TRIMSRCOMMAND_H

#include "srcommand.h"

/**
 * @brief TrimSRCommand - Trim (reclaim freed space) from storage repository
 *
 * Qt equivalent of C# XenAdmin.Commands.TrimSRCommand
 *
 * Reclaims freed space from thin-provisioned storage repositories.
 * When VDIs are deleted, the space isn't always immediately returned
 * to the underlying storage. Trim explicitly reclaims that space.
 *
 * Requirements:
 * - SR must support trim (thin-provisioned storage)
 * - SR must be attached to at least one host
 *
 * The command will:
 * - Check if SR supports trim
 * - Show confirmation dialog
 * - Run SrTrimAction to reclaim space
 */
class TrimSRCommand : public SRCommand
{
    Q_OBJECT

    public:
        explicit TrimSRCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

        /**
         * @brief Explicitly set the SR this command should operate on
         * @param srRef SR opaque reference
         */
        void setTargetSR(const QString& srRef);

    private:
        /**
         * @brief Get selected SR reference
         * @return SR opaque reference or empty string
         */
        QString getSelectedSRRef() const;

        /**
         * @brief Get selected SR data
         * @return SR data from cache
         */
        QVariantMap getSelectedSRData() const;

        /**
         * @brief Check if SR supports trim
         * @param srData SR data from cache
         * @return true if SR supports trim
         */
        bool supportsTrim(const QVariantMap& srData) const;

        /**
         * @brief Check if SR is attached to any host
         * @param srData SR data from cache
         * @return true if at least one PBD is currently attached
         */
        bool isAttachedToHost(const QVariantMap& srData) const;

        /**
         * @brief Get SR name for display
         * @param srData SR data from cache
         * @return SR name
         */
        QString getSRName(const QVariantMap& srData) const;

        QString m_overrideSRRef;
};

#endif // TRIMSRCOMMAND_H
