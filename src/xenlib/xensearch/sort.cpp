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

// sort.cpp - Implementation of Sort
#include "sort.h"
#include "xen/xenobject.h"
#include "common.h"
#include "../customfields/customfieldsmanager.h"
#include "../utils/misc.h"
#include <QDateTime>
#include <QHash>
#include <QScopedPointer>

using namespace XenSearch;
#include <QDebug>

Sort::Sort(const QString& column, bool ascending)
    : column_(column), ascending_(ascending)
{
    // C# equivalent: Sort constructor (Sort.cs lines 48-53)
    // C# code:
    //   public Sort(string column, bool ascending)
    //   {
    //       this.column = column;
    //       this.ascending = ascending;
    //       this.property = CalcProperty();
    //   }
    //
    // Note: We don't implement CalcProperty() yet - comparison is done directly
    // in Compare() method using XenObject's property accessors
}

Sort::Sort()
    : column_(""), ascending_(true)
{
    // Default constructor for container use
}

Sort::Sort(const Sort& other)
    : column_(other.column_), ascending_(other.ascending_)
{
    // Copy constructor
}

Sort& Sort::operator=(const Sort& other)
{
    if (this != &other)
    {
        this->column_ = other.column_;
        this->ascending_ = other.ascending_;
    }
    return *this;
}

Sort::~Sort()
{
    // Nothing to clean up
}

Sort::Sort(QXmlStreamReader& reader)
{
    // C# equivalent: Sort(XmlNode node) (Sort.cs lines 105-108)
    // C# code:
    //   public Sort(XmlNode node)
    //       : this(node.Attributes["column"].Value, SearchMarshalling.ParseBool(node.Attributes["ascending"].Value))
    //   {
    //   }

    // Read attributes
    QXmlStreamAttributes attributes = reader.attributes();

    if (attributes.hasAttribute("column"))
        this->column_ = attributes.value("column").toString();
    else
        this->column_ = "";

    if (attributes.hasAttribute("ascending"))
    {
        QString ascStr = attributes.value("ascending").toString().toLower();
        this->ascending_ = (ascStr == "true" || ascStr == "1");
    } else
    {
        this->ascending_ = true;
    }
}

void Sort::ToXml(QXmlStreamWriter& writer) const
{
    // C# equivalent: ToXmlNode (Sort.cs lines 110-119)
    // C# code:
    //   public XmlNode ToXmlNode(XmlDocument doc)
    //   {
    //       XmlNode node = doc.CreateElement("sort");
    //       SearchMarshalling.AddAttribute(doc, node, "column", column);
    //       SearchMarshalling.AddAttribute(doc, node, "ascending", ascending);
    //       return node;
    //   }

    writer.writeStartElement("sort");
    writer.writeAttribute("column", this->column_);
    writer.writeAttribute("ascending", this->ascending_ ? "true" : "false");
    writer.writeEndElement();
}

namespace
{
    const QString CUSTOM_FIELD_PREFIX = QStringLiteral("CustomField:");  // C# CustomFieldsManager.CUSTOM_FIELD

    bool isNumber(const QVariant& v)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        switch (v.typeId())
#else
        switch (static_cast<int>(v.type()))
#endif
        {
            case QMetaType::Int:
            case QMetaType::UInt:
            case QMetaType::LongLong:
            case QMetaType::ULongLong:
            case QMetaType::Double:
            case QMetaType::Float:
                return true;
            default:
                return false;
        }
    }

    // C# IComparable.CompareTo for the values the sort properties return
    int compareValues(const QVariant& v1, const QVariant& v2)
    {
        if (isNumber(v1) && isNumber(v2))
        {
            const double d1 = v1.toDouble();
            const double d2 = v2.toDouble();
            return d1 < d2 ? -1 : (d1 > d2 ? 1 : 0);
        }
        if (v1.userType() == QMetaType::QDateTime && v2.userType() == QMetaType::QDateTime)
        {
            const QDateTime t1 = v1.toDateTime();
            const QDateTime t2 = v2.toDateTime();
            return t1 < t2 ? -1 : (t1 > t2 ? 1 : 0);
        }
        return QString::localeAwareCompare(v1.toString(), v2.toString());
    }
}

QVariant Sort::propertyOf(XenObject* o) const
{
    // C# Sort.CalcProperty
    if (this->column_.startsWith(CUSTOM_FIELD_PREFIX))
    {
        const QString fieldName = this->column_.mid(CUSTOM_FIELD_PREFIX.length());
        QScopedPointer<CustomFieldDefinition> definition(CustomFieldsManager::instance()->getCustomFieldDefinition(fieldName));
        if (!definition)  // a custom field that existed when the search was saved but no longer does
            return QVariant();

        const QVariant value = CustomFieldsManager::GetCustomFieldValue(o->GetOtherConfig(), *definition);
        if (definition->GetType() == CustomFieldDefinition::Type::Date)
            return value.userType() == QMetaType::QDateTime ? value : QVariant();
        return value.isValid() && !value.isNull() ? QVariant(value.toString()) : QVariant();
    }

    static const QHash<QString, ColumnNames> columns = {
        {"name", ColumnNames::name},       {"cpu", ColumnNames::cpu},
        {"memory", ColumnNames::memory},   {"disks", ColumnNames::disks},
        {"network", ColumnNames::network}, {"ha", ColumnNames::ha},
        {"ip", ColumnNames::ip},           {"uptime", ColumnNames::uptime},
    };
    if (!columns.contains(this->column_))
        return QVariant();

    auto property = PropertyAccessors::Get(PropertyAccessors::GetSortPropertyName(columns.value(this->column_)));
    return property ? property(o) : QVariant();
}

int Sort::Compare(XenObject* one, XenObject* other) const
{
    return this->compare(one, QVariant(), other, QVariant());
}

int Sort::Compare(const QVariant& one, const QVariant& other) const
{
    return this->compare(nullptr, one, nullptr, other);
}

int Sort::compare(XenObject* o1, const QVariant& v1, XenObject* o2, const QVariant& v2) const
{
    // C# Sort.Compare. Objects are compared by the column's sort property; other values
    // (group keys such as tags) only by the name column, as their text.
    const bool nameSort = this->column_ == "name";

    QVariant c1, c2;
    if (o1)
        c1 = this->propertyOf(o1);
    else if (nameSort)
        c1 = v1.userType() == QMetaType::QDateTime ? v1.toDateTime().toUTC().toString("yyyy-MM-dd HH:mm:ssZ") : v1.toString();

    if (o2)
        c2 = this->propertyOf(o2);
    else if (nameSort)
        c2 = v2.userType() == QMetaType::QDateTime ? v2.toDateTime().toUTC().toString("yyyy-MM-dd HH:mm:ssZ") : v2.toString();

    const bool null1 = !c1.isValid() || c1.isNull();
    const bool null2 = !c2.isValid() || c2.isNull();
    if (null1 && null2)
        return 0;
    if (null1)
        return this->ascending_ ? -1 : 1;
    if (null2)
        return this->ascending_ ? 1 : -1;

    int r = nameSort ? Misc::NaturalCompare(c1.toString(), c2.toString()) : compareValues(c1, c2);
    if (!this->ascending_)
        r = -r;
    return r;
}

bool Sort::operator==(const Sort& other) const
{
    // C# equivalent: Equals override (Sort.cs lines 159-167)
    // C# code:
    //   public override bool Equals(object obj)
    //   {
    //       Sort other = obj as Sort;
    //       if (other == null)
    //           return false;
    //       return column == other.column && ascending == other.ascending;
    //   }

    return this->column_ == other.column_ && this->ascending_ == other.ascending_;
}
