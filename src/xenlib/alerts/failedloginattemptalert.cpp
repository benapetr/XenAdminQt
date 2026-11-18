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

#include "failedloginattemptalert.h"
#include "../xen/network/connection.h"
#include "../xen/pool.h"
#include "../xencache.h"
#include "../utils/misc.h"
#include <QDomDocument>
#include <QLocale>

using namespace XenLib;

FailedLoginAttemptAlert::FailedLoginAttemptAlert(XenConnection* connection, const QVariantMap& messageData)
    : MessageAlert(connection, messageData)
{
    this->parseBody();
}

void FailedLoginAttemptAlert::parseBody()
{
    /* - Total and unknown are optional (unknown being absent implies that all the failed
         logins came from known sources, and conversely total being absent implies that all
         the failed logins came from unknown sources).
       - There may be 0 or more known tags.
       - Each known tag must contain date and number, but the other tags (ip, originator,
         username, useragent) are all optional (though we expect at least one) */
    QDomDocument doc;
    if (!doc.setContent(this->GetMessageBody()))
        return; // C# ignores unparsable bodies

    const QDomNodeList totalNodes = doc.elementsByTagName(QStringLiteral("total"));
    if (!totalNodes.isEmpty())
        this->m_total = totalNodes.at(0).toElement().text().trimmed().toInt();

    const QDomNodeList unknownNodes = doc.elementsByTagName(QStringLiteral("unknown"));
    if (!unknownNodes.isEmpty())
        this->m_unknown = unknownNodes.at(0).toElement().text().trimmed().toInt();

    const QDomNodeList knownNodes = doc.elementsByTagName(QStringLiteral("known"));
    for (int i = 0; i < knownNodes.size(); ++i)
    {
        Offender offender;
        for (QDomElement child = knownNodes.at(i).firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        {
            const QString name = child.tagName();
            const QString text = child.text();
            if (name == QLatin1String("number"))
                offender.number = text.trimmed().toInt();
            else if (name == QLatin1String("date"))
                offender.date = Misc::ParseXenDateTime(text.trimmed());
            else if (name == QLatin1String("ip"))
                offender.ip = text;
            else if (name == QLatin1String("originator"))
                offender.originator = text;
            else if (name == QLatin1String("username"))
                offender.username = text;
            else if (name == QLatin1String("useragent"))
                offender.useragent = text;
        }
        this->m_topOffenders.append(offender);
    }
}

QString FailedLoginAttemptAlert::GetTitle() const
{
    // C# Reference: the pool whose uuid is the message's obj_uuid
    XenCache* cache = this->m_connection ? this->m_connection->GetCache() : nullptr;
    if (cache)
    {
        for (const QSharedPointer<Pool>& pool : cache->GetAll<Pool>())
        {
            if (pool && pool->GetUUID() == this->GetObjUUID())
                return tr("There has been a number of failed attempts to login to '%1' within the past hour.").arg(pool->GetName());
        }
    }
    return MessageAlert::GetTitle();
}

QString FailedLoginAttemptAlert::GetDescription() const
{
    if (this->m_total == 0 && this->m_unknown > 0)
        return tr("All %1 attempts originated from unknown sources.").arg(this->m_unknown);

    QStringList lines;
    if (this->m_total > 0)
        lines.append(tr("There has been a total of %1 failed login attempts.").arg(this->m_total));

    if (this->m_unknown > 0)
        lines.append(tr("%1 attempts originated from unknown sources.").arg(this->m_unknown));

    if (!this->m_topOffenders.isEmpty())
    {
        lines.append(tr("The most common attempts originated from the following sources:"));
        for (const Offender& offender : this->m_topOffenders)
            lines.append(offender.ToString());
    }

    // C# falls back to the title (not the description) when the body had nothing usable
    return !lines.isEmpty() ? lines.join(QLatin1Char('\n')) : MessageAlert::GetTitle();
}

QString FailedLoginAttemptAlert::Offender::ToString() const
{
    QStringList entries;
    if (!this->ip.isEmpty())
        entries.append(FailedLoginAttemptAlert::tr("IP address: %1").arg(this->ip));
    if (this->number > 0)
        entries.append(FailedLoginAttemptAlert::tr("Number of attempts: %1").arg(this->number));
    if (this->date.isValid())
    {
        // C# AlertExtensions.GetGuiDate - local time, short date and time
        const QString friendlyDate = QLocale().toString(this->date.toLocalTime(), QLocale::ShortFormat);
        entries.append(FailedLoginAttemptAlert::tr("Last attempt on: %1").arg(friendlyDate));
    }
    if (!this->username.isEmpty())
        entries.append(FailedLoginAttemptAlert::tr("Username: %1").arg(this->username));
    if (!this->originator.isEmpty())
        entries.append(FailedLoginAttemptAlert::tr("Originator: %1").arg(this->originator));
    if (!this->useragent.isEmpty())
        entries.append(FailedLoginAttemptAlert::tr("User agent: %1").arg(this->useragent));

    return QStringLiteral("- ") + entries.join(QStringLiteral(", "));
}
