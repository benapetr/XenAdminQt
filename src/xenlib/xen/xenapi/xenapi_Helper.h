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

#ifndef XENAPI_HELPER_H
#define XENAPI_HELPER_H

#include "../../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace XenAPI
{
    /**
     * @brief XenAPI Helper - Utility functions for XenAPI operations
     *
     * Static-only class providing utility functions for comparing objects,
     * converting between types, and handling opaque references.
     * Matches C# XenModel/XenAPI/Helper.cs structure.
     */
    class XENLIB_EXPORT Helper
    {
        private:
            Helper() = delete; // Static-only class

        public:
            static const QString NullOpaqueRef;

            /**
             * @brief Test if two QVariants are equal
             * @param o1 First object
             * @param o2 Second object
             * @return True if equal (handling collections, maps, and primitives)
             *
             * Matches C# Helper.AreEqual()
             */
            static bool AreEqual(const QVariant& o1, const QVariant& o2);

            /**
             * @brief Test if two QVariants are equal (treating empty collections as equal to null)
             * @param o1 First object
             * @param o2 Second object
             * @return True if equal
             *
             * Matches C# Helper.AreEqual2()
             * Different from AreEqual in that this considers an empty Collection and null to be equal
             */
            static bool AreEqual2(const QVariant& o1, const QVariant& o2);

            /**
             * @brief Test if two dictionaries are equal
             * @param d1 First dictionary
             * @param d2 Second dictionary
             * @return True if equal
             *
             * Matches C# Helper.DictEquals()
             */
            static bool DictEquals(const QVariantMap& d1, const QVariantMap& d2);

            /**
             * @brief Check if opaque reference is null or empty
             * @param opaqueRef Reference to check
             * @return True if null, empty, or "OpaqueRef:NULL"
             *
             * Matches C# Helper.IsNullOrEmptyOpaqueRef()
             */
            static bool IsNullOrEmptyOpaqueRef(const QString& opaqueRef);

            /**
             * @brief Convert list of opaque references to string list
             * @param opaqueRefs List of opaque references
             * @return QStringList
             *
             * Matches C# Helper.RefListToStringArray()
             */
            static QStringList RefListToStringArray(const QVariantList& opaqueRefs);

            /**
             * @brief Convert object list to string list by calling toString()
             * @param list List of objects
             * @return QStringList
             *
             * Matches C# Helper.ObjectListToStringArray()
             */
            static QStringList ObjectListToStringArray(const QVariantList& list);

            /**
             * @brief Parse array of strings into list of longs
             * @param input String list
             * @return List of longs
             *
             * Matches C# Helper.StringArrayToLongArray()
             */
            static QList<qint64> StringArrayToLongArray(const QStringList& input);

            /**
             * @brief Convert array of longs to string list
             * @param input List of longs
             * @return QStringList
             *
             * Matches C# Helper.LongArrayToStringArray()
             */
            static QStringList LongArrayToStringArray(const QList<qint64>& input);

        private:
            static bool IsEmptyCollection(const QVariant& obj);
            static bool AreDictEqual(const QVariantMap& d1, const QVariantMap& d2);
            static bool AreCollectionsEqual(const QVariantList& c1, const QVariantList& c2);
            static bool EqualOrEquallyNull(const QVariant& o1, const QVariant& o2);
    };

} // namespace XenAPI

#endif // XENAPI_HELPER_H
