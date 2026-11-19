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


#ifndef NEWCUSTOMFIELDDIALOG_H
#define NEWCUSTOMFIELDDIALOG_H

#include <QDialog>
#include <QList>
#include "xenlib/customfields/customfielddefinition.h"

class XenConnection;

namespace Ui
{
    class NewCustomFieldDialog;
}

/**
 * @brief Name and type for a new custom field
 *
 * Qt port of C# XenAdmin.Dialogs.NewCustomFieldDialog: OK is enabled for a non-blank name that
 * the pool does not already define.
 */
class NewCustomFieldDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit NewCustomFieldDialog(XenConnection* connection, QWidget* parent = nullptr);
        ~NewCustomFieldDialog() override;

        //! The trimmed name and the chosen type
        CustomFieldDefinition GetDefinition() const;

        //! The fields the name must not repeat (by default the connection's)
        void SetExistingFields(const QList<CustomFieldDefinition>& fields);

    private:
        void updateControls();

        Ui::NewCustomFieldDialog* ui;
        QList<CustomFieldDefinition> m_existingFields;
};

#endif // NEWCUSTOMFIELDDIALOG_H
