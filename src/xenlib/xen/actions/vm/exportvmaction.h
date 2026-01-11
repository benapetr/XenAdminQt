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

#ifndef EXPORTVMACTION_H
#define EXPORTVMACTION_H

#include "../../asyncoperation.h"
#include "../../vm.h"
#include "../../host.h"
#include <QString>
#include <QThread>

class HttpClient;

/**
 * @brief Export a VM or template to an XVA file
 *
 * Qt equivalent of C# XenAdmin.Actions.ExportVmAction.
 * Handles HTTP GET download of VM/template with progress tracking.
 */
class ExportVmAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
        * @brief Construct ExportVmAction
        * @param vm VM or template to export
        * @param host Host to export from (may be null, uses pool master)
        * @param filename Local file path to save export
        * @param verify Whether to verify the exported file
        * @param parent Parent object
        */
        explicit ExportVmAction(QSharedPointer<VM> vm,
                                QSharedPointer<Host> host,
                                const QString& filename,
                                bool verify = false,
                                QObject* parent = nullptr);

        ~ExportVmAction() override;

    protected:
        void run() override;

    private:
        void progressPoll();
        
        QSharedPointer<VM> m_vm;
        QSharedPointer<Host> m_host;
        QString filename_;
        bool verify_;
        
        HttpClient* httpClient_;
        QThread* progressThread_;
        QString exception_;
};

#endif // EXPORTVMACTION_H
