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

// generaleditpage.cpp - General properties edit page
#include "generaleditpage.h"
#include "ui_generaleditpage.h"
#include "../dialogs/folderchangedialog.h"
#include "../dialogs/newtagdialog.h"
#include "xenlib/xen/actions/general/generaleditpageaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/folders/foldersmanager.h"
#include "xenlib/xencache.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>

GeneralEditPage::GeneralEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::GeneralEditPage)
{
    this->ui->setupUi(this);

    // Connect change signals for change tracking
    this->connect(this->ui->txtName, &QLineEdit::textChanged, this, &GeneralEditPage::onNameChanged);
    this->connect(this->ui->txtDescription, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onDescriptionChanged);
    this->connect(this->ui->cmbFolder, qOverload<int>(&QComboBox::currentIndexChanged), this, &GeneralEditPage::onFolderChanged);
    this->connect(this->ui->txtTags, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onTagsChanged);
    this->connect(this->ui->txtIQN, &QLineEdit::textChanged, this, &GeneralEditPage::onIQNChanged);

    this->ui->txtTags->setReadOnly(true);

    m_changeFolderButton = new QPushButton(tr("Change..."), this);
    m_editTagsButton = new QPushButton(tr("Edit..."), this);

    QWidget* folderEditorContainer = new QWidget(this);
    QHBoxLayout* folderEditorLayout = new QHBoxLayout(folderEditorContainer);
    folderEditorLayout->setContentsMargins(0, 0, 0, 0);
    folderEditorLayout->setSpacing(6);
    folderEditorLayout->addWidget(this->ui->cmbFolder, 1);
    folderEditorLayout->addWidget(m_changeFolderButton, 0);
    this->ui->formLayout->setWidget(2, QFormLayout::FieldRole, folderEditorContainer);

    this->ui->formLayout->insertRow(4, QString(), m_editTagsButton);
    connect(m_changeFolderButton, &QPushButton::clicked, this, &GeneralEditPage::onChangeFolderClicked);
    connect(m_editTagsButton, &QPushButton::clicked, this, &GeneralEditPage::onEditTagsClicked);

    // Hide IQN fields by default (only shown for hosts)
    this->ui->lblIQN->setVisible(false);
    this->ui->txtIQN->setVisible(false);
    this->ui->lblIQNHint->setVisible(false);
}

GeneralEditPage::~GeneralEditPage()
{
    delete this->ui;
}

QString GeneralEditPage::GetText() const
{
    return tr("General");
}

QString GeneralEditPage::GetSubText() const
{
    return tr("Name, Description, Tags");
}

QIcon GeneralEditPage::GetImage() const
{
    // Matches C# Images.StaticImages.edit_16
    return QIcon(":/icons/edit_16.png");
}

void GeneralEditPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    this->m_object = object;
    this->m_objectRef = object.isNull() ? QString() : object->OpaqueRef();
    this->m_objectType = object.isNull() ? XenObjectType::Null : object->GetObjectType();
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    this->repopulate();

    emit populated();
}

void GeneralEditPage::repopulate()
{
    // Block signals while populating to avoid triggering change tracking
    this->ui->txtName->blockSignals(true);
    this->ui->txtDescription->blockSignals(true);
    this->ui->cmbFolder->blockSignals(true);
    this->ui->txtTags->blockSignals(true);
    this->ui->txtIQN->blockSignals(true);

    // Populate name
    this->m_originalName = this->m_object->GetName();
    this->ui->txtName->setText(this->m_originalName);

    // Populate description
    this->m_originalDescription = this->m_object->GetDescription();
    this->ui->txtDescription->setPlainText(this->m_originalDescription);

    // Populate folder from other_config
    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
    this->m_originalFolder = otherConfig.value("folder").toString();
    this->m_currentFolder = this->m_originalFolder;
    this->updateFolderDisplay();

    // Populate tags
    QVariantList tagsVariant = this->m_objectDataCopy.value("tags").toList();
    this->m_originalTags.clear();
    for (const QVariant& tag : tagsVariant)
    {
        this->m_originalTags.append(tag.toString());
    }
    this->m_originalTags.sort();
    this->m_currentTags = this->m_originalTags;
    this->updateTagsDisplay();

    // Show/hide IQN fields for hosts
    if (this->m_objectType == XenObjectType::Host)
    {
        this->ui->lblIQN->setVisible(true);
        this->ui->txtIQN->setVisible(true);
        this->ui->lblIQNHint->setVisible(true);

        // Get IQN from other_config
        this->m_originalIQN = otherConfig.value("iscsi_iqn").toString();
        this->ui->txtIQN->setText(this->m_originalIQN);
    } else
    {
        this->ui->lblIQN->setVisible(false);
        this->ui->txtIQN->setVisible(false);
        this->ui->lblIQNHint->setVisible(false);
        this->m_originalIQN.clear();
    }

    // Update title based on object type
    if (this->m_objectType == XenObjectType::VM)
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this virtual machine"));
    } else if (this->m_objectType == XenObjectType::Host)
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this server"));
    } else if (this->m_objectType == XenObjectType::Pool)
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this pool"));
    } else if (this->m_objectType == XenObjectType::SR)
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this storage repository"));
    } else
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description"));
    }

    // Unblock signals
    this->ui->txtName->blockSignals(false);
    this->ui->txtDescription->blockSignals(false);
    this->ui->cmbFolder->blockSignals(false);
    this->ui->txtTags->blockSignals(false);
    this->ui->txtIQN->blockSignals(false);
}

