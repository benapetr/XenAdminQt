/* Copyright (c) 2025, Petr Bena <petr@bena.rocks>
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

#include "searchtabpage.h"
#include "../controls/xensearch/searcher.h"
#include "../controls/xensearch/searchoutput.h"
#include "../controls/xensearch/querypanel.h"
#include "../../xenlib/xensearch/search.h"
#include "../../xenlib/xensearch/sort.h"
#include "../../xenlib/xen/xenobject.h"
#include <QTimer>
#include <QVBoxLayout>

SearchTabPage::SearchTabPage(QWidget* parent) : BaseTabPage(parent), m_ignoreSearchUpdate(false)
{
    this->m_search = nullptr;
    this->m_searcher = new Searcher(this);
    this->m_output = new SearchOutput(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(this->m_searcher);
    layout->addWidget(this->m_output);

    // C# SearchPage wiring
    connect(this->m_searcher, &Searcher::SearchChanged, this, &SearchTabPage::onSearchChanged);
    connect(this->m_searcher, &Searcher::SaveRequested, this, &SearchTabPage::onSaveRequested);

    if (this->m_output->GetQueryPanel())
    {
        connect(this->m_output->GetQueryPanel(), &QueryPanel::SearchChanged, this, &SearchTabPage::onQueryPanelSearchChanged);
        connect(this->m_output->GetQueryPanel(), &QTreeWidget::itemDoubleClicked, this, &SearchTabPage::onItemDoubleClicked);
    }

    this->m_searcher->ToggleExpandedState(false);
}

SearchTabPage::~SearchTabPage()
{
    delete this->m_search;
    this->m_search = nullptr;
}

bool SearchTabPage::IsApplicableForObjectType(const QString& type) const
{
    Q_UNUSED(type);
    return true;
}

void SearchTabPage::SetXenObject(XenConnection* conn, const QString& type, const QString& ref, const QVariantMap& data)
{
    Q_UNUSED(data);

    this->m_connection = conn;
    this->m_objectType = type;
    this->m_objectRef = ref;

    if (!this->m_connection)
        return;

    QStringList refs;
    QStringList types;
    if (!ref.isEmpty())
    {
        refs.append(ref);
        types.append(type);
    }

    Search* search = Search::SearchFor(refs, types, this->m_connection);
    this->setSearch(search);
}

void SearchTabPage::setSearch(Search* search)
{
    if (this->m_search == search)
        return;

    const bool wasIgnoring = this->m_ignoreSearchUpdate;
    this->m_ignoreSearchUpdate = true;

    Search* oldSearch = this->m_search;
    this->m_search = search;
    if (oldSearch)
    {
        QTimer::singleShot(0, this, [oldSearch]() {
            delete oldSearch;
        });
    }

    if (this->m_searcher)
        this->m_searcher->SetSearch(this->m_search);

    if (this->m_output)
    {
        this->m_output->SetSearch(this->m_search);
    }

    this->buildList();

    this->m_ignoreSearchUpdate = wasIgnoring;
}

void SearchTabPage::buildList()
{
    if (this->m_output)
        this->m_output->BuildList();
}

void SearchTabPage::onSearchChanged()
{
    if (this->m_ignoreSearchUpdate)
        return;

    this->m_ignoreSearchUpdate = true;

    Search* search = this->m_searcher ? this->m_searcher->GetSearch() : nullptr;
    if (search)
    {
        this->setSearch(search);
    }

    this->m_ignoreSearchUpdate = false;
}

void SearchTabPage::onQueryPanelSearchChanged()
{
    if (!this->m_search || !this->m_output)
        return;

    QueryPanel* panel = this->m_output->GetQueryPanel();
    if (!panel)
        return;

    QList<Sort> sorting;
    const QList<QPair<QString, bool>> panelSorting = panel->GetSorting();
    for (const auto& sortPair : panelSorting)
    {
        sorting.append(Sort(sortPair.first, sortPair.second));
    }

    this->m_search->SetSorting(sorting);
    this->m_output->BuildList();
}

void SearchTabPage::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item)
        return;

    QVariant typeVar = item->data(0, Qt::UserRole + 1);
    QVariant objVar = item->data(0, Qt::UserRole);

    if (!typeVar.isValid() || !objVar.isValid())
        return;

    XenObject* xenObj = static_cast<XenObject*>(objVar.value<void*>());
    if (!xenObj)
        return;

    emit objectSelected(xenObj->GetObjectType(), xenObj->OpaqueRef());
}

void SearchTabPage::onSaveRequested()
{
    // TODO: Wire to save search dialog/action (matches C# Searcher_SaveRequested)
}

void SearchTabPage::OnPageShown()
{
    QueryPanel::PanelShown();
}

void SearchTabPage::OnPageHidden()
{
    QueryPanel::PanelHidden();
}
