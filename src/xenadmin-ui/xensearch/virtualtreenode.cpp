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

#include "virtualtreenode.h"

VirtualTreeNode::VirtualTreeNode(const QString& text)
    : text_(text),
      tag_(nullptr),
      imageIndex_(0),
      expanded_(false),
      parent_(nullptr)
{
}

VirtualTreeNode::~VirtualTreeNode()
{
    // Clean up children
    qDeleteAll(this->children_);
    this->children_.clear();
}

QString VirtualTreeNode::Text() const
{
    return this->text_;
}

void VirtualTreeNode::SetText(const QString& text)
{
    this->text_ = text;
}

QObject* VirtualTreeNode::Tag() const
{
    return this->tag_;
}

void VirtualTreeNode::SetTag(QObject* tag)
{
    this->tag_ = tag;
}

int VirtualTreeNode::ImageIndex() const
{
    return this->imageIndex_;
}

void VirtualTreeNode::SetImageIndex(int index)
{
    this->imageIndex_ = index;
}

QColor VirtualTreeNode::BackColor() const
{
    return this->backColor_;
}

void VirtualTreeNode::SetBackColor(const QColor& color)
{
    this->backColor_ = color;
}

QColor VirtualTreeNode::ForeColor() const
{
    return this->foreColor_;
}

void VirtualTreeNode::SetForeColor(const QColor& color)
{
    this->foreColor_ = color;
}

bool VirtualTreeNode::IsExpanded() const
{
    return this->expanded_;
}

void VirtualTreeNode::SetExpanded(bool expanded)
{
    this->expanded_ = expanded;
}

void VirtualTreeNode::Expand()
{
    this->expanded_ = true;
}

void VirtualTreeNode::Collapse()
{
    this->expanded_ = false;
}

VirtualTreeNode* VirtualTreeNode::Parent() const
{
    return this->parent_;
}

void VirtualTreeNode::SetParent(VirtualTreeNode* parent)
{
    this->parent_ = parent;
}

QList<VirtualTreeNode*>& VirtualTreeNode::Nodes()
{
    return this->children_;
}

const QList<VirtualTreeNode*>& VirtualTreeNode::Nodes() const
{
    return this->children_;
}

void VirtualTreeNode::Insert(int index, VirtualTreeNode* node)
{
    if (index < 0 || index > this->children_.size())
    {
        this->children_.append(node);
    }
    else
    {
        this->children_.insert(index, node);
    }
    
    if (node)
    {
        node->SetParent(this);
    }
}
