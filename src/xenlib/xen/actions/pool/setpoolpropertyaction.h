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

#ifndef SETPOOLPROPERTYACTION_H
#define SETPOOLPROPERTYACTION_H

#include "../../asyncoperation.h"
#include <QVariant>

class XenConnection;

/**
 * @brief Generic action to set a pool property
 *
 * This action sets a boolean or string property on a pool using the XenAPI.
 * Handles properties like migration_compression, live_patching_disabled, etc.
 *
 * C# Reference: Uses DelegatedAsyncAction with Pool.set_* methods
 */
class SetPoolPropertyAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct pool property update action
     * @param connection Connection to the pool
     * @param poolRef Pool opaque reference
     * @param propertyName Property name (e.g., "migration_compression")
     * @param value Property value (bool, string, etc.)
     * @param description Action description
     * @param parent Parent QObject
     */
    SetPoolPropertyAction(XenConnection* connection,
                          const QString& poolRef,
                          const QString& propertyName,
                          const QVariant& value,
                          const QString& description,
                          QObject* parent = nullptr);

    ~SetPoolPropertyAction() override = default;

protected:
    void run() override;

private:
    QString m_poolRef;
    QString m_propertyName;
    QVariant m_value;
};

#endif // SETPOOLPROPERTYACTION_H
