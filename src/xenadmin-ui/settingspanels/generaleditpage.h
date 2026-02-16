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

// generaleditpage.h - General properties edit page
// Matches C# XenAdmin.SettingsPanels.GeneralEditPage
#ifndef GENERALEDITPAGE_H
#define GENERALEDITPAGE_H

#include "ieditpage.h"
#include "xenlib/xen/xenobjecttype.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Ui
{
    class GeneralEditPage;
}

class AsyncOperation;
class GeneralEditPageAction;
class QPushButton;

/**
 * @brief GeneralEditPage - Edit general properties (name, description, folder, tags)
 *
 * Qt equivalent of C# XenAdmin.SettingsPanels.GeneralEditPage
 *
 * This page allows editing basic properties common to most XenServer objects:
 * - Name (name_label)
 * - Description (name_description)
 * - Folder (stored in other_config["folder"])
 * - Tags (string list)
 * - iSCSI IQN (for hosts only)
 *
 * Key C# pattern implemented:
 * SaveSettings() uses MIXED approach:
 * 1. Simple properties (name, description, IQN) - Direct edits to objectDataCopy
 * 2. Complex operations (folder, tags) - Return GeneralEditPageAction
 *
 * C# source: XenAdmin/SettingsPanels/GeneralEditPage.cs
 */
class GeneralEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit GeneralEditPage(QWidget* parent = nullptr);
        ~GeneralEditPage();

        // IVerticalTab interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        // IEditPage interface
        void SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantMap GetModifiedObjectData() const override;

    private slots:
        void onNameChanged();
        void onDescriptionChanged();
        void onFolderChanged();
        void onTagsChanged();
        void onIQNChanged();
        void onChangeFolderClicked();
        void onEditTagsClicked();

    private:
        void repopulate();
        QStringList collectAllKnownTags() const;
        void updateFolderDisplay();
        void updateTagsDisplay();
        bool nameChanged() const;
        bool descriptionChanged() const;
        bool folderChanged() const;
        bool tagsChanged() const;
        bool iqnChanged() const;

        Ui::GeneralEditPage* ui;

        QString m_objectRef;
        XenObjectType m_objectType = XenObjectType::Null;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;

        // Original values for change tracking
        QString m_originalName;
        QString m_originalDescription;
        QString m_originalFolder;
        QStringList m_originalTags;
        QString m_originalIQN;

        QString m_currentFolder;
        QStringList m_currentTags;

        QPushButton* m_changeFolderButton = nullptr;
        QPushButton* m_editTagsButton = nullptr;
};

#endif // GENERALEDITPAGE_H
