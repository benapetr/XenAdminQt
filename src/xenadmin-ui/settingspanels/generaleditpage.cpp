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
#include "../../xenlib/xen/connection.h"
#include <QIcon>
#include <QDebug>

GeneralEditPage::GeneralEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::GeneralEditPage)
{
    ui->setupUi(this);

    // Connect change signals for change tracking
    connect(ui->txtName, &QLineEdit::textChanged, this, &GeneralEditPage::onNameChanged);
    connect(ui->txtDescription, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onDescriptionChanged);
    connect(ui->cmbFolder, &QComboBox::currentTextChanged, this, &GeneralEditPage::onFolderChanged);
    connect(ui->txtTags, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onTagsChanged);
    connect(ui->txtIQN, &QLineEdit::textChanged, this, &GeneralEditPage::onIQNChanged);

    // Hide IQN fields by default (only shown for hosts)
    ui->lblIQN->setVisible(false);
    ui->txtIQN->setVisible(false);
    ui->lblIQNHint->setVisible(false);
}

GeneralEditPage::~GeneralEditPage()
{
    delete ui;
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
    return QIcon(":/icons/edit_16.png");
}

void GeneralEditPage::setXenObjects(const QString& objectRef,
                                    const QString& objectType,
                                    const QVariantMap& objectDataBefore,
                                    const QVariantMap& objectDataCopy)
{
    m_objectRef = objectRef;
    m_objectType = objectType;
    m_objectDataBefore = objectDataBefore;
    m_objectDataCopy = objectDataCopy;

    repopulate();

    emit populated();
}

void GeneralEditPage::repopulate()
{
    // Block signals while populating to avoid triggering change tracking
    ui->txtName->blockSignals(true);
    ui->txtDescription->blockSignals(true);
    ui->cmbFolder->blockSignals(true);
    ui->txtTags->blockSignals(true);
    ui->txtIQN->blockSignals(true);

    // Populate name
    m_originalName = m_objectDataCopy.value("name_label").toString();
    ui->txtName->setText(m_originalName);

    // Populate description
    m_originalDescription = m_objectDataCopy.value("name_description").toString();
    ui->txtDescription->setPlainText(m_originalDescription);

    // Populate folder from other_config
    QVariantMap otherConfig = m_objectDataCopy.value("other_config").toMap();
    m_originalFolder = otherConfig.value("folder").toString();
    ui->cmbFolder->setEditText(m_originalFolder);

    // TODO: Populate folder dropdown with available folders from cache

    // Populate tags
    QVariantList tagsVariant = m_objectDataCopy.value("tags").toList();
    m_originalTags.clear();
    for (const QVariant& tag : tagsVariant)
    {
        m_originalTags.append(tag.toString());
    }
    m_originalTags.sort();
    ui->txtTags->setPlainText(m_originalTags.join(", "));

    // Show/hide IQN fields for hosts
    if (m_objectType == "host")
    {
        ui->lblIQN->setVisible(true);
        ui->txtIQN->setVisible(true);
        ui->lblIQNHint->setVisible(true);

        // Get IQN from other_config
        m_originalIQN = otherConfig.value("iscsi_iqn").toString();
        ui->txtIQN->setText(m_originalIQN);
    } else
    {
        ui->lblIQN->setVisible(false);
        ui->txtIQN->setVisible(false);
        ui->lblIQNHint->setVisible(false);
        m_originalIQN.clear();
    }

    // Update title based on object type
    if (m_objectType == "vm")
    {
        ui->labelTitle->setText(tr("Enter a meaningful name and description for this virtual machine"));
    } else if (m_objectType == "host")
    {
        ui->labelTitle->setText(tr("Enter a meaningful name and description for this server"));
    } else if (m_objectType == "pool")
    {
        ui->labelTitle->setText(tr("Enter a meaningful name and description for this pool"));
    } else if (m_objectType == "sr")
    {
        ui->labelTitle->setText(tr("Enter a meaningful name and description for this storage repository"));
    } else
    {
        ui->labelTitle->setText(tr("Enter a meaningful name and description"));
    }

    // Unblock signals
    ui->txtName->blockSignals(false);
    ui->txtDescription->blockSignals(false);
    ui->cmbFolder->blockSignals(false);
    ui->txtTags->blockSignals(false);
    ui->txtIQN->blockSignals(false);
}

