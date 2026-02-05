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

#include "xenapi_Helper.h"
#include "../xenobject.h"

namespace XenAPI
{
    const QString Helper::NullOpaqueRef = XENOBJECT_NULL;

    bool Helper::AreEqual(const QVariant& o1, const QVariant& o2)
    {
        if (!o1.isValid() && !o2.isValid())
            return true;
        if (!o1.isValid() || !o2.isValid())
            return false;

        // Handle maps (dictionaries)
        if (o1.canConvert<QVariantMap>() && o2.canConvert<QVariantMap>())
            return AreDictEqual(o1.toMap(), o2.toMap());

        // Handle lists (collections)
        if (o1.canConvert<QVariantList>() && o2.canConvert<QVariantList>())
            return AreCollectionsEqual(o1.toList(), o2.toList());

        // Compare primitives
        return o1 == o2;
    }

    bool Helper::AreEqual2(const QVariant& o1, const QVariant& o2)
    {
        if (!o1.isValid() && !o2.isValid())
            return true;
        if (!o1.isValid() || !o2.isValid())
        {
            // Consider empty collection and null to be equal
            return (!o1.isValid() && IsEmptyCollection(o2)) ||
                   (!o2.isValid() && IsEmptyCollection(o1));
        }

        // Handle maps (dictionaries)
        if (o1.canConvert<QVariantMap>() && o2.canConvert<QVariantMap>())
            return AreDictEqual(o1.toMap(), o2.toMap());

        // Handle lists (collections)
        if (o1.canConvert<QVariantList>() && o2.canConvert<QVariantList>())
            return AreCollectionsEqual(o1.toList(), o2.toList());

        // Compare primitives
        return o1 == o2;
    }

    bool Helper::IsEmptyCollection(const QVariant& obj)
    {
        if (!obj.isValid())
            return false;

        if (obj.canConvert<QVariantList>())
        {
            QVariantList list = obj.toList();
            return list.isEmpty();
        }

        if (obj.canConvert<QVariantMap>())
        {
            QVariantMap map = obj.toMap();
            return map.isEmpty();
        }

        return false;
    }

    bool Helper::AreDictEqual(const QVariantMap& d1, const QVariantMap& d2)
    {
        if (d1.size() != d2.size())
            return false;

        for (auto it = d1.constBegin(); it != d1.constEnd(); ++it)
        {
            if (!d2.contains(it.key()) || !AreEqual(d2.value(it.key()), it.value()))
                return false;
        }

        return true;
    }

    bool Helper::AreCollectionsEqual(const QVariantList& c1, const QVariantList& c2)
    {
        if (c1.size() != c2.size())
            return false;

        for (int i = 0; i < c1.size(); ++i)
        {
            if (!AreEqual(c1.at(i), c2.at(i)))
                return false;
        }

        return true;
    }

    bool Helper::DictEquals(const QVariantMap& d1, const QVariantMap& d2)
    {
        if (d1.size() != d2.size())
            return false;

        for (auto it = d1.constBegin(); it != d1.constEnd(); ++it)
        {
            if (!d2.contains(it.key()) || !EqualOrEquallyNull(d2.value(it.key()), it.value()))
                return false;
        }

        return true;
    }

    bool Helper::EqualOrEquallyNull(const QVariant& o1, const QVariant& o2)
    {
        if (!o1.isValid())
            return !o2.isValid();
        return o1 == o2;
    }

    bool Helper::IsNullOrEmptyOpaqueRef(const QString& opaqueRef)
    {
        return opaqueRef.isEmpty() || 
               opaqueRef.compare(NullOpaqueRef, Qt::CaseInsensitive) == 0;
    }

    QStringList Helper::RefListToStringArray(const QVariantList& opaqueRefs)
    {
        QStringList result;
        result.reserve(opaqueRefs.size());

        for (const QVariant& ref : opaqueRefs)
        {
            result << ref.toString();
        }

        return result;
    }

    QStringList Helper::ObjectListToStringArray(const QVariantList& list)
    {
        QStringList result;
        result.reserve(list.size());

        for (const QVariant& item : list)
        {
            result << item.toString();
        }

        return result;
    }

    QList<qint64> Helper::StringArrayToLongArray(const QStringList& input)
    {
        QList<qint64> result;
        result.reserve(input.size());

        for (const QString& str : input)
        {
            bool ok;
            qint64 value = str.toLongLong(&ok);
            if (ok)
            {
                result << value;
            } else
            {
                result << 0; // Default value on parse failure
            }
        }

        return result;
    }

    QStringList Helper::LongArrayToStringArray(const QList<qint64>& input)
    {
        QStringList result;
        result.reserve(input.size());

        for (qint64 value : input)
        {
            result << QString::number(value);
        }

        return result;
    }

} // namespace XenAPI
