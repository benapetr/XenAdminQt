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

/// <summary>
/// Object types that can be searched
/// Flags enum matching C# XenAdmin.XenSearch.ObjectTypes
///
/// C# equivalent: xenadmin/XenModel/XenSearch/Common.cs lines 54-74
/// Note: Order determines tree order in Folder View (CA-28418)
/// </summary>
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
class XenLib;

/// <summary>
/// Defines the scope of a search query - which object types to include
///
/// C# equivalent: xenadmin/XenModel/XenSearch/QueryScope.cs
/// </summary>
class QueryScope
{
public:
    /// <summary>
    /// Constructor with object types
    /// C# equivalent: QueryScope(ObjectTypes types)
    /// </summary>
    explicit QueryScope(ObjectTypes types);

    /// <summary>
    /// Get the object types included in this scope
    /// C# equivalent: ObjectTypes property
    /// </summary>
    ObjectTypes getObjectTypes() const
    {
        return m_types;
    }

    /// <summary>
    /// Check if this scope wants a specific object
    /// C# equivalent: WantType(IXenObject o)
    /// </summary>
    /// <param name="objectData">The object data</param>
    /// <param name="objectType">The object type string ("vm", "host", etc.)</param>
    /// <param name="xenLib">XenLib instance for resolving references</param>
    bool wantType(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib = nullptr) const;

    /// <summary>
    /// Check if this scope wants a specific type
    /// Want type t: or if t is a bitwise OR, want *all* types in t
    /// I.e., the types "this" includes are a superset of t
    ///
    /// C# equivalent: WantType(ObjectTypes t)
    /// </summary>
    bool wantType(ObjectTypes t) const;

    /// <summary>
    /// Check if this scope wants all types in another scope
    /// C# equivalent: WantType(QueryScope q)
    /// </summary>
    bool wantType(const QueryScope* q) const;

    /// <summary>
    /// Check if this scope is a subset of given types
    /// The types "this" includes are a subset of t
    ///
    /// C# equivalent: WantSubsetOf(ObjectTypes t)
    /// </summary>
    bool wantSubsetOf(ObjectTypes t) const;

    /// <summary>
    /// Check if this scope is a subset of another scope
    /// C# equivalent: WantSubsetOf(QueryScope q)
    /// </summary>
    bool wantSubsetOf(const QueryScope* q) const;

    /// <summary>
    /// Check if this scope wants any of the types in t
    /// I.e., the types "this" includes overlap with t
    ///
    /// C# equivalent: WantAnyOf(ObjectTypes t)
    /// </summary>
    bool wantAnyOf(ObjectTypes t) const;

    /// <summary>
    /// Check if this scope wants any of the types in another scope
    /// C# equivalent: WantAnyOf(QueryScope q)
    /// </summary>
    bool wantAnyOf(const QueryScope* q) const;

    /// <summary>
    /// Check if this scope exactly equals given types
    /// C# equivalent: Equals(ObjectTypes t)
    /// </summary>
    bool equals(ObjectTypes t) const;

    /// <summary>
    /// Check if this scope exactly equals another scope
    /// C# equivalent: Equals(QueryScope q)
    /// </summary>
    bool equals(const QueryScope* q) const;

    /// <summary>
    /// Equality operator
    /// </summary>
    bool operator==(const QueryScope& other) const;

    /// <summary>
    /// Hash code for use in QHash
    /// C# equivalent: GetHashCode()
    /// </summary>
    uint hashCode() const;

private:
    /// <summary>
    /// Get the ObjectTypes enum value for a given object
    /// C# equivalent: ObjectTypeOf(IXenObject o)
    /// Uses PropertyAccessors.Get(PropertyNames.type)
    /// </summary>
    ObjectTypes objectTypeOf(const QVariantMap& objectData, const QString& objectType, XenLib* xenLib) const;

    ObjectTypes m_types; // The object types included in this scope
};

// Hash function for QHash support
inline uint qHash(const QueryScope& scope)
{
    return scope.hashCode();
}

#endif // QUERYSCOPE_H
