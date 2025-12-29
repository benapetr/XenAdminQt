/* Copyright (c) 2025 Petr Bena
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
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

#ifndef SEARCHTABPAGE_H
#define SEARCHTABPAGE_H

#include "basetabpage.h"

class Search;
class Searcher;
class SearchOutput;
class QTreeWidgetItem;

/**
 * @brief SearchTabPage - Search panel tab using Searcher + SearchOutput
 *
 * C# equivalent: XenAdmin.TabPages.SearchPage
 * (hosts Searcher + SearchOutput.QueryPanel)
 */
class SearchTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit SearchTabPage(QWidget* parent = nullptr);
        ~SearchTabPage() override;

        bool IsApplicableForObjectType(const QString& type) const override;
        void SetXenObject(XenConnection* conn, const QString& type, const QString& ref, const QVariantMap& data) override;
        QString GetTitle() const override
        {
            return tr("Search");
        }

        void setSearch(Search* search);
        Search* getSearch() const { return m_search; }
        void buildList();

        void OnPageShown() override;
        void OnPageHidden() override;

    signals:
        void objectSelected(const QString& objectType, const QString& objectRef);

    private slots:
        void onSearchChanged();
        void onQueryPanelSearchChanged();
        void onItemDoubleClicked(QTreeWidgetItem* item, int column);
        void onSaveRequested();

    private:
        Search* m_search;
        Searcher* m_searcher;
        SearchOutput* m_output;
        bool m_ignoreSearchUpdate;
};

#endif // SEARCHTABPAGE_H