AsyncOperation* GeneralEditPage::SaveSettings()
{
    // C# pattern: SaveSettings() does TWO things:
    // 1. Edit objectDataCopy directly for simple fields
    // 2. Return AsyncAction for complex operations

    // Step 1: Apply simple changes to objectDataCopy
    // These will be persisted by VerticallyTabbedDialog.applySimpleChanges()

    if (this->nameChanged())
    {
        this->m_objectDataCopy["name_label"] = this->ui->txtName->text();
    }

    if (this->descriptionChanged())
    {
        this->m_objectDataCopy["name_description"] = this->ui->txtDescription->toPlainText();
    }

    if (this->iqnChanged() && this->m_objectType == XenObjectType::Host)
    {
        // IQN is stored in other_config["iscsi_iqn"]
        QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
        otherConfig["iscsi_iqn"] = this->ui->txtIQN->text();
        this->m_objectDataCopy["other_config"] = otherConfig;
    }

    // Step 2: Return action for complex operations (folder, tags)
    // C# code:
    //   if (FolderChanged || TagsChanged)
    //       return new GeneralEditPageAction(xenObjectOrig, xenObjectCopy,
    //                                        folderEditor.Path, tagsEditor.Tags, true);

    if (this->folderChanged() || this->tagsChanged())
    {
        const QString newFolder = this->m_currentFolder.trimmed();
        QStringList newTags = this->m_currentTags;
        newTags.sort();

        return new GeneralEditPageAction(this->m_object, this->m_originalFolder, newFolder, this->m_originalTags, newTags, true);
    }

    return nullptr;
}

bool GeneralEditPage::IsValidToSave() const
{
    // Name is required
    if (this->ui->txtName->text().trimmed().isEmpty())
    {
        return false;
    }

    // IQN validation for hosts
    if (this->m_objectType == XenObjectType::Host && this->ui->lblIQN->isVisible())
    {
        QString iqn = this->ui->txtIQN->text().trimmed();
        if (!iqn.isEmpty())
        {
            // Basic IQN format validation: must start with "iqn." or "eui." or "naa."
            if (!iqn.startsWith("iqn.") && !iqn.startsWith("eui.") && !iqn.startsWith("naa."))
            {
                return false;
            }
        }
    }

    return true;
}

void GeneralEditPage::ShowLocalValidationMessages()
{
    if (this->ui->txtName->text().trimmed().isEmpty())
    {
        this->ui->txtName->setFocus();
        this->ui->txtName->setStyleSheet("border: 1px solid red;");
        // TODO: Show balloon tooltip with error message
    }

    if (this->m_objectType == XenObjectType::Host && this->ui->lblIQN->isVisible())
    {
        QString iqn = this->ui->txtIQN->text().trimmed();
        if (!iqn.isEmpty() && !iqn.startsWith("iqn.") && !iqn.startsWith("eui.") && !iqn.startsWith("naa."))
        {
            this->ui->txtIQN->setStyleSheet("border: 1px solid red;");
            // TODO: Show balloon tooltip with error message
        }
    }
}

void GeneralEditPage::HideLocalValidationMessages()
{
    this->ui->txtName->setStyleSheet("");
    this->ui->txtIQN->setStyleSheet("");
    // TODO: Hide balloon tooltips
}

void GeneralEditPage::Cleanup()
{
    // Disconnect signals, cleanup resources
    this->disconnect(this->ui->txtName, nullptr, this, nullptr);
    this->disconnect(this->ui->txtDescription, nullptr, this, nullptr);
    this->disconnect(this->ui->cmbFolder, nullptr, this, nullptr);
    this->disconnect(this->ui->txtTags, nullptr, this, nullptr);
    this->disconnect(this->ui->txtIQN, nullptr, this, nullptr);
    if (m_changeFolderButton)
        this->disconnect(m_changeFolderButton, nullptr, this, nullptr);
    if (m_editTagsButton)
        this->disconnect(m_editTagsButton, nullptr, this, nullptr);
}

bool GeneralEditPage::HasChanged() const
{
    return this->nameChanged() || this->descriptionChanged() || this->folderChanged() ||
           this->tagsChanged() || this->iqnChanged();
}

void GeneralEditPage::onNameChanged()
{
    this->HideLocalValidationMessages();
}

