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

#ifndef DETACHSRACTION_H
#define DETACHSRACTION_H

#include "../../asyncoperation.h"
#include <QString>

/**
 * @brief DetachSrAction - Detaches SR by unplugging (and optionally destroying) PBDs
 *
 * C# equivalent: XenModel/Actions/SR/SrAction.cs::DetachSrAction
 *
 * Unplugs all PBDs for the SR, with coordinator PBD last.
 * Optionally destroys PBDs after unplugging.
 */
class DetachSrAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct DetachSrAction
     * @param connection XenConnection
     * @param srRef SR opaque reference
     * @param srName SR name (for messages)
     * @param destroyPbds Whether to destroy PBDs after unplugging
     * @param parent Parent QObject
     */
    explicit DetachSrAction(XenConnection* connection,
                            const QString& srRef,
                            const QString& srName,
                            bool destroyPbds = false,
                            QObject* parent = nullptr);

protected:
    void run() override;

private:
    void unplugPBDs();
    void destroyPBDs();

    QString m_srRef;
    QString m_srName;
    bool m_destroyPbds;
    QStringList m_pbdRefs;
    int m_currentPbdIndex;
};

#endif // DETACHSRACTION_H
