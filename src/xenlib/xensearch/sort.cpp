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

int Sort::Compare(XenObject* one, XenObject* other) const
{
    // C# equivalent: Compare method (Sort.cs lines 121-157)
    // C# code handles:
    // - Special case for "name" column (natural string comparison)
    // - Custom fields
    // - Property accessors for other columns
    //
    // Simplified Qt implementation for now - just compare by column name

    if (!one && !other)
        return 0;
    if (!one)
        return this->ascending_ ? -1 : 1;
    if (!other)
        return this->ascending_ ? 1 : -1;

    // Get property values from XenObject
    QVariant v1, v2;

    // Map column names to XenObject properties
    // C# uses PropertyAccessors.Get(propertyName) which returns a delegate
    // We directly access XenObject properties instead
    if (this->column_ == "name")
    {
        v1 = one->GetName();
        v2 = other->GetName();
    }
    else if (this->column_ == "cpu" || this->column_ == "cpus")
    {
        // For VMs: number of vCPUs
        // For Hosts: number of CPUs
        v1 = one->GetData().value("VCPUs_max", 0);
        v2 = other->GetData().value("VCPUs_max", 0);
    }
    else if (this->column_ == "memory")
    {
        // Memory in bytes
        v1 = one->GetData().value("memory_static_max", 0LL);
        v2 = other->GetData().value("memory_static_max", 0LL);
    }
    else if (this->column_ == "disks")
    {
        // Number of VBDs (virtual block devices)
        QVariantList vbds1 = one->GetData().value("VBDs", QVariantList()).toList();
        QVariantList vbds2 = other->GetData().value("VBDs", QVariantList()).toList();
        v1 = vbds1.size();
        v2 = vbds2.size();
    }
    else if (this->column_ == "network" || this->column_ == "networks")
    {
        // Number of VIFs (virtual network interfaces)
        QVariantList vifs1 = one->GetData().value("VIFs", QVariantList()).toList();
        QVariantList vifs2 = other->GetData().value("VIFs", QVariantList()).toList();
        v1 = vifs1.size();
        v2 = vifs2.size();
    }
    else if (this->column_ == "ha")
    {
        // HA restart priority
        v1 = one->GetData().value("ha_restart_priority", QString(""));
        v2 = other->GetData().value("ha_restart_priority", QString(""));
    }
    else if (this->column_ == "uptime")
    {
        // Start time (as timestamp)
        v1 = one->GetData().value("start_time", QString(""));
        v2 = other->GetData().value("start_time", QString(""));
    }
    else if (this->column_ == "ip" || this->column_ == "ip_address")
    {
        // IP address
        // For VMs: get from guest metrics
        // For now, use simple string comparison
        v1 = one->GetData().value("ip_address", QString(""));
        v2 = other->GetData().value("ip_address", QString(""));
    }
    else
    {
        // Generic property access
        v1 = one->GetData().value(this->column_, QVariant());
        v2 = other->GetData().value(this->column_, QVariant());
    }

    // Compare values
    int result = 0;

    // Handle null/invalid values
    if (!v1.isValid() && !v2.isValid())
        result = 0;
    else if (!v1.isValid())
        result = -1;
    else if (!v2.isValid())
        result = 1;
    else
    {
        // Compare based on type
        if (v1.type() == QVariant::String && v2.type() == QVariant::String)
        {
            // String comparison
            QString s1 = v1.toString();
            QString s2 = v2.toString();

            // For "name" column, use natural comparison (like C# StringUtility.NaturalCompare)
            // For now, use case-insensitive comparison
            if (this->column_ == "name")
                result = QString::compare(s1, s2, Qt::CaseInsensitive);
            else
                result = QString::compare(s1, s2);
        }
        else if (v1.type() == QVariant::Int || v1.type() == QVariant::LongLong)
        {
            // Numeric comparison
            qint64 n1 = v1.toLongLong();
            qint64 n2 = v2.toLongLong();
            if (n1 < n2)
                result = -1;
            else if (n1 > n2)
                result = 1;
            else
                result = 0;
        }
        else if (v1.type() == QVariant::Double)
        {
            // Double comparison
            double d1 = v1.toDouble();
            double d2 = v2.toDouble();
            if (d1 < d2)
                result = -1;
            else if (d1 > d2)
                result = 1;
            else
                result = 0;
        }
        else
        {
            // Default: convert to string and compare
            QString s1 = v1.toString();
            QString s2 = v2.toString();
            result = QString::compare(s1, s2);
        }
    }

    // Apply ascending/descending
    if (!this->ascending_)
        result = -result;

    return result;
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
