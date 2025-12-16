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

#include "generaleditpage.h"
#include "ui_generaleditpage.h"
#include "../../xenlib/xenlib.h"
#include <QRegularExpression>
#include <QMessageBox>

GeneralEditPage::GeneralEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::GeneralEditPage), m_xenLib(nullptr), m_hasChanges(false)
{
    ui->setupUi(this);

    // Initially hide IQN fields (only visible for hosts)
    ui->lblIQN->setVisible(false);
    ui->txtIQN->setVisible(false);
    ui->labelIqnHint->setVisible(false);

    // Initially hide read-only description (shown for VMs with CloudConfigDrive)
    ui->lblDescrReadOnly->setVisible(false);
    ui->txtDescrReadOnly->setVisible(false);

    // Connect change signals
    connect(ui->txtName, &QLineEdit::textChanged, this, &GeneralEditPage::onNameChanged);
    connect(ui->txtDescription, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onDescriptionChanged);
    connect(ui->txtTags, &QPlainTextEdit::textChanged, this, &GeneralEditPage::onTagsChanged);
    connect(ui->txtIQN, &QLineEdit::textChanged, this, &GeneralEditPage::onIqnChanged);
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
    return ui->txtName->text();
}

QIcon GeneralEditPage::image() const
{
    return QIcon(":/icons/edit_16.png");
}

void GeneralEditPage::setXenObject(const QString& type, const QString& ref, const QVariantMap& data)
{
    m_objectType = type;
    m_objectRef = ref;
    m_objectData = data;
    m_hasChanges = false;

    // Get XenLib instance (assuming MainWindow provides access)
    if (!m_xenLib && parentWidget() && parentWidget()->parentWidget())
    {
        QObject* mainWindow = parentWidget()->parentWidget();
        m_xenLib = mainWindow->property("xenLib").value<XenLib*>();
    }

    // Populate fields
    ui->txtName->setText(getOriginalName());
    ui->txtDescription->setPlainText(getOriginalDescription());
    ui->txtFolder->setText(getOriginalFolder());

    // Tags (convert from list to comma-separated string)
    QStringList tags = getOriginalTags();
    ui->txtTags->setPlainText(tags.join(", "));

    // IQN (only for hosts)
    if (type == "host")
    {
        ui->txtIQN->setText(getOriginalIqn());
    }

    updateVisibility();
}

bool GeneralEditPage::hasChanged() const
{
    if (ui->txtName->text() != getOriginalName())
        return true;

    if (ui->lblDescription->isVisible() && ui->txtDescription->toPlainText() != getOriginalDescription())
        return true;

    // Check tags
    QStringList currentTags = ui->txtTags->toPlainText().split(",", Qt::SkipEmptyParts);
    for (QString& tag : currentTags)
        tag = tag.trimmed();
    currentTags.sort();

    QStringList originalTags = getOriginalTags();
    originalTags.sort();

    if (currentTags != originalTags)
        return true;

    // Check IQN for hosts
    if (m_objectType == "host" && ui->txtIQN->text() != getOriginalIqn())
        return true;

    return false;
}

bool GeneralEditPage::validateToSave(QString& error)
{
    // Name must not be empty
    if (ui->txtName->text().trimmed().isEmpty())
    {
        error = tr("Name cannot be empty");
        ui->txtName->setFocus();
        return false;
    }

    // Validate IQN if this is a host and IQN has changed
    if (m_objectType == "host")
    {
        QString currentIqn = ui->txtIQN->text().trimmed();
        QString originalIqn = getOriginalIqn();

        if (currentIqn != originalIqn)
        {
            if (!validateIqn(currentIqn))
            {
                error = tr("Invalid iSCSI IQN format. IQN must be in format:\n"
                           "iqn.yyyy-mm.reverse.domain.name:identifier\n"
                           "or eui.xxxxxxxxxxxx");
                ui->txtIQN->setFocus();
                return false;
            }
        }
    }

    return true;
}

