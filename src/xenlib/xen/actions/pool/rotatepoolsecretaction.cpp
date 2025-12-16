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

#include "rotatepoolsecretaction.h"
#include "../../../xen/connection.h"
#include "../../../xen/session.h"
#include "../../xenapi/xenapi_Pool.h"
#include <stdexcept>

RotatePoolSecretAction::RotatePoolSecretAction(XenConnection* connection, const QString& poolRef, QObject* parent)
    : AsyncOperation(connection,
                     QString("Rotating pool secret"),
                     "Rotating pool secret",
                     parent),
      m_poolRef(poolRef)
{
    if (m_poolRef.isEmpty())
    {
        throw std::invalid_argument("Pool reference cannot be empty");
    }
}

void RotatePoolSecretAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Rotating pool secret...");

        // Call Pool.rotate_secret API
        XenAPI::Pool::rotate_secret(session(), m_poolRef);

        setPercentComplete(100);
        setDescription("Pool secret rotated successfully");

    } catch (const std::exception& e)
    {
        if (isCancelled())
        {
            setDescription("Rotation cancelled");
        } else
        {
            setError(QString("Failed to rotate pool secret: %1").arg(e.what()));
        }
    }
}
