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


#ifndef CUSTOMFIELDACTIONS_H
#define CUSTOMFIELDACTIONS_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"
#include "../../../customfields/customfielddefinition.h"
#include <QList>
#include <QPair>
#include <QSharedPointer>
#include <QVariant>

class XenObject;

/**
 * Add a custom field definition to the pool's gui_config list
 *
 * Qt port of C# XenAdmin.Actions.AddCustomFieldAction. The list is shared with XenCenter
 * (gui_config key "XenCenter.CustomFields").
 */
class XENLIB_EXPORT AddCustomFieldAction : public AsyncOperation
{
    Q_OBJECT

    public:
        AddCustomFieldAction(XenConnection* connection, const CustomFieldDefinition& definition, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        CustomFieldDefinition m_definition;
};

/**
 * Remove a custom field definition from the pool, and its value from every object
 *
 * Qt port of C# XenAdmin.Actions.RemoveCustomFieldAction (CustomFieldsManager.RemoveCustomField).
 */
class XENLIB_EXPORT RemoveCustomFieldAction : public AsyncOperation
{
    Q_OBJECT

    public:
        RemoveCustomFieldAction(XenConnection* connection, const CustomFieldDefinition& definition, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        CustomFieldDefinition m_definition;
};

/**
 * Set custom field values on one object; a null value removes the field from it
 *
 * Qt port of C# XenAdmin.Actions.SaveCustomFieldsAction.
 */
class XENLIB_EXPORT SaveCustomFieldsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        using FieldValue = QPair<CustomFieldDefinition, QVariant>;

        SaveCustomFieldsAction(QSharedPointer<XenObject> object, const QList<FieldValue>& values, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<XenObject> m_object;
        QList<FieldValue> m_values;
};

#endif // CUSTOMFIELDACTIONS_H
