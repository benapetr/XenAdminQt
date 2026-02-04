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

#include "customtreenode.h"
#include "xenlib/utils/misc.h"

CustomTreeNode::CustomTreeNode() = default;

CustomTreeNode::CustomTreeNode(bool selectable) : m_selectable(selectable)
{
}

CustomTreeNode::~CustomTreeNode() = default;

void CustomTreeNode::SetState(Qt::CheckState value)
{
    if (value == this->m_state && this->Level != -1)
        return;

    this->m_state = value;

    if (value != Qt::PartiallyChecked)
    {
        for (CustomTreeNode* child : this->ChildNodes)
        {
            if (!child || !child->Enabled)
                continue;
            if (child->State() != value)
                child->SetState(value);
        }
    }

    if (this->Level == -1 || !this->ParentNode)
        return;

    Qt::CheckState initial = value;
    if (initial != Qt::PartiallyChecked)
    {
        for (CustomTreeNode* sibling : this->ParentNode->ChildNodes)
        {
            if (!sibling || !sibling->Enabled)
                continue;
            if (sibling->State() != initial)
            {
                initial = Qt::PartiallyChecked;
                break;
            }
        }
    }

    this->ParentNode->SetState(initial);
}

void CustomTreeNode::SetExpanded(bool value)
{
    this->m_expanded = value;
    for (CustomTreeNode* child : this->ChildNodes)
    {
        if (!child)
            continue;
        if (!value)
            child->SetExpanded(value);
        else
            child->SetExpanded(child->PreferredExpanded);
    }
}

void CustomTreeNode::AddChild(CustomTreeNode* child)
{
    if (!child)
        return;

    child->Level = this->Level + 1;
    child->ParentNode = this;
    child->ChildNumber = this->ChildNodes.size();
    this->ChildNodes.append(child);
}

bool CustomTreeNode::IsDescendantOf(const CustomTreeNode* parent) const
{
    if (!parent || this->Level <= parent->Level || !this->ParentNode)
        return false;

    if (this->ParentNode == parent)
        return true;

    return this->ParentNode->IsDescendantOf(parent);
}

QString CustomTreeNode::ToString() const
{
    return this->Text;
}

int CustomTreeNode::CompareTo(const CustomTreeNode* other) const
{
    if (this == other)
        return 0;
    if (!other)
        return 1;

    if (other->IsDescendantOf(this))
        return -1;
    if (this->IsDescendantOf(other))
        return 1;
    if (this->ParentNode == other->ParentNode && this->SameLevelSortOrder(other) == 0)
        return this->ChildNumber - other->ChildNumber;
    if (this->ParentNode == other->ParentNode)
        return this->SameLevelSortOrder(other);
    if (this->Level < other->Level)
        return this->CompareTo(other->ParentNode);
    if (this->Level > other->Level)
        return this->ParentNode ? this->ParentNode->CompareTo(other) : 0;
    if (this->ParentNode)
        return this->ParentNode->CompareTo(other->ParentNode);

    return 0;
}

int CustomTreeNode::SameLevelSortOrder(const CustomTreeNode* other) const
{
    if (!other)
        return 1;
    return Misc::NaturalCompare(this->ToString(), other->ToString());
}
