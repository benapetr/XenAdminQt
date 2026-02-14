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

#ifndef GETHEARTBEATSRSACTION_H
#define GETHEARTBEATSRSACTION_H

#include "../../asyncoperation.h"
#include <QList>
#include <QSharedPointer>

class Pool;
class SR;

struct SRWrapper
{
    bool enabled = false;
    QString reasonUnsuitable;
    QSharedPointer<SR> sr;
};

/**
 * @brief Async action that probes all shared SRs for HA statefile suitability.
 *
 * Parity target: C# GetHeartbeatSRsAction.
 */
class GetHeartbeatSRsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit GetHeartbeatSRsAction(QSharedPointer<Pool> pool, QObject* parent = nullptr);
        ~GetHeartbeatSRsAction() override = default;

        const QList<SRWrapper>& GetSRs() const
        {
            return this->m_srs;
        }

    protected:
        void run() override;

    private:
        QSharedPointer<Pool> m_pool;
        QList<SRWrapper> m_srs;
};

#endif // GETHEARTBEATSRSACTION_H
