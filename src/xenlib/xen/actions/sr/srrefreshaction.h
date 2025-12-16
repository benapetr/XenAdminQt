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

#ifndef SRREFRESHACTION_H
#define SRREFRESHACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;

/**
 * @brief Refresh an SR to detect new or changed VDIs
 *
 * Qt equivalent of C# XenAdmin.Actions.SrRefreshAction
 *
 * This action calls SR.scan() to refresh the SR's VDI list, detecting:
 * - New VDIs added outside of XenCenter/XenAdmin
 * - Changed VDI metadata
 * - Removed VDIs
 *
 * This is a simple wrapper around SR.scan() with proper title/description.
 *
 * C# reference: XenModel/Actions/SR/SrRefreshAction.cs
 *
 * Usage:
 *   SrRefreshAction* action = new SrRefreshAction(connection, srRef);
 *   action->runAsync();
 */
class XENLIB_EXPORT SrRefreshAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection The connection to the XenServer
     * @param srRef The SR to refresh/scan
     * @param parent Parent QObject
     *
     * C# equivalent: SrRefreshAction(SR sr)
     */
    SrRefreshAction(XenConnection* connection, const QString& srRef,
                    QObject* parent = nullptr);

protected:
    void run() override;

private:
    QString m_srRef;

    QString getSRName() const;
};

#endif // SRREFRESHACTION_H
