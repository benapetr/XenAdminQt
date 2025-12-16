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

#include "srtrimaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_SR.h"
#include <QXmlStreamReader>
#include <QDebug>

SrTrimAction::SrTrimAction(XenConnection* connection,
                           SR* sr,
                           QObject* parent)
    : AsyncOperation(connection,
                     QString("Trim SR '%1'").arg(sr->nameLabel()),
                     QString("Reclaiming freed space..."),
                     parent),
      m_sr(sr)
{
    setAppliesToFromObject(sr);
}

void SrTrimAction::run()
{
    qDebug() << "SrTrimAction: Trimming SR" << m_sr->uuid();

    XenSession* session = this->session();
    if (!session || !session->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    setDescription("Reclaiming freed space from storage...");

    // Find first attached storage host
    Host* host = m_sr->getFirstAttachedStorageHost();
    if (!host)
    {
        qWarning() << "SrTrimAction: Cannot reclaim freed space - SR is detached";
        setError("Cannot reclaim freed space, because the SR is detached");
        return;
    }

    qDebug() << "SrTrimAction: Using host" << host->nameLabel();

    // Call trim plugin
    try
    {
        QVariantMap args;
        args["sr_uuid"] = m_sr->uuid();

        QString result = XenAPI::Host::call_plugin(session,
                                                   host->opaqueRef(),
                                                   "trim",
                                                   "do_trim",
                                                   args);

        setResult(result);

        // Check result
        bool success = (result.toLower() == "true");

        if (success)
        {
            qDebug() << "SrTrimAction: Trim successful";
            setDescription("Freed space reclaimed successfully");
            setPercentComplete(100);
        } else
        {
            qWarning() << "SrTrimAction: Trim failed with result:" << result;
            QString error = getTrimError(result);
            if (error.isEmpty())
            {
                error = "Unknown error occurred during trim operation";
            }
            setError(error);
        }
    } catch (const std::exception& e)
    {
        qWarning() << "SrTrimAction: Plugin call failed:" << e.what();
        setError(QString("Failed to reclaim freed space: %1").arg(e.what()));
    }
}

QString SrTrimAction::getTrimError(const QString& xml)
{
    // Parse XML response like:
    // <trim_response>
    //   <key_value_pair><key>errcode</key><value>UnsupportedSRForTrim</value></key_value_pair>
    //   <key_value_pair><key>errmsg</key><value>Trim on [uuid] not supported</value></key_value_pair>
    // </trim_response>

    QString errcode;
    QString errmsg;

    QXmlStreamReader reader(xml);
    QString currentKey;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement() && reader.name().toString() == "key")
        {
            currentKey = reader.readElementText();
        } else if (reader.isStartElement() && reader.name().toString() == "value")
        {
            QString value = reader.readElementText();

            if (currentKey == "errcode")
            {
                errcode = value;
            } else if (currentKey == "errmsg")
            {
                errmsg = value;
            }
        }
    }

    if (reader.hasError())
    {
        qDebug() << "SrTrimAction: XML parsing error:" << reader.errorString();
        return QString();
    }

    // Return the error message (C# also tries to lookup friendly names from resources,
    // but for now we'll just return the message from the server)
    return errmsg;
}
