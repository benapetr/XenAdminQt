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

#include "folderchangedialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"

FolderChangeDialog::FolderChangeDialog(XenConnection* connection, const QString& originalFolderPath, QWidget* parent) : QDialog(parent),
      m_connection(connection), m_originalFolderPath(originalFolderPath.trimmed()), m_selectedFolderPath(originalFolderPath.trimmed())
{
    this->setWindowTitle(tr("Change Folder"));
    this->resize(520, 420);

    QVBoxLayout* root = new QVBoxLayout(this);

    this->m_noneRadio = new QRadioButton(tr("Not in a folder"), this);
    this->m_chooseRadio = new QRadioButton(tr("Place in selected folder"), this);
    root->addWidget(this->m_noneRadio);
    root->addWidget(this->m_chooseRadio);

    this->m_tree = new QTreeWidget(this);
    this->m_tree->setHeaderHidden(true);
    this->m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(this->m_tree, 1);

    QHBoxLayout* folderActions = new QHBoxLayout();
    this->m_newButton = new QPushButton(tr("New"), this);
    this->m_renameButton = new QPushButton(tr("Rename"), this);
    this->m_deleteButton = new QPushButton(tr("Delete"), this);
    folderActions->addWidget(this->m_newButton);
    folderActions->addWidget(this->m_renameButton);
    folderActions->addWidget(this->m_deleteButton);
    folderActions->addStretch();
    root->addLayout(folderActions);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    this->m_okButton = buttons->button(QDialogButtonBox::Ok);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this->m_noneRadio, &QRadioButton::toggled, this, &FolderChangeDialog::onRadioChanged);
    connect(this->m_chooseRadio, &QRadioButton::toggled, this, &FolderChangeDialog::onRadioChanged);
    connect(this->m_tree, &QTreeWidget::itemSelectionChanged, this, &FolderChangeDialog::onSelectionChanged);
    connect(this->m_newButton, &QPushButton::clicked, this, &FolderChangeDialog::onNewFolder);
    connect(this->m_renameButton, &QPushButton::clicked, this, &FolderChangeDialog::onRenameFolder);
    connect(this->m_deleteButton, &QPushButton::clicked, this, &FolderChangeDialog::onDeleteFolder);

    this->populateTree();

    if (this->m_originalFolderPath.isEmpty())
        this->m_noneRadio->setChecked(true);
    else
        this->m_chooseRadio->setChecked(true);
    this->selectPath(this->m_originalFolderPath);
    this->onRadioChanged();
}

QString FolderChangeDialog::SelectedFolderPath() const
{
    return this->m_noneRadio->isChecked() ? QString() : this->m_selectedFolderPath;
}

bool FolderChangeDialog::FolderChanged() const
{
    return this->SelectedFolderPath() != this->m_originalFolderPath;
}

void FolderChangeDialog::onRadioChanged()
{
    const bool choose = this->m_chooseRadio->isChecked();
    this->m_tree->setEnabled(choose);

    if (!choose)
        this->m_selectedFolderPath.clear();

    const bool valid = this->m_noneRadio->isChecked() || (choose && this->m_tree->currentItem() != nullptr);
    this->m_okButton->setEnabled(valid);
    this->updateActionButtons();
}

void FolderChangeDialog::onSelectionChanged()
{
    QTreeWidgetItem* item = this->m_tree->currentItem();
    if (!item)
    {
        this->onRadioChanged();
        return;
    }

    const QString path = item->data(0, Qt::UserRole).toString().trimmed();
    this->m_selectedFolderPath = path;
    if (!this->m_chooseRadio->isChecked())
        this->m_chooseRadio->setChecked(true);
    this->onRadioChanged();
}

