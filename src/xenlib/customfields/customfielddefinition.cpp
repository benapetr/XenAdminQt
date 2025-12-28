/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * Copyright (c) Cloud Software Group, Inc.
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

#include "customfielddefinition.h"

const QString CustomFieldDefinition::TAG_NAME = "CustomFieldDefinition";

CustomFieldDefinition::CustomFieldDefinition(const QString& name, Type type)
    : name_(name)
    , type_(type)
{
}

CustomFieldDefinition CustomFieldDefinition::fromXml(QXmlStreamReader& xml)
{
    // Read attributes
    QXmlStreamAttributes attributes = xml.attributes();
    QString name = attributes.value("name").toString();
    QString typeStr = attributes.value("type").toString();

    Type type = Type::String;
    if (typeStr == "Date")
    {
        type = Type::Date;
    }

    // Skip to end element
    xml.skipCurrentElement();

    return CustomFieldDefinition(name, type);
}

void CustomFieldDefinition::toXml(QXmlStreamWriter& xml) const
{
    xml.writeStartElement(TAG_NAME);
    xml.writeAttribute("name", this->name_);
    xml.writeAttribute("type", this->type_ == Type::Date ? "Date" : "String");
    xml.writeAttribute("defaultValue", ""); // Legacy compatibility (CA-37473)
    xml.writeEndElement();
}

QString CustomFieldDefinition::toString() const
{
    QString typeString = this->getTypeString();
    return QString("%1 (%2)").arg(this->name_, typeString);
}

QString CustomFieldDefinition::getTypeString() const
{
    return this->type_ == Type::Date ? "Date and Time" : "Text";
}

bool CustomFieldDefinition::operator==(const CustomFieldDefinition& other) const
{
    return this->name_ == other.name_ && this->type_ == other.type_;
}

bool CustomFieldDefinition::operator!=(const CustomFieldDefinition& other) const
{
    return !(*this == other);
}

uint CustomFieldDefinition::qHash() const
{
    return ::qHash(this->name_);
}
