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

#include "searchoutput.h"
#include "querypanel.h"
#include "foldernavigator.h"
#include "../dropdownbutton.h"
#include "../../../xenlib/xensearch/search.h"
#include <QVBoxLayout>
#include <QMenu>

SearchOutput::SearchOutput(QWidget* parent)
    : QWidget(parent),
      layout_(nullptr),
      columnsButton_(nullptr),
      folderNavigator_(nullptr),
      queryPanel_(nullptr)
{
    // C# equivalent: SearchOutput() constructor (SearchOutput.cs line 38)
    // C# code:
    //   public SearchOutput()
    //   {
    //       InitializeComponent();
    //   }
    
    this->setupUi();
}

SearchOutput::~SearchOutput()
{
    // Qt manages child widget cleanup automatically
}

void SearchOutput::setupUi()
{
    // C# equivalent: InitializeComponent() in SearchOutput.Designer.cs
    // 
    // C# layout structure:
    // - TableLayoutPanel (3 rows)
    //   - Row 0: buttonColumns (DropDownButton with "Choose Columns" text)
    //   - Row 1: folderNavigator (optional breadcrumb)
    //   - Row 2: queryPanel (main grid)
    
    this->layout_ = new QVBoxLayout(this);
    this->layout_->setContentsMargins(0, 0, 0, 0);
    this->layout_->setSpacing(0);
    
    // Column chooser button at top
    // C# equivalent: buttonColumns (DropDownButton)
    this->columnsButton_ = new DropDownButton(tr("Choose Columns"), this);
    this->layout_->addWidget(this->columnsButton_);
    
    // Folder breadcrumb navigator (optional)
    this->folderNavigator_ = new FolderNavigator(this);
    this->folderNavigator_->setVisible(false);
    this->layout_->addWidget(this->folderNavigator_);
    
    // Main QueryPanel grid
    // C# equivalent: queryPanel (QueryPanel)
    this->queryPanel_ = new QueryPanel(this);
    this->layout_->addWidget(this->queryPanel_, 1); // Stretch factor 1
}

void SearchOutput::SetSearch(Search* search)
{
    // C# equivalent: Search property setter (SearchOutput.cs lines 60-73)
    // C# code:
    //   [Browsable(false)]
    //   public Search Search
    //   {
    //       set
    //       {
    //           string folder = (value == null ? null : value.FolderForNavigator);
    //           if (FolderNavigator != null)
    //               FolderNavigator.Folder = folder;
    //           if (QueryPanel != null)
    //               QueryPanel.Search = value;
    //       }
    //   }
    
    if (!this->queryPanel_)
        return;
    
    // Handle FolderForNavigator - shows breadcrumb navigation if path is set
    // TODO: Implement Search::GetFolderForNavigator() when folder support is added
    // QString folder = (search ? search->GetFolderForNavigator() : QString());
    // if (this->folderNavigator_)
    // {
    //     this->folderNavigator_->SetFolder(folder);
    //     this->folderNavigator_->setVisible(!folder.isEmpty());
    // }
    
    this->queryPanel_->SetSearch(search);
}

void SearchOutput::BuildList()
{
    // C# equivalent: BuildList() method (SearchOutput.cs lines 75-79)
    // C# code:
    //   public void BuildList()
    //   {
    //       if (QueryPanel != null)
    //           QueryPanel.BuildList();
    //   }
    
    if (this->queryPanel_)
    {
        this->queryPanel_->BuildList();
    }
}

void SearchOutput::onColumnsButtonClicked()
{
    // C# equivalent: contextMenuStripColumns_Opening event handler
    // (SearchOutput.cs lines 81-85)
    // C# code:
    //   private void contextMenuStripColumns_Opening(object sender, CancelEventArgs e)
    //   {
    //       contextMenuStripColumns.Items.Clear();
    //       contextMenuStripColumns.Items.AddRange(QueryPanel.GetChooseColumnsMenu().ToArray());
    //   }
    
    if (!this->queryPanel_)
        return;
    
    // Get column chooser actions from QueryPanel
    QList<QAction*> columnActions = this->queryPanel_->GetChooseColumnsMenu();
    if (columnActions.isEmpty())
        return;
    
    // Create menu and set it on the button
    QMenu* columnsMenu = new QMenu(this);
    for (QAction* action : columnActions)
    {
        columnsMenu->addAction(action);
    }
    
    // Button will show the menu when clicked
    this->columnsButton_->setMenu(columnsMenu);
}
