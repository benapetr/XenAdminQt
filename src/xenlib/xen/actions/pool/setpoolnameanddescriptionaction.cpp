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

#include "setpoolnameanddescriptionaction.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../session.h"
#include <stdexcept>

SetPoolNameAndDescriptionAction::SetPoolNameAndDescriptionAction(XenConnection* connection,
                                                                 const QString& poolRef,
                                                                 const QString& name,
                                                                 const QString& description,
                                                                 QObject* parent)
    : AsyncOperation(connection, "Setting Pool Properties", "Updating pool name and description...", parent), m_poolRef(poolRef), m_name(name), m_description(description)
{
}

void SetPoolNameAndDescriptionAction::run()
{
    try
    {
        if (!session())
        {
            throw std::runtime_error("Not connected to XenServer");
        }

        if (m_poolRef.isEmpty())
        {
            throw std::runtime_error("Pool reference is empty");
        }

        setPercentComplete(0);

        // Set pool name if provided
        if (!m_name.isNull())
        {
            setDescription(QString("Setting pool name to '%1'...").arg(m_name));
            XenAPI::Pool::set_name_label(session(), m_poolRef, m_name);
            setPercentComplete(50);
        }

        // Set pool description if provided
        if (!m_description.isNull())
        {
            setDescription(QString("Setting pool description..."));
            XenAPI::Pool::set_name_description(session(), m_poolRef, m_description);
            setPercentComplete(100);
        }

        setDescription("Pool properties updated successfully");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to set pool properties: %1").arg(e.what()));
        throw;
    }
}
