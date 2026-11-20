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
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/sort.h"
#include "xenlib/xen/xenobject.h"
#include "../controls/dropdownbutton.h"
#include "../dialogs/savesearchdialog.h"
#include "../dialogs/warningdialogs/warningdialog.h"
#include "../mainwindow.h"
#include "xenlib/xensearch/searchmanager.h"
#include "xenlib/xensearch/searchmarshalling.h"
#include "xenlib/xen/actions/general/searchaction.h"
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

SearchTabPage::SearchTabPage(QWidget* parent) : BaseTabPage(parent), m_ignoreSearchUpdate(false)
{
    this->m_search = nullptr;
    this->m_searcher = new Searcher(this);
    this->m_output = new SearchOutput(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // C# SearchPage buttons
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->setContentsMargins(4, 4, 4, 4);
    this->m_newSearchButton = new QPushButton(tr("&New Search"), this);
    this->m_editSearchButton = new QPushButton(tr("&Edit Search"), this);
    this->m_savedSearchesButton = new DropDownButton(this);
    this->m_savedSearchesButton->setText(tr("&Saved Searches"));
    this->m_savedSearchesButton->SetMenu(new QMenu(this->m_savedSearchesButton));
    this->m_exportButton = new QPushButton(tr("E&xport..."), this);
    this->m_importButton = new QPushButton(tr("&Import..."), this);
    buttons->addWidget(this->m_newSearchButton);
    buttons->addWidget(this->m_editSearchButton);
    buttons->addWidget(this->m_savedSearchesButton);
    buttons->addStretch();
    buttons->addWidget(this->m_exportButton);
    buttons->addWidget(this->m_importButton);
    layout->addLayout(buttons);

    connect(this->m_newSearchButton, &QPushButton::clicked, this, &SearchTabPage::onNewSearch);
    connect(this->m_editSearchButton, &QPushButton::clicked, this, &SearchTabPage::onEditSearch);
    connect(this->m_savedSearchesButton->GetMenu(), &QMenu::aboutToShow, this, &SearchTabPage::populateSavedSearchesMenu);
    connect(this->m_exportButton, &QPushButton::clicked, this, &SearchTabPage::onExport);
    connect(this->m_importButton, &QPushButton::clicked, this, [this]() {
        if (MainWindow* mainWindow = MainWindow::instance())
            mainWindow->ImportSearch(this);
    });

    layout->addWidget(this->m_searcher);
    this->m_searcher->installEventFilter(this);
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
    this->updateEditButton();
}

SearchTabPage::~SearchTabPage()
{
    delete this->m_search;
    this->m_search = nullptr;
}

bool SearchTabPage::IsApplicableForObjectType(XenObjectType objectType) const
{
    Q_UNUSED(objectType);
    return true;
}

void SearchTabPage::SetObject(QSharedPointer<XenObject> object)
{
    BaseTabPage::SetObject(object);

    if (!this->m_connection)
        return;

    QStringList refs;
    QStringList types;
    if (!object.isNull() && !object->OpaqueRef().isEmpty())
    {
        refs.append(object->OpaqueRef());
        types.append(object->GetObjectTypeName());
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
        this->m_output->SetSearch(this->m_search);

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
        this->setSearch(search);

    this->m_ignoreSearchUpdate = false;

    if (search)
        emit this->searchChanged();
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

    if (!this->m_ignoreSearchUpdate)
        emit this->searchChanged();
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

    emit objectSelected(xenObj->GetObjectTypeName(), xenObj->OpaqueRef());
}

// C# Searcher_SaveRequested: name it (avoiding names in use, CA-34780), choose a pool, save
void SearchTabPage::onSaveRequested()
{
    std::unique_ptr<Search> edited(this->m_searcher ? this->m_searcher->GetSearch() : nullptr);
    if (!edited)
        return;

    SearchManager* manager = SearchManager::instance();
    const QList<SearchManager::Entry> existing = manager->GetSearches();
    auto nameTaken = [&](const QString& name) {
        return std::any_of(existing.begin(), existing.end(), [&](const SearchManager::Entry& e) { return e.name == name; });
    };

    QString name = this->m_search ? this->m_search->GetName() : QString();
    if (nameTaken(name))
    {
        for (int i = 2;; ++i)
        {
            const QString candidate = QString("%1 (%2)").arg(name).arg(i);
            if (!nameTaken(candidate))
            {
                name = candidate;
                break;
            }
        }
    }

    SaveSearchDialog dialog(name, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.GetConnection())
        return;

    // Saving a saved search again updates it (C# keeps the UUID); built-in ones get a copy
    QString uuid = this->m_search ? this->m_search->GetUUID() : QString();
    const bool savedSearch = std::any_of(existing.begin(), existing.end(), [&](const SearchManager::Entry& e) {
        return e.uuid == uuid && !e.isDefault;
    });
    if (!savedSearch)
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    Search toSave(edited->GetQuery(), edited->GetGrouping(), dialog.GetName(), uuid, false,
                  this->m_search ? this->m_search->GetColumns() : QList<QPair<QString, int>>(),
                  this->m_search ? this->m_search->GetSorting() : QList<Sort>());
    edited->ReleaseOwnership();

    SearchAction* action = new SearchAction(dialog.GetConnection(), uuid, dialog.GetName(),
                                            SearchMarshalling::SearchToXml(&toSave), SearchAction::Operation::Save);
    action->RunAsync(true);
}

void SearchTabPage::onNewSearch()
{
    // C# buttonNewSearch_Click
    this->m_searcher->ToggleExpandedState(true);
    this->m_searcher->BlankSearch();
}

void SearchTabPage::onEditSearch()
{
    this->m_searcher->ToggleExpandedState(true);
}

void SearchTabPage::updateEditButton()
{
    // C#: Edit Search is only offered while the editor is closed
    this->m_editSearchButton->setEnabled(!this->m_searcher->isVisibleTo(this));
}

bool SearchTabPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == this->m_searcher && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
        this->updateEditButton();
    return BaseTabPage::eventFilter(watched, event);
}

Search* SearchTabPage::defaultSearch() const
{
    QStringList refs;
    QStringList types;
    if (this->m_object && !this->m_object->OpaqueRef().isEmpty())
    {
        refs.append(this->m_object->OpaqueRef());
        types.append(this->m_object->GetObjectTypeName());
    }
    return Search::SearchFor(refs, types, this->m_connection);
}

// C# contextMenuStripSearches_Opening
void SearchTabPage::populateSavedSearchesMenu()
{
    QMenu* menu = this->m_savedSearchesButton->GetMenu();
    menu->clear();

    connect(menu->addAction(tr("Default Search")), &QAction::triggered, this, [this]() {
        this->setSearch(this->defaultSearch());
    });

    const QList<SearchManager::Entry> searches = SearchManager::instance()->GetSearches();
    auto addSearch = [this, menu](const SearchManager::Entry& entry) {
        QAction* action = menu->addAction(QIcon(entry.isDefault ? ":/icons/default_search_16.png" : ":/icons/search_16.png"),
                                          QString(entry.name).replace("&", "&&"));
        const QString uuid = entry.uuid;
        connect(action, &QAction::triggered, this, [this, uuid]() {
            if (Search* search = SearchManager::instance()->CreateSearch(uuid))
                this->setSearch(search);
        });
    };
    // Saved searches first, then the built-in ones
    for (const SearchManager::Entry& entry : searches)
        if (!entry.isDefault)
            addSearch(entry);
    for (const SearchManager::Entry& entry : searches)
        if (entry.isDefault)
            addSearch(entry);

    menu->addSeparator();
    QMenu* deleteMenu = menu->addMenu(tr("&Delete"));
    bool anySaved = false;
    for (const SearchManager::Entry& entry : searches)
    {
        if (entry.isDefault)
            continue;
        anySaved = true;
        const QString uuid = entry.uuid;
        const QString name = entry.name;
        connect(deleteMenu->addAction(QIcon(":/icons/search_16.png"), QString(name).replace("&", "&&")),
                &QAction::triggered, this, [this, uuid, name]() { this->deleteSavedSearch(uuid, name); });
    }
    deleteMenu->setEnabled(anySaved);
}

void SearchTabPage::deleteSavedSearch(const QString& uuid, const QString& name)
{
    if (WarningDialog::ShowYesNo(tr("Are you sure you want to delete saved search '%1'?").arg(name),
                                 tr("Delete Search '%1'").arg(name), this) != WarningDialog::Result::Yes)
        return;

    // C# Search.Delete: remove it from every pool that holds it
    for (XenConnection* connection : SearchManager::instance()->ConnectionsHolding(uuid))
        (new SearchAction(connection, uuid, name, QString(), SearchAction::Operation::Delete))->RunAsync(true);
}

// C# buttonExport_Click / Search.Save(filename)
void SearchTabPage::onExport()
{
    if (!this->m_search)
        return;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Search"), QString(),
                                                    tr("Saved Searches (*.xensearch)"));
    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".xensearch", Qt::CaseInsensitive))
        fileName += ".xensearch";

    // An exported search needs a UUID
    QString uuid = this->m_search->GetUUID();
    if (uuid.isEmpty())
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<Search> copy(SearchMarshalling::LoadSearch(SearchMarshalling::SearchToXml(this->m_search)));
    if (!copy)
        return;
    Search exported(copy->GetQuery(), copy->GetGrouping(), copy->GetName(), uuid, false, copy->GetColumns(), copy->GetSorting());
    copy->ReleaseOwnership();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Export Search"), tr("Unable to write '%1': %2").arg(fileName, file.errorString()));
        return;
    }
    file.write(SearchMarshalling::SearchesToXml({&exported}).toUtf8());
}

void SearchTabPage::OnPageShown()
{
    QueryPanel::PanelShown();
}

void SearchTabPage::OnPageHidden()
{
    QueryPanel::PanelHidden();
}
