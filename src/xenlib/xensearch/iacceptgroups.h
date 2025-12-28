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

// iacceptgroups.h - Interface for accepting grouped objects
// Port of C# xenadmin/XenModel/XenSearch/GroupAlg.cs - IAcceptGroups interface
#ifndef IACCEPTGROUPS_H
#define IACCEPTGROUPS_H

#include <QVariant>

// Forward declarations
class Grouping;
class XenConnection;

/**
 * @brief Interface for UI adapters that accept grouped objects
 *
 * C# equivalent: IAcceptGroups interface in GroupAlg.cs
 *
 * This interface allows the Search.PopulateAdapters() method to delegate
 * the actual tree/list building to UI-specific adapters without knowing
 * the details of the UI framework.
 *
 * Flow:
 * 1. Search::PopulateAdapters() filters objects and organizes into groups
 * 2. For each group, it calls adapter->Add(grouping, groupValue, indent)
 * 3. Adapter creates UI element (tree node, list item, etc.)
 * 4. Adapter returns a new IAcceptGroups for adding children
 * 5. When group is complete, FinishedInThisGroup() is called
 */
class IAcceptGroups
{
    public:
        virtual ~IAcceptGroups() = default;

        /**
        * @brief Add a group or object to the adapter
        *
        * C# equivalent: IAcceptGroups.Add(Grouping grouping, Object group, int indent)
        *
        * @param grouping The grouping algorithm that created this group
        * @param group The group value (e.g., "pool-ref", "host-ref", or object ref for leaf items)
        * @param objectType The type of the group/object ("pool", "host", "vm", etc.). Empty for group headers.
        * @param objectData Full object data map (for leaf items). Empty for group headers.
        * @param indent Indentation level (0=top level, 1=first child, etc.)
        * @param xenLib XenLib instance for resolving object data
        * @return New IAcceptGroups instance for adding children, or nullptr if no children
        */
        virtual IAcceptGroups* Add(Grouping* grouping, const QVariant& group, 
                                const QString& objectType, const QVariantMap& objectData,
                                int indent, XenConnection* conn) = 0;

        /**
        * @brief Called when all items in this group have been added
        *
        * C# equivalent: IAcceptGroups.FinishedInThisGroup(bool defaultExpand)
        *
        * @param defaultExpand True if this group should be expanded by default
        */
        virtual void FinishedInThisGroup(bool defaultExpand) = 0;
};

#endif // IACCEPTGROUPS_H
