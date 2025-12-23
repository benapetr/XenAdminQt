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
#include "../../xenlib/xen/actions/general/generaleditpageaction.h"
#include "../../xenlib/xen/network/connection.h"
#include <QIcon>
#include <QDebug>

GeneralEditPage::GeneralEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::GeneralEditPage)
{
    this->ui->setupUi(this);

    // Connect change signals for change tracking
    this->connect(this->ui->txtName, &QLineEdit::textChanged, this, &GeneralEditPage::onNameChanged);
    this->connect(this->ui->txtDescription, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onDescriptionChanged);
    this->connect(this->ui->cmbFolder, &QComboBox::currentTextChanged, this, &GeneralEditPage::onFolderChanged);
    this->connect(this->ui->txtTags, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onTagsChanged);
    this->connect(this->ui->txtIQN, &QLineEdit::textChanged, this, &GeneralEditPage::onIQNChanged);

    // Hide IQN fields by default (only shown for hosts)
    this->ui->lblIQN->setVisible(false);
    this->ui->txtIQN->setVisible(false);
    this->ui->lblIQNHint->setVisible(false);
}

GeneralEditPage::~GeneralEditPage()
{
    delete this->ui;
}

QString GeneralEditPage::text() const
{
    return tr("General");
}

QString GeneralEditPage::subText() const
{
    return tr("Name, Description, Tags");
}

QIcon GeneralEditPage::image() const
{
    // Matches C# Images.StaticImages.edit_16
    return QIcon(":/icons/edit_16.png");
}

void GeneralEditPage::setXenObjects(const QString& objectRef,
                                    const QString& objectType,
                                    const QVariantMap& objectDataBefore,
                                    const QVariantMap& objectDataCopy)
{
    this->m_objectRef = objectRef;
    this->m_objectType = objectType;
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
    this->m_originalName = this->m_objectDataCopy.value("name_label").toString();
    this->ui->txtName->setText(this->m_originalName);

    // Populate description
    this->m_originalDescription = this->m_objectDataCopy.value("name_description").toString();
    this->ui->txtDescription->setPlainText(this->m_originalDescription);

    // Populate folder from other_config
    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
    this->m_originalFolder = otherConfig.value("folder").toString();
    this->ui->cmbFolder->setEditText(this->m_originalFolder);

    // TODO: Populate folder dropdown with available folders from cache

    // Populate tags
    QVariantList tagsVariant = this->m_objectDataCopy.value("tags").toList();
    this->m_originalTags.clear();
    for (const QVariant& tag : tagsVariant)
    {
        this->m_originalTags.append(tag.toString());
    }
    this->m_originalTags.sort();
    this->ui->txtTags->setPlainText(this->m_originalTags.join(", "));

    // Show/hide IQN fields for hosts
    if (this->m_objectType == "host")
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
    if (this->m_objectType == "vm")
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this virtual machine"));
    } else if (this->m_objectType == "host")
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this server"));
    } else if (this->m_objectType == "pool")
    {
        this->ui->labelTitle->setText(tr("Enter a meaningful name and description for this pool"));
    } else if (this->m_objectType == "sr")
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

AsyncOperation* GeneralEditPage::saveSettings()
{
    // C# pattern: SaveSettings() does TWO things:
    // 1. Edit objectDataCopy directly for simple fields
    // 2. Return AsyncAction for complex operations

    // Step 1: Apply simple changes to objectDataCopy
    // These will be persisted by VerticallyTabbedDialog.applySimpleChanges()

    if (this->nameChanged())
    {
        this->m_objectDataCopy["name_label"] = this->ui->txtName->text();
        qDebug() << "GeneralEditPage: Name changed to" << this->ui->txtName->text();
    }

    if (this->descriptionChanged())
    {
        this->m_objectDataCopy["name_description"] = this->ui->txtDescription->toPlainText();
        qDebug() << "GeneralEditPage: Description changed";
    }

    if (this->iqnChanged() && this->m_objectType == "host")
    {
        // IQN is stored in other_config["iscsi_iqn"]
        QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
        otherConfig["iscsi_iqn"] = this->ui->txtIQN->text();
        this->m_objectDataCopy["other_config"] = otherConfig;
        qDebug() << "GeneralEditPage: IQN changed to" << this->ui->txtIQN->text();
    }

    // Step 2: Return action for complex operations (folder, tags)
    // C# code:
    //   if (FolderChanged || TagsChanged)
    //       return new GeneralEditPageAction(xenObjectOrig, xenObjectCopy,
    //                                        folderEditor.Path, tagsEditor.Tags, true);

    if (this->folderChanged() || this->tagsChanged())
    {
        QString newFolder = this->ui->cmbFolder->currentText().trimmed();
        QStringList newTags = this->parseTagsFromText();

        qDebug() << "GeneralEditPage: Creating GeneralEditPageAction - folder:" << newFolder
                 << "tags:" << newTags;

        return new GeneralEditPageAction(
             this->m_connection,
             this->m_objectRef,
             this->m_objectType,
             this->m_originalFolder,
             newFolder,
             this->m_originalTags,
             newTags,
             true);
    }

    return nullptr;
}