void FolderChangeDialog::onNewFolder()
{
    if (!this->m_connection)
        return;

    const QString parentPath = (this->m_chooseRadio->isChecked() && this->m_tree->currentItem())
        ? this->m_tree->currentItem()->data(0, Qt::UserRole).toString().trimmed()
        : FoldersManager::PATH_SEPARATOR;

    bool ok = false;
    QString name = QInputDialog::getText(this,
                                         tr("New Folder"),
                                         tr("Folder name:"),
                                         QLineEdit::Normal,
                                         QString(),
                                         &ok).trimmed();
    if (!ok)
        return;
    name = FoldersManager::FixupRelativePath(name);
    if (name.isEmpty() || name.contains(';') || name.contains('/'))
        return;

    const QString newPath = FoldersManager::AppendPath(parentPath.isEmpty() ? FoldersManager::PATH_SEPARATOR : parentPath, name);
    if (!FoldersManager::instance()->CreateFolder(this->m_connection, newPath))
        return;

    this->populateTree();
    this->m_chooseRadio->setChecked(true);
    this->selectPath(newPath);
}

void FolderChangeDialog::onRenameFolder()
{
    QTreeWidgetItem* item = this->m_tree->currentItem();
    if (!item || !this->m_connection || !this->m_connection->GetCache())
        return;

    const QString oldPath = item->data(0, Qt::UserRole).toString().trimmed();
    if (oldPath.isEmpty() || oldPath == FoldersManager::PATH_SEPARATOR)
        return;

    const QString oldName = item->text(0);
    bool ok = false;
    QString newName = QInputDialog::getText(this,
                                            tr("Rename Folder"),
                                            tr("New folder name:"),
                                            QLineEdit::Normal,
                                            oldName,
                                            &ok).trimmed();
    if (!ok)
        return;
    newName = FoldersManager::FixupRelativePath(newName);
    if (newName.isEmpty() || newName == oldName || newName.contains(';') || newName.contains('/'))
        return;

    const QString parentPath = FoldersManager::GetParent(oldPath);
    const QString newPath = FoldersManager::AppendPath(parentPath.isEmpty() ? FoldersManager::PATH_SEPARATOR : parentPath, newName);

    const QList<QPair<XenObjectType, QString>> searchable = this->m_connection->GetCache()->GetXenSearchableObjects();
    for (const auto& pair : searchable)
    {
        if (pair.first == XenObjectType::Folder)
            continue;

        QSharedPointer<XenObject> obj = this->m_connection->GetCache()->ResolveObject(pair.first, pair.second);
        if (!obj)
            continue;

        const QString currentPath = obj->GetFolderPath();
        QString updatedPath = currentPath;
        if (currentPath == oldPath)
            updatedPath = newPath;
        else if (currentPath.startsWith(oldPath + FoldersManager::PATH_SEPARATOR))
            updatedPath = newPath + currentPath.mid(oldPath.length());

        if (updatedPath == currentPath)
            continue;

        if (updatedPath.isEmpty())
            FoldersManager::instance()->UnfolderObject(this->m_connection, obj->GetObjectTypeName(), obj->OpaqueRef());
        else
            FoldersManager::instance()->MoveObjectToFolder(this->m_connection, obj->GetObjectTypeName(), obj->OpaqueRef(), updatedPath);
    }

    QStringList descendants = FoldersManager::instance()->Descendants(this->m_connection, oldPath);
    std::sort(descendants.begin(), descendants.end(), [](const QString& a, const QString& b) { return a.size() < b.size(); });

    FoldersManager::instance()->CreateFolder(this->m_connection, newPath);
    for (const QString& oldDescendant : descendants)
    {
        QString newDescendant = oldDescendant;
        if (oldDescendant == oldPath)
            newDescendant = newPath;
        else if (oldDescendant.startsWith(oldPath + FoldersManager::PATH_SEPARATOR))
            newDescendant = newPath + oldDescendant.mid(oldPath.length());
        FoldersManager::instance()->CreateFolder(this->m_connection, newDescendant);
    }
    FoldersManager::instance()->DeleteFolder(this->m_connection, oldPath);

    this->populateTree();
    this->selectPath(newPath);
}

