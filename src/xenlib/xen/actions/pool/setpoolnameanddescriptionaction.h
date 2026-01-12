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

#ifndef SETPOOLNAMEANDDESCRIPTIONACTION_H
#define SETPOOLNAMEANDDESCRIPTIONACTION_H

#include "../../asyncoperation.h"

class XenConnection;

/**
 * @brief Action to set pool name and description
 *
 * This action updates the name (label) and description of a pool.
 * Used when renaming a pool or updating its description metadata.
 *
 * Corresponds to C# CreatePoolAction and DestroyPoolAction usage patterns
 * (which call Pool.set_name_label and Pool.set_name_description directly).
 */
class SetPoolNameAndDescriptionAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct pool metadata update action
         * @param pool Pool object
         * @param name New pool name (empty to leave unchanged)
         * @param description New pool description (empty to leave unchanged)
         * @param parent Parent QObject
         */
        explicit SetPoolNameAndDescriptionAction(QSharedPointer<Pool> pool,
                                                 const QString& name,
                                                 const QString& description,
                                                 QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<Pool> m_pool;
        QString m_name;
        QString m_description;
};

#endif // SETPOOLNAMEANDDESCRIPTIONACTION_H
