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
#include "xenlib/xensearch/iacceptgroups.h"
#include "xenlib/xensearch/common.h"

class Search;
class XenConnection;

/**
 * @brief Drop-down that picks one object from the results of a search
 *
 * Qt port of C# XenAdmin.Controls.XenSearch.ResourceSelectButton. Search::PopulateAdapters
 * adds the objects grouped and indented (for example pool, server, VM); group headings that are
 * not objects are left out, and objects outside the search scope are listed but disabled.
 */
class ResourceSelectButton : public DropDownButton, public IAcceptGroups
{
    Q_OBJECT

    public:
        explicit ResourceSelectButton(QWidget* parent = nullptr);

        //! List the objects of the search, keeping the current selection if still listed
        void Populate(Search* search);

        //! Opaque ref and UUID of the selected object (empty if none)
        QString selectedRef() const;
        QString selectedUuid() const;

        void setSelectedRef(const QString& ref);
        //! Select the listed object with this UUID (C# FromQuery matches by UUID)
        void setSelectedUuid(const QString& uuid);

        // IAcceptGroups implementation (matches C# interface)
        IAcceptGroups* Add(Grouping* grouping, const QVariant& group,
                          const QString& objectType, const QVariantMap& objectData,
                          int indent, XenConnection* conn) override;
        void FinishedInThisGroup(bool defaultExpand) override;

    signals:
        void itemSelected(const QString& ref);

    private slots:
        void onActionTriggered(QAction* action);

    private:
        QAction* findAction(const QString& key, const QString& value) const;
        void select(QAction* action);

        bool hasScope_ = false;
        XenSearch::ObjectTypes scopeTypes_ = XenSearch::ObjectTypes::None;
        QString selectedRef_;
        QString selectedUuid_;
        static const QString INDENT;
};

#endif // RESOURCESELECTBUTTON_H
