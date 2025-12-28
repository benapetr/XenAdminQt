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

// searchoutput.h - Container widget for search results display
// C# equivalent: xenadmin/XenAdmin/Controls/XenSearch/SearchOutput.cs

#ifndef SEARCHOUTPUT_H
#define SEARCHOUTPUT_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMenu>

class QueryPanel;
class Search;
class FolderNavigator;
class DropDownButton;

/*!
 * \brief Container widget for search results display
 *
 * C# equivalent: XenAdmin.Controls.XenSearch.SearchOutput
 * (xenadmin/XenAdmin/Controls/XenSearch/SearchOutput.cs)
 *
 * SearchOutput is a composite widget that contains:
 * - Column chooser button (top)
 * - FolderNavigator breadcrumb (optional, when Search.FolderForNavigator is set)
 * - QueryPanel grid (main content)
 *
 * This is the primary widget used by SearchTabPage to display search results.
 *
 * Usage example:
 * \code
 * SearchOutput* output = new SearchOutput(this);
 * Search* search = new Search(query, grouping, "My Search", "", false);
 * output->SetSearch(search);
 * output->BuildList();
 * \endcode
 */
class SearchOutput : public QWidget
{
    Q_OBJECT

    public:
        /*!
         * \brief Constructor
         * \param parent Parent widget
         *
         * C# equivalent: SearchOutput() constructor (SearchOutput.cs line 38)
         */
        explicit SearchOutput(QWidget* parent = nullptr);

        /*!
         * \brief Destructor
         */
        ~SearchOutput() override;

        /*!
         * \brief Get the QueryPanel instance
         * \return Pointer to the QueryPanel child widget
         *
         * C# equivalent: QueryPanel property (SearchOutput.cs lines 52-58)
         */
        QueryPanel* GetQueryPanel() const { return this->queryPanel_; }

        /*!
         * \brief Set the search to display
         * \param search Search instance (ownership not transferred)
         *
         * C# equivalent: Search property setter (SearchOutput.cs lines 60-73)
         *
         * Sets the search on the QueryPanel. If the search has a FolderForNavigator,
         * the FolderNavigator breadcrumb is shown (not yet implemented).
         */
        void SetSearch(Search* search);

        /*!
         * \brief Rebuild the search results list
         *
         * C# equivalent: BuildList() method (SearchOutput.cs lines 75-79)
         *
         * Triggers QueryPanel to rebuild its content based on current search.
         */
        void BuildList();

    private slots:
        /*!
         * \brief Handle column chooser button click
         *
         * C# equivalent: contextMenuStripColumns_Opening event handler
         * (SearchOutput.cs lines 81-85)
         *
         * Shows a context menu with column visibility toggles.
         */
        void onColumnsButtonClicked();

    private:
        /*!
         * \brief Initialize the UI layout
         */
        void setupUi();

    private:
        QVBoxLayout* layout_;           ///< Main vertical layout
        DropDownButton* columnsButton_;    ///< "Choose Columns" button
        FolderNavigator* folderNavigator_;   ///< Breadcrumb navigation (optional)
        QueryPanel* queryPanel_;        ///< Search results grid
};

#endif // SEARCHOUTPUT_H
