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

#ifndef VIRTUALTREENODE_H
#define VIRTUALTREENODE_H

#include <QString>
#include <QList>
#include <QColor>
#include <QObject>

class VirtualTreeNode;

/**
 * @brief Interface for objects that have tree nodes
 * 
 * Qt equivalent of C# IHaveNodes interface
 * Used for tree node expansion/collapse and node collection access
 * 
 * C# equivalent: xenadmin/XenAdmin/Controls/TreeViews/VirtualTreeView.cs (IHaveNodes)
 */
class IHaveNodes
{
    public:
        virtual ~IHaveNodes() = default;
        
        virtual QList<VirtualTreeNode*>& Nodes() = 0;
        virtual QObject* Tag() const = 0;
        virtual void Expand() = 0;
        virtual void Collapse() = 0;
};

/**
 * @brief Virtual tree node for tree view
 * 
 * Qt equivalent of C# VirtualTreeNode class.
 * Represents a node in the tree view with text, icon, colors, and child nodes.
 * 
 * C# equivalent: xenadmin/XenAdmin/Controls/TreeViews/VirtualTreeView.cs (VirtualTreeNode)
 */
class VirtualTreeNode : public IHaveNodes
{
    public:
        explicit VirtualTreeNode(const QString& text);
        ~VirtualTreeNode();
        
        // Text and display properties
        QString Text() const;
        void SetText(const QString& text);
        
        QObject* Tag() const override;
        void SetTag(QObject* tag);
        
        int ImageIndex() const;
        void SetImageIndex(int index);
        
        QColor BackColor() const;
        void SetBackColor(const QColor& color);
        
        QColor ForeColor() const;
        void SetForeColor(const QColor& color);
        
        // Expansion state
        bool IsExpanded() const;
        void SetExpanded(bool expanded);
        void Expand() override;
        void Collapse() override;
        
        // Tree hierarchy
        VirtualTreeNode* Parent() const;
        void SetParent(VirtualTreeNode* parent);
        
        QList<VirtualTreeNode*>& Nodes() override;
        const QList<VirtualTreeNode*>& Nodes() const;
        
        void Insert(int index, VirtualTreeNode* node);
        
    private:
        QString text_;
        QObject* tag_;
        int imageIndex_;
        QColor backColor_;
        QColor foreColor_;
        bool expanded_;
        VirtualTreeNode* parent_;
        QList<VirtualTreeNode*> children_;
};

#endif // VIRTUALTREENODE_H
