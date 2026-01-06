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

#ifndef CREATESRIOVACTION_H
#define CREATESRIOVACTION_H

#include "../../asyncoperation.h"
#include <QStringList>

/**
 * @brief CreateSriovAction - Creates SR-IOV network
 *
 * Qt port of C# XenAdmin.Actions.CreateSriovAction.
 * Creates an SR-IOV enabled network on selected PIFs.
 *
 * C# path: XenModel/Actions/Network/CreateSriovAction.cs
 */
class CreateSriovAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param networkName Name for the new SR-IOV network
         * @param pifRefs List of PIF references to enable SR-IOV on
         * @param parent Parent QObject
         */
        CreateSriovAction(XenConnection* connection,
                          const QString& networkName,
                          const QStringList& pifRefs,
                          QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_networkName;
        QStringList m_pifRefs;
};

#endif // CREATESRIOVACTION_H
