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

// queryscope.h - Query scope for searching objects
// Port of C# xenadmin/XenModel/XenSearch/QueryScope.cs and Common.cs (ObjectTypes enum)
#ifndef QUERYSCOPE_H
#define QUERYSCOPE_H

#include <QString>
#include <QVariantMap>

/**
 * @brief Object types that can be searched
 *
 * Flags enum matching C# XenAdmin.XenSearch.ObjectTypes
 *
 * C# equivalent: xenadmin/XenModel/XenSearch/Common.cs lines 54-74
 * Note: Order determines tree order in Folder View (CA-28418)
 */
enum class ObjectTypes
{
    None = 0,
    Pool = 1 << 0,                           // 1
    Server = 1 << 1,                         // 2 (Host)
    DisconnectedServer = 1 << 2,             // 4
    VM = 1 << 3,                             // 8
    Snapshot = 1 << 4,                       // 16
    UserTemplate = 1 << 5,                   // 32
    DefaultTemplate = 1 << 6,                // 64
    RemoteSR = 1 << 7,                       // 128
    LocalSR = 1 << 8,                        // 256
    VDI = 1 << 9,                            // 512
    Network = 1 << 10,                       // 1024
    Folder = 1 << 11,                        // 2048
    AllIncFolders = (1 << 12) - 1,           // 4095 (all of the above)
    AllExcFolders = AllIncFolders & ~Folder, // 2047 (all except folders)
    Appliance = 1 << 13,                     // 8192
    DockerContainer = 1 << 14                // 16384
};

// Bitwise operators for ObjectTypes flags
inline ObjectTypes operator|(ObjectTypes a, ObjectTypes b)
{
    return static_cast<ObjectTypes>(static_cast<int>(a) | static_cast<int>(b));
}

inline ObjectTypes operator&(ObjectTypes a, ObjectTypes b)
{
    return static_cast<ObjectTypes>(static_cast<int>(a) & static_cast<int>(b));
}

inline ObjectTypes operator~(ObjectTypes a)
{
    return static_cast<ObjectTypes>(~static_cast<int>(a));
}

inline ObjectTypes& operator|=(ObjectTypes& a, ObjectTypes b)
{
    a = a | b;
    return a;
}

inline ObjectTypes& operator&=(ObjectTypes& a, ObjectTypes b)
{
    a = a & b;
    return a;
}

inline bool operator==(ObjectTypes a, int b)
{
    return static_cast<int>(a) == b;
}

inline bool operator!=(ObjectTypes a, int b)
{
    return static_cast<int>(a) != b;
}

// Forward declaration
class XenConnection;

/**
 * @brief Defines the scope of a search query - which object types to include
 *
 * C# equivalent: xenadmin/XenModel/XenSearch/QueryScope.cs
 */
class QueryScope
{
    public:
        /**
         * @brief Constructor with object types
         * @param types ObjectTypes bitmask specifying included types
         * C# equivalent: QueryScope(ObjectTypes types)
         */
        explicit QueryScope(ObjectTypes types);

        /**
         * @brief Get the object types included in this scope
         * @return ObjectTypes bitmask
         * C# equivalent: ObjectTypes property
         */
        ObjectTypes getObjectTypes() const
        {
            return m_types;
        }

        /**
         * @brief Check if this scope wants a specific object
         *
         * C# equivalent: WantType(IXenObject o)
         *
         * @param objectData The object data
         * @param objectType The object type string ("vm", "host", etc.)
         * @param xenLib XenLib instance for resolving references (optional)
         * @return true if the object matches the scope
         */
        bool wantType(const QVariantMap& objectData, const QString& objectType, XenConnection* conn = nullptr) const;

        /**
         * @brief Check if this scope wants a specific type
         *
         * Want type t: or if t is a bitwise OR, want *all* types in t
         * I.e., the types "this" includes are a superset of t
         *
         * C# equivalent: WantType(ObjectTypes t)
         * @param t ObjectTypes to check
         * @return true if this scope includes all types in t
         */
        bool wantType(ObjectTypes t) const;

        /**
         * @brief Check if this scope wants all types in another scope
         * @param q Other QueryScope to compare
         * C# equivalent: WantType(QueryScope q)
         * @return true if this scope includes all types in q
         */
        bool wantType(const QueryScope* q) const;

        /**
         * @brief Check if this scope is a subset of given types
         * The types "this" includes are a subset of t
         *
         * C# equivalent: WantSubsetOf(ObjectTypes t)
         * @param t ObjectTypes to compare against
         * @return true if this scope is a subset of t
         */
        bool wantSubsetOf(ObjectTypes t) const;

        /**
         * @brief Check if this scope is a subset of another scope
         * C# equivalent: WantSubsetOf(QueryScope q)
         * @param q Other QueryScope to compare
         * @return true if this scope is a subset of q
         */
        bool wantSubsetOf(const QueryScope* q) const;

        /**
         * @brief Get the object types included in this scope
         *
         * Exposes ObjectTypes for UI integration (QueryPanel type filtering)
         * C# equivalent: ObjectTypes property
         *
         * @return ObjectTypes flags included in this scope
         */
        ObjectTypes GetObjectTypes() const
        {
            return this->m_types;
        }

        /**
         * @brief Check if this scope wants any of the types in t
         * I.e., the types "this" includes overlap with t
         *
         * C# equivalent: WantAnyOf(ObjectTypes t)
         * @param t ObjectTypes to test for overlap
         * @return true if any types overlap
         */
        bool wantAnyOf(ObjectTypes t) const;

        /**
         * @brief Check if this scope wants any of the types in another scope
         * C# equivalent: WantAnyOf(QueryScope q)
         * @param q Other QueryScope to test for overlap
         * @return true if any types overlap
         */
        bool wantAnyOf(const QueryScope* q) const;

        /**
         * @brief Check if this scope exactly equals given types
         * C# equivalent: Equals(ObjectTypes t)
         * @param t ObjectTypes to compare
         * @return true if exactly equal
         */
        bool equals(ObjectTypes t) const;

        /**
         * @brief Check if this scope exactly equals another scope
         * C# equivalent: Equals(QueryScope q)
         * @param q Other QueryScope to compare
         * @return true if exactly equal
         */
        bool equals(const QueryScope* q) const;

        /**
         * @brief Equality operator
         */
        bool operator==(const QueryScope& other) const;

        /**
         * @brief Hash code for use in QHash
         * C# equivalent: GetHashCode()
         * @return 32-bit hash value
         */
        uint hashCode() const;

    private:
        /**
         * @brief Get the ObjectTypes enum value for a given object
         *
         * C# equivalent: ObjectTypeOf(IXenObject o)
         * Uses PropertyAccessors.Get(PropertyNames.type)
         *
         * @param objectData The object data map
         * @param objectType The object type string (may be empty)
         * @param xenLib XenLib instance for resolving references
         * @return ObjectTypes enum value for the object
         */
        ObjectTypes objectTypeOf(const QVariantMap& objectData, const QString& objectType, XenConnection* conn) const;

        ObjectTypes m_types; // The object types included in this scope
};

// Hash function for QHash support
inline uint qHash(const QueryScope& scope)
{
    return scope.hashCode();
}

#endif // QUERYSCOPE_H