AsyncOperation* GeneralEditPage::saveSettings()
{
    // C# pattern: SaveSettings() does TWO things:
    // 1. Edit objectDataCopy directly for simple fields
    // 2. Return AsyncAction for complex operations

    // Step 1: Apply simple changes to objectDataCopy
    // These will be persisted by VerticallyTabbedDialog.applySimpleChanges()

    if (nameChanged())
    {
        m_objectDataCopy["name_label"] = ui->txtName->text();
        qDebug() << "GeneralEditPage: Name changed to" << ui->txtName->text();
    }

    if (descriptionChanged())
    {
        m_objectDataCopy["name_description"] = ui->txtDescription->toPlainText();
        qDebug() << "GeneralEditPage: Description changed";
    }

    if (iqnChanged() && m_objectType == "host")
    {
        // IQN is stored in other_config["iscsi_iqn"]
        QVariantMap otherConfig = m_objectDataCopy.value("other_config").toMap();
        otherConfig["iscsi_iqn"] = ui->txtIQN->text();
        m_objectDataCopy["other_config"] = otherConfig;
        qDebug() << "GeneralEditPage: IQN changed to" << ui->txtIQN->text();
    }

    // Step 2: Return action for complex operations (folder, tags)
    // C# code:
    //   if (FolderChanged || TagsChanged)
    //       return new GeneralEditPageAction(xenObjectOrig, xenObjectCopy,
    //                                        folderEditor.Path, tagsEditor.Tags, true);

    if (folderChanged() || tagsChanged())
    {
        QString newFolder = ui->cmbFolder->currentText().trimmed();
        QStringList newTags = parseTagsFromText();

        qDebug() << "GeneralEditPage: Creating GeneralEditPageAction - folder:" << newFolder
                 << "tags:" << newTags;

        // TODO: Get connection from parent dialog
        // For now, return nullptr - full implementation needs connection reference
        // return new GeneralEditPageAction(
        //     connection,
        //     m_objectRef,
        //     m_objectType,
        //     m_originalFolder,
        //     newFolder,
        //     m_originalTags,
        //     newTags,
        //     true
        // );
    }

    return nullptr;
}

bool GeneralEditPage::isValidToSave() const
{
    // Name is required
    if (ui->txtName->text().trimmed().isEmpty())
    {
        return false;
    }

    // IQN validation for hosts
    if (m_objectType == "host" && ui->lblIQN->isVisible())
    {
        QString iqn = ui->txtIQN->text().trimmed();
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
    if (ui->txtName->text().trimmed().isEmpty())
    {
        ui->txtName->setFocus();
        ui->txtName->setStyleSheet("border: 1px solid red;");
        // TODO: Show balloon tooltip with error message
    }

    if (m_objectType == "host" && ui->lblIQN->isVisible())
    {
        QString iqn = ui->txtIQN->text().trimmed();
        if (!iqn.isEmpty() && !iqn.startsWith("iqn.") && !iqn.startsWith("eui.") && !iqn.startsWith("naa."))
        {
            ui->txtIQN->setStyleSheet("border: 1px solid red;");
            // TODO: Show balloon tooltip with error message
        }
    }
}

void GeneralEditPage::hideLocalValidationMessages()
{
    ui->txtName->setStyleSheet("");
    ui->txtIQN->setStyleSheet("");
    // TODO: Hide balloon tooltips
}

void GeneralEditPage::cleanup()
{
    // Disconnect signals, cleanup resources
    disconnect(ui->txtName, nullptr, this, nullptr);
    disconnect(ui->txtDescription, nullptr, this, nullptr);
    disconnect(ui->cmbFolder, nullptr, this, nullptr);
    disconnect(ui->txtTags, nullptr, this, nullptr);
    disconnect(ui->txtIQN, nullptr, this, nullptr);
}

bool GeneralEditPage::hasChanged() const
{
    return nameChanged() || descriptionChanged() || folderChanged() ||
           tagsChanged() || iqnChanged();
}

void GeneralEditPage::onNameChanged()
{
    hideLocalValidationMessages();
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
    hideLocalValidationMessages();
}

QStringList GeneralEditPage::parseTagsFromText() const
{
    QString tagsText = ui->txtTags->toPlainText();
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
    return ui->txtName->text() != m_originalName;
}

bool GeneralEditPage::descriptionChanged() const
{
    return ui->txtDescription->toPlainText() != m_originalDescription;
}

bool GeneralEditPage::folderChanged() const
{
    return ui->cmbFolder->currentText().trimmed() != m_originalFolder;
}

bool GeneralEditPage::tagsChanged() const
{
    QStringList currentTags = parseTagsFromText();
    return currentTags != m_originalTags;
}

bool GeneralEditPage::iqnChanged() const
{
    if (m_objectType != "host" || !ui->lblIQN->isVisible())
    {
        return false;
    }
    return ui->txtIQN->text() != m_originalIQN;
}
