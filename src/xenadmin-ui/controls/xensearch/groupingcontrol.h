/* Copyright (c) Cloud Software Group, Inc.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met: 
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer. 
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#ifndef GROUPINGCONTROL_H
#define GROUPINGCONTROL_H

#include <QWidget>
#include <QPushButton>
#include <QMenu>
#include <QList>
#include <QString>
#include <QPoint>
#include "../dropdownbutton.h"
#include "xenlib/xensearch/grouping.h"

// Forward declarations
class Searcher;

/**
 * GroupingControl - Widget for managing search result grouping
 * 
 * Provides a UI for selecting and managing grouping criteria.
 * Users can add multiple grouping levels and rearrange them by dragging.
 * 
 * Based on C# xenadmin/XenAdmin/Controls/XenSearch/GroupingControl.cs
 */
class GroupingControl : public QWidget
{
    Q_OBJECT

    public:
        explicit GroupingControl(QWidget* parent = nullptr);
        ~GroupingControl();

        /**
         * Get the current grouping hierarchy
         */
        Grouping* GetGrouping() const;

        /**
         * Set the grouping hierarchy
         */
        void SetGrouping(Grouping* grouping);

        /**
         * Set the associated Searcher (for filtering applicable grouping types)
         */
        void SetSearcher(Searcher* searcher);

    signals:
        /**
         * Emitted when the grouping configuration changes
         */
        void GroupingChanged();

    private slots:
        void onGroupButtonClicked();
        void onAddGroupButtonClicked();
        void onGroupButtonMenuTriggered(QAction* action);
        void onAddGroupMenuTriggered(QAction* action);
        void onSearchForChanged();

    private:
        /**
         * GroupingType - Base class for grouping type definitions
         */
        class GroupingType
        {
            public:
                GroupingType(const QString& name, int appliesTo)
                    : name_(name), appliesTo_(appliesTo) {}
                virtual ~GroupingType() {}

                QString GetName() const { return this->name_; }
                int GetAppliesTo() const { return this->appliesTo_; }

                virtual Grouping* GetGroup(Grouping* subgrouping) const = 0;
                virtual bool ForGrouping(const Grouping* grouping) const = 0;
                virtual bool IsDescendantOf(const GroupingType* gt) const { Q_UNUSED(gt); return false; }

            protected:
                QString name_;
                int appliesTo_;
        };

        /**
         * PropertyGroupingType - Grouping by object property
         */
        class PropertyGroupingType : public GroupingType
        {
            public:
                PropertyGroupingType(const QString& name, const QString& property, int appliesTo)
                    : GroupingType(name, appliesTo), property_(property) {}

                Grouping* GetGroup(Grouping* subgrouping) const override;
                bool ForGrouping(const Grouping* grouping) const override;

            protected:
                QString property_;
            };

            /**
             * XenObjectPropertyGroupingType - Grouping by XenObject reference property
             */
            class XenObjectPropertyGroupingType : public PropertyGroupingType
            {
            public:
                XenObjectPropertyGroupingType(const QString& name, const QString& property,
                                              int appliesTo, GroupingType* parent = nullptr)
                    : PropertyGroupingType(name, property, appliesTo), parent_(parent) {}

                Grouping* GetGroup(Grouping* subgrouping) const override;
                bool IsDescendantOf(const GroupingType* gt) const override;

            private:
                GroupingType* parent_;
        };

        /**
         * FolderGroupingType - Grouping by folder
         */
        class FolderGroupingType : public GroupingType
        {
            public:
                FolderGroupingType() : GroupingType("Folder", -1 /*AllIncFolders*/) {}

                Grouping* GetGroup(Grouping* subgrouping) const override;
                bool ForGrouping(const Grouping* grouping) const override;
        };

        void initializePotentialGroups();
        void setup();
        void addGroup(GroupingType* groupType);
        void removeGroup(DropDownButton* button);
        void removeAllGroups();
        void removeUnwantedGroups();
        QList<GroupingType*> getRemainingGroupTypes(DropDownButton* context) const;
        bool showFolderOption(DropDownButton* context) const;
        bool wantGroupingType(const GroupingType* gt) const;
        DropDownButton* newGroupButton(GroupingType* groupType);
        void buildContextMenu(DropDownButton* button, QMenu* menu);
        void buildAddGroupMenu(QMenu* menu);

        static const int MAX_GROUPS = 5;
        static const int INNER_GUTTER = 6;

        QList<GroupingType*> potentialGroups_;
        FolderGroupingType* folderGroupingType_;
        QList<DropDownButton*> groups_;
        DropDownButton* addGroupButton_;
        Searcher* searcher_;
        DropDownButton* lastButtonClicked_;

        // TODO: Implement drag-and-drop reordering of grouping buttons
        // TODO: Implement custom field grouping types
        // TODO: Implement grouping type hierarchy (ancestors/descendants)
};

#endif // GROUPINGCONTROL_H