bool GeneralEditPage::isValidToSave() const
{
    // Name is required
    if (this->ui->txtName->text().trimmed().isEmpty())
    {
        return false;
    }

    // IQN validation for hosts
    if (this->m_objectType == "host" && this->ui->lblIQN->isVisible())
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

void GeneralEditPage::showLocalValidationMessages()
{
    if (this->ui->txtName->text().trimmed().isEmpty())
    {
        this->ui->txtName->setFocus();
        this->ui->txtName->setStyleSheet("border: 1px solid red;");
        // TODO: Show balloon tooltip with error message
    }

    if (this->m_objectType == "host" && this->ui->lblIQN->isVisible())
    {
        QString iqn = this->ui->txtIQN->text().trimmed();
        if (!iqn.isEmpty() && !iqn.startsWith("iqn.") && !iqn.startsWith("eui.") && !iqn.startsWith("naa."))
        {
            this->ui->txtIQN->setStyleSheet("border: 1px solid red;");
            // TODO: Show balloon tooltip with error message
        }
    }
}

void GeneralEditPage::hideLocalValidationMessages()
{
    this->ui->txtName->setStyleSheet("");
    this->ui->txtIQN->setStyleSheet("");
    // TODO: Hide balloon tooltips
}

void GeneralEditPage::cleanup()
{
    // Disconnect signals, cleanup resources
    this->disconnect(this->ui->txtName, nullptr, this, nullptr);
    this->disconnect(this->ui->txtDescription, nullptr, this, nullptr);
    this->disconnect(this->ui->cmbFolder, nullptr, this, nullptr);
    this->disconnect(this->ui->txtTags, nullptr, this, nullptr);
    this->disconnect(this->ui->txtIQN, nullptr, this, nullptr);
}

bool GeneralEditPage::hasChanged() const
{
    return this->nameChanged() || this->descriptionChanged() || this->folderChanged() ||
           this->tagsChanged() || this->iqnChanged();
}

void GeneralEditPage::onNameChanged()
{
    this->hideLocalValidationMessages();
}

void GeneralEditPage::onDescriptionChanged()
{
    // No validation needed for description
}

void GeneralEditPage::onFolderChanged()
{
    // No validation needed for folder
}

void GeneralEditPage::onTagsChanged()
{
    // No validation needed for tags
}

void GeneralEditPage::onIQNChanged()
{
    this->hideLocalValidationMessages();
}

QStringList GeneralEditPage::parseTagsFromText() const
{
    QString tagsText = this->ui->txtTags->toPlainText();
    QStringList tags;

    // Split by comma and trim each tag
    QStringList rawTags = tagsText.split(',', Qt::SkipEmptyParts);
    for (const QString& tag : rawTags)
    {
        QString trimmed = tag.trimmed();
        if (!trimmed.isEmpty())
        {
            tags.append(trimmed);
        }
    }

    tags.sort();
    return tags;
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
    return this->ui->cmbFolder->currentText().trimmed() != this->m_originalFolder;
}

bool GeneralEditPage::tagsChanged() const
{
    QStringList currentTags = this->parseTagsFromText();
    return currentTags != this->m_originalTags;
}

bool GeneralEditPage::iqnChanged() const
{
    if (this->m_objectType != "host" || !this->ui->lblIQN->isVisible())
    {
        return false;
    }
    return this->ui->txtIQN->text() != this->m_originalIQN;
}

QVariantMap GeneralEditPage::getModifiedObjectData() const
{
    return this->m_objectDataCopy;
}
