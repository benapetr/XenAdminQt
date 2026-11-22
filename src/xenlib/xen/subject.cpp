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

#include "subject.h"
#include "friendlynames.h"
#include "network/connection.h"
#include "../xencache.h"
#include <QCoreApplication>

const char* const Subject::SUBJECT_NAME_KEY = "subject-name";
const char* const Subject::SUBJECT_DISPLAYNAME_KEY = "subject-displayname";
const char* const Subject::SUBJECT_IS_GROUP_KEY = "subject-is-group";

Subject::Subject(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString Subject::SubjectIdentifier() const
{
    return this->stringProperty("subject_identifier");
}

QStringList Subject::RoleRefs() const
{
    return this->property("roles").toStringList();
}

QString Subject::DisplayName() const
{
    return this->GetOtherConfig().value(SUBJECT_DISPLAYNAME_KEY).toString();
}

QString Subject::SubjectName() const
{
    return this->GetOtherConfig().value(SUBJECT_NAME_KEY).toString();
}

bool Subject::IsGroup() const
{
    // C# bool.TryParse: "true"/"false" in any case
    return this->GetOtherConfig().value(SUBJECT_IS_GROUP_KEY).toString().compare("true", Qt::CaseInsensitive) == 0;
}

QString Subject::GetName() const
{
    return this->SubjectName();
}

QList<QPair<QString, QString>> Subject::ExtractKvpInfo(const QVariantMap& otherConfig)
{
    QList<QPair<QString, QString>> result;
    QVariantMap info = otherConfig;
    for (const char* key : {"subject-gid", "subject-uid", "subject-sid", "subject-is-group", "subject-gecos", "subject-upn"})
        info.remove(key);
    if (info.isEmpty())
        return result;

    if (info.contains(SUBJECT_DISPLAYNAME_KEY))
    {
        result.append(qMakePair(info.take(SUBJECT_DISPLAYNAME_KEY).toString(), QString()));
    }
    if (info.contains(SUBJECT_NAME_KEY))
    {
        result.append(qMakePair(info.take(SUBJECT_NAME_KEY).toString(), QString()));
    }
    result.append(qMakePair(QString(), QString()));

    for (auto it = info.constBegin(); it != info.constEnd(); ++it)
    {
        QString keyText = FriendlyNames::getString(QString("AD.PropertyKey-%1").arg(it.key()));
        if (keyText.isEmpty())
            keyText = it.key();
        QString value = it.value().toString();
        if (value == "true")
            value = QCoreApplication::translate("Subject", "Yes");
        else if (value == "false")
            value = QCoreApplication::translate("Subject", "No");
        result.append(qMakePair(keyText + QCoreApplication::translate("Subject", ": "), value));
    }
    return result;
}

QSharedPointer<Subject> Subject::GetBySID(XenConnection* connection, const QString& sid)
{
    if (!connection || !connection->GetCache())
        return QSharedPointer<Subject>();
    for (const QSharedPointer<Subject>& subject : connection->GetCache()->GetAll<Subject>(XenObjectType::Subject))
    {
        if (subject && subject->SubjectIdentifier() == sid)
            return subject;
    }
    return QSharedPointer<Subject>();
}