void GeneralEditPage::onDescriptionChanged()
{
    // No validation needed for description
}

void GeneralEditPage::onFolderChanged()
{
    const QVariant selectedFolder = this->ui->cmbFolder->currentData();
    this->m_currentFolder = selectedFolder.toString().trimmed();
}

void GeneralEditPage::onTagsChanged()
{
    // No validation needed for tags
}

void GeneralEditPage::onIQNChanged()
{
    this->HideLocalValidationMessages();
}

bool GeneralEditPage::nameChanged() const
{
    return this->ui->txtName->text() != this->m_originalName;
}

bool GeneralEditPage::descriptionChanged() const
{
    return this->ui->txtDescription->toPlainText() != this->m_originalDescription;
}

bool GeneralEditPage::folderChanged() const
{
    return this->m_currentFolder.trimmed() != this->m_originalFolder;
}

bool GeneralEditPage::tagsChanged() const
{
    QStringList currentTags = this->m_currentTags;
    currentTags.removeDuplicates();
    currentTags.sort();
    return currentTags != this->m_originalTags;
}

bool GeneralEditPage::iqnChanged() const
{
    if (this->m_objectType != XenObjectType::Host || !this->ui->lblIQN->isVisible())
    {
        return false;
    }
    return this->ui->txtIQN->text() != this->m_originalIQN;
}

QVariantMap GeneralEditPage::GetModifiedObjectData() const
{
    return this->m_objectDataCopy;
}

void GeneralEditPage::onChangeFolderClicked()
{
    if (!this->m_object || !this->m_object->GetConnection())
        return;

    FolderChangeDialog dialog(this->m_object->GetConnection(), this->m_currentFolder, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!dialog.FolderChanged())
        return;

    this->m_currentFolder = dialog.SelectedFolderPath();
    this->updateFolderDisplay();
    onFolderChanged();
}

void GeneralEditPage::onEditTagsClicked()
{
    if (!this->m_object)
        return;

    NewTagDialog dialog(this);
    dialog.SetTags(this->collectAllKnownTags(), this->m_currentTags, QStringList());
    if (dialog.exec() != QDialog::Accepted)
        return;

    this->m_currentTags = dialog.GetSelectedTags();
    this->m_currentTags.removeDuplicates();
    this->m_currentTags.sort();
    this->updateTagsDisplay();
    onTagsChanged();
}

QStringList GeneralEditPage::collectAllKnownTags() const
{
    QStringList allTags = this->m_currentTags;

    const QList<XenConnection*> connections = Xen::ConnectionsManager::instance()->GetAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->IsConnected() || !connection->GetCache())
            continue;

        const QList<QPair<XenObjectType, QString>> searchable = connection->GetCache()->GetXenSearchableObjects();
        for (const auto& pair : searchable)
        {
            if (pair.first == XenObjectType::Folder)
                continue;

            const QSharedPointer<XenObject> candidate = connection->GetCache()->ResolveObject(pair.first, pair.second);
            if (!candidate)
                continue;

            const QStringList tags = candidate->GetTags();
            for (const QString& tag : tags)
            {
                const QString cleaned = tag.trimmed();
                if (!cleaned.isEmpty())
                    allTags.append(cleaned);
            }
        }
    }

    allTags.removeDuplicates();
    allTags.sort();
    return allTags;
}

void GeneralEditPage::updateFolderDisplay()
{
    this->ui->cmbFolder->blockSignals(true);
    this->ui->cmbFolder->clear();
    this->ui->cmbFolder->setEditable(false);
    this->ui->cmbFolder->addItem(tr("(none)"), QString());

    QStringList availableFolders;
    if (this->m_object && this->m_object->GetConnection() && this->m_object->GetConnection()->GetCache())
    {
        const QStringList refs = this->m_object->GetConnection()->GetCache()->GetAllRefs(XenObjectType::Folder);
        for (const QString& ref : refs)
        {
            if (ref == FoldersManager::PATH_SEPARATOR)
                continue;
            availableFolders.append(ref);
        }
    }

    availableFolders.removeDuplicates();
    availableFolders.sort();
    for (const QString& folderPath : availableFolders)
        this->ui->cmbFolder->addItem(folderPath, folderPath);

    if (!this->m_currentFolder.isEmpty() && !availableFolders.contains(this->m_currentFolder))
        this->ui->cmbFolder->addItem(this->m_currentFolder, this->m_currentFolder);

    const int selectedIndex = this->ui->cmbFolder->findData(this->m_currentFolder);
    this->ui->cmbFolder->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    this->ui->cmbFolder->setEnabled(this->ui->cmbFolder->count() > 1);
    this->ui->cmbFolder->blockSignals(false);
}

void GeneralEditPage::updateTagsDisplay()
{
    if (this->m_currentTags.isEmpty())
        this->ui->txtTags->setPlainText(tr("(none)"));
    else
        this->ui->txtTags->setPlainText(this->m_currentTags.join(", "));
}
