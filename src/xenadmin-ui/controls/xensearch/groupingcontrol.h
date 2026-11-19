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

#ifndef GROUPINGCONTROL_H
#define GROUPINGCONTROL_H

#include <QWidget>
#include <QList>
#include <QPoint>
#include "../dropdownbutton.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/common.h"
#include "xenlib/customfields/customfielddefinition.h"

class QHBoxLayout;
class Searcher;

/**
 * "Group by" buttons of the search editor
 *
 * Qt port of C# XenAdmin.Controls.XenSearch.GroupingControl. Each button is one level of grouping
 * (left = outermost); a button's menu replaces or removes it, "Add Group" appends one. Only
 * groupings relevant to what is searched for are offered, related-object groupings keep their
 * parent/child order (pool before server before VM ...), and buttons can be dragged to reorder.
 */
class GroupingControl : public QWidget
{
    Q_OBJECT

    public:
        explicit GroupingControl(QWidget* parent = nullptr);
        ~GroupingControl() override;

        //! The grouping chain of the buttons (new objects owned by the caller), or null
        Grouping* GetGrouping() const;
        //! Show the buttons for a grouping chain (not taken over)
        void SetGrouping(Grouping* grouping);
        void SetSearcher(Searcher* searcher);

        class GroupingType
        {
            public:
                explicit GroupingType(XenSearch::ObjectTypes appliesTo) : appliesTo_(appliesTo) {}
                virtual ~GroupingType() = default;

                XenSearch::ObjectTypes GetAppliesTo() const { return this->appliesTo_; }
                virtual QString ToString() const = 0;
                virtual Grouping* GetGroup(Grouping* subgrouping) const = 0;
                virtual bool ForGrouping(const Grouping* grouping) const = 0;
                virtual bool IsDescendantOf(const GroupingType* gt) const { Q_UNUSED(gt); return false; }

            private:
                XenSearch::ObjectTypes appliesTo_;
        };

        class FolderGroupingType;
        class PropertyGroupingType;
        class BoolGroupingType;
        class XenModelObjectPropertyGroupingType;
        class CustomFieldGroupingType;
        class AllCustomFieldsGroupingType;

    signals:
        void GroupingChanged();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private slots:
        void onSearchForChanged();
        void onCustomFieldsChanged();

    private:
        void setup();
        void addGroup(const GroupingType* groupType);
        void removeGroup(DropDownButton* button);
        void removeAllGroups();
        void removeUnwantedGroups();
        void fromGrouping(const Grouping* grouping);
        QList<const GroupingType*> allGroupTypes() const;
        QList<const GroupingType*> getRemainingGroupTypes(DropDownButton* context) const;
        bool showFolderOption(DropDownButton* context) const;
        bool wantGroupingType(const GroupingType* gt) const;
        bool searchingForFolders() const;
        DropDownButton* newGroupButton(const GroupingType* groupType);
        void setButtonType(DropDownButton* button, const GroupingType* groupType);
        const GroupingType* buttonType(DropDownButton* button) const;
        void populateButtonMenu(DropDownButton* button);
        void populateAddGroupMenu();
        void dragTo(DropDownButton* button, int x);

        static const int MAX_GROUPS = 5;
        static const int INNER_GUTTER = 6;

        QHBoxLayout* layout_ = nullptr;
        QList<GroupingType*> potentialGroups_;
        GroupingType* folderGroupingType_ = nullptr;
        QList<GroupingType*> customFieldTypes_;
        QList<DropDownButton*> groups_;
        DropDownButton* addGroupButton_ = nullptr;
        Searcher* searcher_ = nullptr;

        // Drag to reorder (C# button_MouseDown/Move/Up)
        DropDownButton* draggedButton_ = nullptr;
        QPoint dragStart_;
        bool dragging_ = false;
};

#endif // GROUPINGCONTROL_H
