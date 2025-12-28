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

#ifndef RESOURCESELECTBUTTON_H
#define RESOURCESELECTBUTTON_H

#include "../dropdownbutton.h"
#include <QSharedPointer>

QT_FORWARD_DECLARE_CLASS(Search)
QT_FORWARD_DECLARE_CLASS(QueryScope)
class XenObject;

/**
 * @brief ResourceSelectButton - Dropdown with XenObject selection for UuidQueryType
 *
 * Matches C# XenAdmin/Controls/XenSearch/ResourceSelectButton.cs
 *
 * Populates dropdown from Search results showing XenObjects with:
 * - Hierarchical indentation (Pool→Host→VM)
 * - Icons for each object type
 * - Disabled items for objects outside query scope
 * - Folder path display for folder objects
 *
 * Usage:
 * ```cpp
 * ResourceSelectButton* selector = new ResourceSelectButton();
 * selector->Populate(search);
 * connect(selector, &ResourceSelectButton::itemSelected, [](const QString& ref) {
 *     qDebug() << "Selected ref:" << ref;
 * });
 * ```
 *
 * Integration with UuidQueryType:
 * - UuidQueryType creates this widget instead of StringTextBox
 * - Widget populates from Search::PopulateAdapters() (hierarchical tree)
 * - User selects object, widget emits itemSelected(ref)
 * - QueryType uses ref for XenModelObjectPropertyQuery matching
 */
class ResourceSelectButton : public DropDownButton
{
    Q_OBJECT

    public:
        explicit ResourceSelectButton(QWidget* parent = nullptr);

        /**
         * @brief Populate dropdown from Search results
         * @param search Search instance providing PopulateAdapters() for tree building
         *
         * Calls search->PopulateAdapters(this) which adds grouped/indented items
         * via Add(grouping, group, indent) method
         */
        void Populate(Search* search);

        /**
         * @brief Get currently selected object reference
         * @return Opaque reference of selected object, or empty string
         */
        QString selectedRef() const;

        /**
         * @brief Set selected object by reference
         * @param ref Opaque reference to select
         */
        void setSelectedRef(const QString& ref);

        // IAcceptGroups implementation (matches C# interface)
        void AddGroup(const QString& grouping, QSharedPointer<XenObject> object, int indent);
        void FinishedInThisGroup(bool defaultExpand);

    signals:
        /**
         * @brief Emitted when user selects an object
         * @param ref Opaque reference of selected object
         */
        void itemSelected(const QString& ref);

    private slots:
        void onActionTriggered();

    private:
        QueryScope* scope_;
        QString selectedRef_;
        static const QString INDENT;

        QIcon getObjectIcon(QSharedPointer<XenObject> object) const;
};

#endif // RESOURCESELECTBUTTON_H