void FolderChangeDialog::onDeleteFolder()
{
    QTreeWidgetItem* item = this->m_tree->currentItem();
    if (!item || !this->m_connection || !this->m_connection->GetCache())
        return;

    const QString path = item->data(0, Qt::UserRole).toString().trimmed();
    if (path.isEmpty() || path == FoldersManager::PATH_SEPARATOR)
        return;

    if (QMessageBox::question(this,
                              tr("Delete Folder"),
                              tr("Delete folder '%1' and remove all folder assignments below it?").arg(path),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    const QList<QPair<XenObjectType, QString>> searchable = this->m_connection->GetCache()->GetXenSearchableObjects();
    for (const auto& pair : searchable)
    {
        if (pair.first == XenObjectType::Folder)
            continue;

        QSharedPointer<XenObject> obj = this->m_connection->GetCache()->ResolveObject(pair.first, pair.second);
        if (!obj)
            continue;

        const QString objectPath = obj->GetFolderPath();
        if (objectPath == path || objectPath.startsWith(path + FoldersManager::PATH_SEPARATOR))
            FoldersManager::instance()->UnfolderObject(this->m_connection, obj->GetObjectTypeName(), obj->OpaqueRef());
    }

    const QString parentPath = FoldersManager::GetParent(path);
    FoldersManager::instance()->DeleteFolder(this->m_connection, path);
    this->populateTree();
    this->selectPath(parentPath);
}

void FolderChangeDialog::populateTree()
{
    this->m_tree->clear();

    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    QStringList folders = this->m_connection->GetCache()->GetAllRefs(XenObjectType::Folder);
    folders.removeAll(QString());
    folders.removeAll(FoldersManager::PATH_SEPARATOR);
    folders.removeDuplicates();
    folders.sort();

    for (const QString& folderPath : folders)
        this->ensurePathNode(folderPath);
}

void FolderChangeDialog::selectPath(const QString& path)
{
    if (path.isEmpty())
    {
        this->m_tree->clearSelection();
        return;
    }

    const QList<QTreeWidgetItem*> allItems = this->m_tree->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive, 0);
    for (QTreeWidgetItem* item : allItems)
    {
        if (!item)
            continue;
        if (item->data(0, Qt::UserRole).toString() != path)
            continue;
        this->m_tree->setCurrentItem(item);
        item->setSelected(true);
        item->setExpanded(true);
        return;
    }
}

QTreeWidgetItem* FolderChangeDialog::ensurePathNode(const QString& path)
{
    if (path.isEmpty() || path == FoldersManager::PATH_SEPARATOR)
        return nullptr;

    QTreeWidgetItem* parent = nullptr;
    QStringList parts = FoldersManager::PointToPath(path);
    QString partial = FoldersManager::PATH_SEPARATOR;

    for (const QString& part : parts)
    {
        partial = FoldersManager::AppendPath(partial, part);

        QTreeWidgetItem* found = nullptr;
        const int count = parent ? parent->childCount() : this->m_tree->topLevelItemCount();
        for (int i = 0; i < count; ++i)
        {
            QTreeWidgetItem* candidate = parent ? parent->child(i) : this->m_tree->topLevelItem(i);
            if (!candidate)
                continue;
            if (candidate->data(0, Qt::UserRole).toString() == partial)
            {
                found = candidate;
                break;
            }
        }

        if (!found)
        {
            found = new QTreeWidgetItem();
            found->setText(0, part);
            found->setData(0, Qt::UserRole, partial);
            if (parent)
                parent->addChild(found);
            else
                this->m_tree->addTopLevelItem(found);
        }

        parent = found;
    }

    return parent;
}

void FolderChangeDialog::updateActionButtons()
{
    const bool canMutate = this->m_tree->currentItem() != nullptr;
    this->m_renameButton->setEnabled(canMutate);
    this->m_deleteButton->setEnabled(canMutate);
}
