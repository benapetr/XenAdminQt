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

#ifndef CERTIFICATECOMMAND_H
#define CERTIFICATECOMMAND_H

#include "../command.h"
#include <QList>
#include "xenlib/xen/host.h"

class MainWindow;
class XenConnection;

/**
 * @brief Base class for certificate commands (install, reset)

 Port of XenAdmin/Commands/CertificateCommand.cs
 *
 * Provides common CanRun logic checking for:
 * - Stockholm or greater API version
 * - Single host selection
 * - Host must be standalone or pool coordinator
 */
class CertificateCommand : public Command
{
    Q_OBJECT

    public:
        explicit CertificateCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        ~CertificateCommand() override = default;

        bool CanRun() const override;
        void Run() override {}
        QString MenuText() const override { return "Certificate..."; }

    protected:
        /**
         * @brief Get hosts to operate on
         * @return List of hosts (from constructor or main window selection)
         */
        QList<QSharedPointer<Host>> getHosts() const;

        /**
         * @brief Check if version supports certificates (Stockholm or greater)
         */
        bool isVersionSupported(XenConnection* connection) const;

    private:
};

/**
 * @brief Install certificate on a host
 *
 * Port of XenAdmin/Commands/InstallCertificateCommand.cs
 * Shows InstallCertificateDialog and installs certificate on host
 */
class InstallCertificateCommand : public CertificateCommand
{
    Q_OBJECT

    public:
        explicit InstallCertificateCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        ~InstallCertificateCommand() override = default;

        void Run() override;
        QString MenuText() const override { return "Install Certificate..."; }
};

/**
 * @brief Reset certificate to self-signed
 *
 * Port of XenAdmin/Commands/ResetCertificateCommand.cs
 * Shows warning dialog and resets host certificate to self-signed
 * Requires Cloud or greater (API 1.290.0+)
 */
class ResetCertificateCommand : public CertificateCommand
{
    Q_OBJECT

    public:
        explicit ResetCertificateCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        ~ResetCertificateCommand() override = default;

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override { return "Reset Certificate..."; }

    private:
        bool isResetVersionSupported(XenConnection* connection) const;
};

#endif // CERTIFICATECOMMAND_H
