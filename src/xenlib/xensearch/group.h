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

#ifndef XENSEARCH_GROUP_H
#define XENSEARCH_GROUP_H

#include "../xenlib_global.h"
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QList>
#include <QHash>
#include <QSharedPointer>

// Forward declarations
class IAcceptGroups;
class Grouping;
class Query;
class Search;
class XenConnection;

namespace XenSearch
{
    /**
     * @brief Key for identifying a group in the hierarchy
     *
     * Matches C# GroupAlg.cs GroupKey class
     */
    class XENLIB_EXPORT GroupKey
    {
        public:
            GroupKey(Grouping* grouping, const QVariant& key);

            Grouping* GetGrouping() const { return this->grouping_; }
            QVariant GetKey() const { return this->key_; }

            bool operator==(const GroupKey& other) const;
            bool operator!=(const GroupKey& other) const { return !(*this == other); }

            // Hash function for QHash
            friend uint qHash(const GroupKey& key, uint seed);

        private:
            Grouping* grouping_;
            QVariant key_;
    };

    /**
     * @brief Abstract base class for grouping algorithms
     *
     * Matches C# GroupAlg.cs Group abstract class
     */
    class XENLIB_EXPORT Group
    {
        public:
            virtual ~Group() = default;

            /**
             * @brief Create grouped tree structure from search
             * @param search The search to group
             * @return Root group
             *
             * Matches C# Group.GetGrouped()
             */
            static QSharedPointer<Group> GetGrouped(Search* search);

            /**
             * @brief Compare two objects for sorting
             * @param one First object
             * @param other Second object
             * @param search Search context (for sorting rules)
             * @return -1 if one < other, 0 if equal, 1 if one > other
             *
             * Matches C# Group.Compare()
             */
            static int Compare(const QVariant& one, const QVariant& other, Search* search);

            /**
             * @brief Add object to this group
             * @param objectType Type of object ("vm", "host", etc.)
             * @param objectRef Opaque reference
             * @param objectData Full object data
             *
             * Matches C# Group.Add()
             */
            virtual void Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) = 0;

            /**
             * @brief Populate adapter with grouped items
             * @param adapter UI adapter to populate
             * @return True if any items were added
             *
             * Matches C# Group.Populate()
             */
            virtual bool Populate(IAcceptGroups* adapter);

            /**
             * @brief Populate adapter with grouped items (with indent/expand control)
             * @param adapter UI adapter
             * @param indent Indentation level
             * @param defaultExpand Default expansion state
             * @return True if any items were added
             *
             * Matches C# Group.Populate()
             */
            virtual bool Populate(IAcceptGroups* adapter, int indent, bool defaultExpand) = 0;

            /**
             * @brief Populate for specific group
             * @param adapter UI adapter
             * @param group Group to populate
             * @param indent Indentation level
             * @param defaultExpand Default expansion state
             *
             * Matches C# Group.PopulateFor()
             */
            virtual void PopulateFor(IAcceptGroups* adapter, const GroupKey& group, int indent, bool defaultExpand) = 0;

            /**
             * @brief Get next level group keys
             * @param nextLevel Output list of group keys
             *
             * Matches C# Group.GetNextLevel()
             */
            virtual void GetNextLevel(QList<GroupKey>& nextLevel) = 0;

        protected:
            Group(Search* search);

            static QSharedPointer<Group> GetGroupFor(Grouping* grouping, Search* search, Grouping* subgrouping);

            int Compare(const QVariant& one, const QVariant& other) const;
            int CompareGroupKeys(const GroupKey& one, const GroupKey& other) const;

            void FilterAdd(Query* query, const QString& objectType, const QString& objectRef, const QVariantMap& objectData, XenConnection* conn);

            Search* search_;

        private:
            static void GetGrouped(Search* search, QSharedPointer<Group> group);
            static bool Hide(const QString& objectType, const QString& objectRef, const QVariantMap& objectData, XenConnection* conn);
            static int CompareByType(const QVariantMap& oneData, const QVariantMap& otherData);
            static QString TypeOf(const QString& objectType, const QVariantMap& objectData);
    };

    /**
     * @brief Abstract base for node groups (non-leaf)
     *
     * Matches C# GroupAlg.cs AbstractNodeGroup class
     */
    class XENLIB_EXPORT AbstractNodeGroup : public Group
    {
        public:
            AbstractNodeGroup(Search* search, Grouping* grouping);
            virtual ~AbstractNodeGroup() = default;

            bool Populate(IAcceptGroups* adapter) override;
            bool Populate(IAcceptGroups* adapter, int indent, bool defaultExpand) override;
            void PopulateFor(IAcceptGroups* adapter, const GroupKey& group, int indent, bool defaultExpand) override;
            void GetNextLevel(QList<GroupKey>& nextLevel) override;

            QSharedPointer<Group> FindOrAddSubgroup(Grouping* grouping, const QVariant& o, Grouping* subgrouping);

        protected:
            QHash<GroupKey, QSharedPointer<Group>> grouped_;
            QSharedPointer<Group> ungrouped_;
            Grouping* grouping_;
    };

    /**
     * @brief Node group for general grouping
     *
     * Matches C# GroupAlg.cs NodeGroup class
     */
    class XENLIB_EXPORT NodeGroup : public AbstractNodeGroup
    {
        public:
            NodeGroup(Search* search, Grouping* grouping);

            void Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) override;

        private:
            void AddGrouped(const QString& objectType, const QString& objectRef, const QVariantMap& objectData, const QVariant& group);
            void AddUngrouped(const QString& objectType, const QString& objectRef, const QVariantMap& objectData);
    };

    /**
     * @brief Folder-specific grouping
     *
     * Matches C# GroupAlg.cs FolderGroup class
     */
    class XENLIB_EXPORT FolderGroup : public AbstractNodeGroup
    {
        public:
            FolderGroup(Search* search, Grouping* grouping);

            void Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) override;
    };

    /**
     * @brief Leaf group (contains actual objects)
     *
     * Matches C# GroupAlg.cs LeafGroup class
     */
    class XENLIB_EXPORT LeafGroup : public Group
    {
        public:
            LeafGroup(Search* search);

            void Add(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) override;
            bool Populate(IAcceptGroups* adapter, int indent, bool defaultExpand) override;
            void GetNextLevel(QList<GroupKey>& nextLevel) override;
            void PopulateFor(IAcceptGroups* adapter, const GroupKey& group, int indent, bool defaultExpand) override;

        private:
            struct Item
            {
                QString objectType;
                QString objectRef;
                QVariantMap objectData;
            };

            QList<Item> items_;
    };

} // namespace XenSearch

// Hash function implementation
inline uint qHash(const XenSearch::GroupKey& key, uint seed)
{
    return qHash(key.GetKey().toString(), seed);
}

#endif // XENSEARCH_GROUP_H
