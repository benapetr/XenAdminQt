/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef NRPEACTIONS_H
#define NRPEACTIONS_H

#include "../../asyncoperation.h"
#include "nrpehostconfiguration.h"
#include <QVariantMap>

class XenObject;

/*!
 * \brief Reads the NRPE settings of a host, or of a pool's coordinator
 * (C# XenModel/Actions/NRPE/NRPERetrieveAction.cs)
 *
 * Like C#, a plugin failure doesn't fail the action: Succeeded() reports it instead.
 */
class XENLIB_EXPORT NRPERetrieveAction : public AsyncOperation
{
    Q_OBJECT

    public:
        NRPERetrieveAction(QSharedPointer<XenObject> hostOrPool, bool suppressHistory, QObject* parent = nullptr);

        NRPEHostConfiguration GetConfiguration() const { return this->m_configuration; }
        bool Succeeded() const { return this->m_succeeded; }

    protected:
        void run() override;

    private:
        QString m_hostRef;
        NRPEHostConfiguration m_configuration;
        bool m_succeeded = false;
};

/*!
 * \brief Applies changed NRPE settings to a host, or to every host of a pool
 * (C# XenModel/Actions/NRPE/NRPEUpdateAction.cs)
 */
class XENLIB_EXPORT NRPEUpdateAction : public AsyncOperation
{
    Q_OBJECT

    public:
        NRPEUpdateAction(QSharedPointer<XenObject> hostOrPool, const NRPEHostConfiguration& configuration,
                         const NRPEHostConfiguration& original, bool suppressHistory, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        void configureHost(const QString& hostRef);
        QString callPlugin(const QString& hostRef, const QString& function, const QVariantMap& args = QVariantMap());

        QStringList m_hostRefs;
        NRPEHostConfiguration m_configuration;
        NRPEHostConfiguration m_original;
};

#endif // NRPEACTIONS_H
