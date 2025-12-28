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

#ifndef CUSTOMFIELDDEFINITION_H
#define CUSTOMFIELDDEFINITION_H

#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

/**
 * @brief Custom field definition
 *
 * Represents a single custom field definition with name and type.
 * Custom fields are user-defined metadata stored in XenServer object other_config.
 *
 * C# equivalent: XenAdmin/CustomFields/CustomFieldDefinition.cs
 */
class CustomFieldDefinition
{
    public:
        enum class Type
        {
            String,
            Date
        };

        static const QString TAG_NAME;

        CustomFieldDefinition(const QString& name, Type type);

        // XML serialization
        static CustomFieldDefinition fromXml(QXmlStreamReader& xml);
        void toXml(QXmlStreamWriter& xml) const;

        // Getters
        QString getName() const { return this->name_; }
        Type getType() const { return this->type_; }

        // Display formatting
        QString toString() const;
        QString getTypeString() const;

        // Equality/comparison
        bool operator==(const CustomFieldDefinition& other) const;
        bool operator!=(const CustomFieldDefinition& other) const;

        // For use in QHash/QSet
        uint qHash() const;

    private:
        QString name_;
        Type type_;
};

// qHash free function for Qt container support
inline uint qHash(const CustomFieldDefinition& def)
{
    return qHash(def.getName());
}

#endif // CUSTOMFIELDDEFINITION_H
