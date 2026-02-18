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

#ifndef SAVECHANGESACTION_H
#define SAVECHANGESACTION_H

#include "../../asyncoperation.h"
#include "xen/xenobjecttype.h"
#include <QVariantMap>

class XenObject;

/**
 * @brief SaveChangesAction - Persist cloned object metadata changes
 *
 * Qt counterpart to XenAdmin C# SaveChangesAction. Applies simple
 * XenObject metadata changes (name/description/other_config/platform/etc.)
 * against the target object by comparing before/copy snapshots.
 */
class XENLIB_EXPORT SaveChangesAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit SaveChangesAction(QSharedPointer<XenObject> object,
                                   const QVariantMap& objectDataBefore,
                                   const QVariantMap& objectDataCopy,
                                   bool suppressHistory = true,
                                   QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        void throwIfJsonError(const QString& context) const;

        QSharedPointer<XenObject> m_object;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;
        QString m_objectRef;
        XenObjectType m_objectType = XenObjectType::Null;
};

#endif // SAVECHANGESACTION_H
