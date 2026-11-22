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

#ifndef SUBJECT_H
#define SUBJECT_H

#include "xenobject.h"
#include <QStringList>

/**
 * A user or group from an external directory (Active Directory) allowed to log in to the pool.
 * C# XenAPI.Subject (XenModel/XenAPI/Subject2.cs, XenAPI-Extensions/Subject.cs)
 */
class XENLIB_EXPORT Subject : public XenObject
{
    Q_OBJECT

    public:
        static const char* const SUBJECT_NAME_KEY;         //!< "subject-name"
        static const char* const SUBJECT_DISPLAYNAME_KEY;  //!< "subject-displayname"
        static const char* const SUBJECT_IS_GROUP_KEY;     //!< "subject-is-group"

        explicit Subject(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        XenObjectType GetObjectType() const override { return XenObjectType::Subject; }

        //! The directory's identifier for the user or group (its SID for AD)
        QString SubjectIdentifier() const;
        //! Refs of the roles granted to the subject
        QStringList RoleRefs() const;
        //! other_config subject-displayname, or empty if there's none (C# null)
        QString DisplayName() const;
        //! other_config subject-name, or empty if there's none (C# null)
        QString SubjectName() const;
        //! other_config subject-is-group parsed as a bool, false if absent
        bool IsGroup() const;
        //! C# Subject.Name(): the subject name
        QString GetName() const override;

        /**
         * C# Subject.ExtractKvpInfo: what the AD tab's details show. Display name and name
         * first as headings (empty values), a blank separator, then the other properties
         * with friendly names and Yes/No for booleans.
         */
        static QList<QPair<QString, QString>> ExtractKvpInfo(const QVariantMap& otherConfig);

        //! C# Subject.GetBySID
        static QSharedPointer<Subject> GetBySID(XenConnection* connection, const QString& sid);
};

#endif // SUBJECT_H