void GeneralEditPage::saveChanges()
{
    if (!m_xenLib)
        return;

    // Prepare name/description changes
    QVariantMap changes;

    if (ui->txtName->text() != getOriginalName())
    {
        changes["name_label"] = ui->txtName->text();
    }

    if (ui->lblDescription->isVisible() && ui->txtDescription->toPlainText() != getOriginalDescription())
    {
        changes["name_description"] = ui->txtDescription->toPlainText();
    }

    // Save basic fields
    if (!changes.isEmpty())
    {
        m_xenLib->setObjectProperties(m_objectType, m_objectRef, changes);
    }

    // Save tags
    QStringList currentTags = ui->txtTags->toPlainText().split(",", Qt::SkipEmptyParts);
    for (QString& tag : currentTags)
        tag = tag.trimmed();
    currentTags.removeAll(QString()); // Remove empty strings

    QStringList originalTags = getOriginalTags();
    if (currentTags != originalTags)
    {
        m_xenLib->setObjectTags(m_objectType, m_objectRef, currentTags);
    }

    // Save IQN for hosts
    if (m_objectType == "host" && ui->txtIQN->text() != getOriginalIqn())
    {
        m_xenLib->setHostIqn(m_objectRef, ui->txtIQN->text());
    }

    m_hasChanges = false;
}

void GeneralEditPage::onNameChanged()
{
    emit contentChanged();
}

void GeneralEditPage::onDescriptionChanged()
{
    emit contentChanged();
}

void GeneralEditPage::onTagsChanged()
{
    emit contentChanged();
}

void GeneralEditPage::onIqnChanged()
{
    emit contentChanged();
}

void GeneralEditPage::updateVisibility()
{
    // Show/hide IQN based on object type
    bool isHost = (m_objectType == "host");
    ui->lblIQN->setVisible(isHost);
    ui->txtIQN->setVisible(isHost);
    ui->labelIqnHint->setVisible(isHost);

    // Handle description field visibility
    // For VMs with cloud-config drive, show read-only description instead of editable
    bool showReadOnlyDescription = false;
    if (m_objectType == "vm")
    {
        // Check if VM has cloud-config drive (simplified check)
        QVariantMap other_config = m_objectData.value("other_config").toMap();
        showReadOnlyDescription = other_config.contains("config-drive");
    }

    ui->lblDescription->setVisible(!showReadOnlyDescription);
    ui->txtDescription->setVisible(!showReadOnlyDescription);
    ui->lblDescrReadOnly->setVisible(showReadOnlyDescription);
    ui->txtDescrReadOnly->setVisible(showReadOnlyDescription);

    if (showReadOnlyDescription)
    {
        ui->txtDescrReadOnly->setText(getOriginalDescription());
    }

    // Hide folder/tags for VMSS and VM_appliance (not implemented yet, placeholder)
    bool isVMSS = (m_objectType == "vmss");
    bool isVMAppliance = (m_objectType == "vm_appliance");

    if (isVMSS || isVMAppliance)
    {
        ui->lblFolder->setVisible(false);
        ui->txtFolder->setVisible(false);
        ui->lblTags->setVisible(false);
        ui->txtTags->setVisible(false);

        if (isVMSS)
        {
            ui->labelTitle->setText(tr("Snapshot Schedule Settings"));
        } else
        {
            ui->labelTitle->setText(tr("VM Appliance Settings"));
        }
    }
}

QString GeneralEditPage::getOriginalName() const
{
    return m_objectData.value("name_label").toString();
}

QString GeneralEditPage::getOriginalDescription() const
{
    return m_objectData.value("name_description").toString();
}

QString GeneralEditPage::getOriginalFolder() const
{
    // Folder path is stored in object's path property
    return m_objectData.value("path", "/").toString();
}

QStringList GeneralEditPage::getOriginalTags() const
{
    QVariant tagsVariant = m_objectData.value("tags");
    if (tagsVariant.canConvert<QStringList>())
    {
        return tagsVariant.toStringList();
    }
    return QStringList();
}

QString GeneralEditPage::getOriginalIqn() const
{
    if (m_objectType != "host")
        return QString();

    QVariantMap other_config = m_objectData.value("other_config").toMap();
    return other_config.value("iscsi_iqn").toString();
}

bool GeneralEditPage::validateIqn(const QString& iqn) const
{
    if (iqn.isEmpty())
        return true; // Empty IQN is valid

    // IQN format: iqn.yyyy-mm.reverse.domain.name:identifier
    // or EUI format: eui.xxxxxxxxxxxx

    QRegularExpression iqnRegex("^iqn\\.\\d{4}-\\d{2}\\.[a-z0-9]([a-z0-9\\-]*[a-z0-9])?(\\.[a-z0-9]([a-z0-9\\-]*[a-z0-9])?)*:[\\w\\.\\-:]+$",
                                QRegularExpression::CaseInsensitiveOption);

    QRegularExpression euiRegex("^eui\\.[0-9A-F]{16}$",
                                QRegularExpression::CaseInsensitiveOption);

    return iqnRegex.match(iqn).hasMatch() || euiRegex.match(iqn).hasMatch();
}
