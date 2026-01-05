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

#ifndef IMPORTVMACTION_H
#define IMPORTVMACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>

class HttpClient;

/**
 * @brief Import a VM from an XVA file
 *
 * Qt equivalent of C# XenAdmin.Actions.ImportVmAction.
 * Handles HTTP PUT upload of VM export file with progress tracking.
 */
class ImportVmAction : public AsyncOperation
{
    Q_OBJECT

    public:
        static const QString IMPORT_TASK;
        static const int HTTP_PUT_TIMEOUT = 30 * 60 * 1000; // 30 minutes

        /**
        * @brief Construct ImportVmAction
        * @param connection XenServer connection
        * @param hostRef Affinity host (may be empty)
        * @param filename Local XVA file path
        * @param srRef Target SR reference
        * @param parent Parent object
        */
        explicit ImportVmAction(XenConnection* connection,
                                const QString& hostRef,
                                const QString& filename,
                                const QString& srRef,
                                QObject* parent = nullptr);

        ~ImportVmAction() override;

        /**
        * @brief Called by wizard when it finishes configuration
        * @param startAutomatically Whether to start VM after import
        * @param vifRefs List of VIF references to create
        */
        void endWizard(bool startAutomatically, const QStringList& vifRefs);

        /**
        * @brief Get imported VM reference (available after completion)
        */
        QString vmRef() const { return this->vmRef_; }

    protected:
        void run() override;

    private:
        QString uploadFile();
        QString defaultVMName(const QString& vmName);
        int vmsWithName(const QString& name);
        void updateNetworks(const QString& vmRef, const QString& importTaskRef);

        QString hostRef_;
        QString filename_;
        QString srRef_;
        QString vmRef_;
        QString importTaskRef_;
        
        bool wizardDone_;
        bool startAutomatically_;
        QStringList vifRefs_;
        
        QMutex mutex_;
        QWaitCondition waitCondition_;
        
        HttpClient* httpClient_;
};

#endif // IMPORTVMACTION_H
