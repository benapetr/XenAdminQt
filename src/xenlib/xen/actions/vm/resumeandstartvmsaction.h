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

#ifndef RESUMEANDSTARTVMSACTION_H
#define RESUMEANDSTARTVMSACTION_H

#include "../../asyncoperation.h"
#include "vmstartabstractaction.h"
#include <QList>

class VM;
class Host;
class XenConnection;

/**
 * @brief Resume and start multiple VMs
 *
 * Qt port of C# ResumeAndStartVMsAction. This action:
 * 1. Resumes suspended VMs (either on a specific host or anywhere)
 * 2. Starts halted VMs (either on a specific host or anywhere)
 *
 * Progress is tracked across all operations.
 *
 * C# location: XenModel/Actions/VM/ResumeAndStartVMsAction.cs
 */
class XENLIB_EXPORT ResumeAndStartVMsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param host Optional host to start/resume VMs on (can be null for auto-placement)
         * @param vmsToResume List of suspended VMs to resume
         * @param vmsToStart List of halted VMs to start
         * @param warningDialogHAInvalidConfig Callback for HA configuration warnings
         * @param startDiagnosisForm Callback for start failure diagnosis
         * @param parent Parent QObject
         */
        explicit ResumeAndStartVMsAction(XenConnection* connection,
                                         QSharedPointer<Host> host,
                                         const QList<QSharedPointer<VM>>& vmsToResume,
                                         const QList<QSharedPointer<VM>>& vmsToStart,
                                         VMStartAbstractAction::WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                         VMStartAbstractAction::StartDiagnosisForm startDiagnosisForm,
                                         QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        bool canVMBootOnHost(QSharedPointer<VM> vm, QSharedPointer<Host> host);
        void onActionProgress(int percent);

        QSharedPointer<Host> m_host;
        QList<QSharedPointer<VM>> m_vmsToResume;
        QList<QSharedPointer<VM>> m_vmsToStart;
        VMStartAbstractAction::WarningDialogHAInvalidConfig m_warningDialogHAInvalidConfig;
        VMStartAbstractAction::StartDiagnosisForm m_startDiagnosisForm;

        int m_actionCountTotal;
        int m_actionCountCompleted;
};

#endif // RESUMEANDSTARTVMSACTION_H
