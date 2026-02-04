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

#ifndef CUSTOMTREENODE_H
#define CUSTOMTREENODE_H

#include <QIcon>
#include <QList>
#include <QString>
#include <QVariant>
#include <Qt>

class CustomTreeNode
{
    public:
        CustomTreeNode();
        explicit CustomTreeNode(bool selectable);
        virtual ~CustomTreeNode();

        bool Enabled = true;
        bool PreferredExpanded = true;
        bool HideCheckbox = false;
        bool CheckedIfDisabled = true;

        QString Text = QStringLiteral("new_node");
        QString Description = QStringLiteral("a_node");
        int Level = -1;
        int ChildNumber = -1;
        CustomTreeNode* ParentNode = nullptr;
        QIcon Image;
        QVariant Tag;

        QList<CustomTreeNode*> ChildNodes;

        bool Selectable() const { return m_selectable; }

        Qt::CheckState State() const { return m_state; }
        void SetState(Qt::CheckState value);

        bool Expanded() const { return m_expanded; }
        void SetExpanded(bool value);

        void AddChild(CustomTreeNode* child);
        bool IsDescendantOf(const CustomTreeNode* parent) const;

        virtual QString ToString() const;

        int CompareTo(const CustomTreeNode* other) const;

    protected:
        virtual int SameLevelSortOrder(const CustomTreeNode* other) const;

    private:
        bool m_selectable = true;
        bool m_expanded = true;
        Qt::CheckState m_state = Qt::Unchecked;
};

#endif // CUSTOMTREENODE_H
