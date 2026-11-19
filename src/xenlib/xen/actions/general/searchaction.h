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

#ifndef SEARCHACTION_H
#define SEARCHACTION_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"

/**
 * Save a search in, or delete it from, a pool's gui_config
 *
 * Qt port of C# XenAdmin.Actions.SearchAction (Search.Save / Search.Delete). The search is stored
 * as XenCenter XML under "xensearch.Search-<uuid>", so XenCenter sees it too.
 */
class XENLIB_EXPORT SearchAction : public AsyncOperation
{
    Q_OBJECT

    public:
        enum class Operation
        {
            Save,
            Delete
        };

        //! For Save, xml is the search (SearchMarshalling::SearchToXml); unused for Delete
        SearchAction(XenConnection* connection, const QString& uuid, const QString& name,
                     const QString& xml, Operation operation, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_uuid;
        QString m_xml;
        Operation m_operation;
};

#endif // SEARCHACTION_H
