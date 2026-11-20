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

#ifndef RUNPLUGINACTION_H
#define RUNPLUGINACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>

class Host;

/*!
 * \brief Calls a host plugin function (C# XenModel/Actions/Host/RunPluginAction.cs)
 *
 * The plugin answers "True" (optionally followed by data, which becomes the result);
 * any other answer fails the action with that answer as the error.
 */
class XENLIB_EXPORT RunPluginAction : public AsyncOperation
{
    Q_OBJECT

    public:
        RunPluginAction(XenConnection* connection, QSharedPointer<Host> host, const QString& plugin, const QString& function,
                        const QVariantMap& args, bool suppressHistory, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_hostRef;
        QString m_plugin;
        QString m_function;
        QVariantMap m_args;
};

#endif // RUNPLUGINACTION_H
