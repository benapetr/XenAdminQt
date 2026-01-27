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

// saveandrestoreoptionspage.h - Save and restore settings options page
#ifndef SAVEANDRESTOREOPTIONSPAGE_H
#define SAVEANDRESTOREOPTIONSPAGE_H

#include "ioptionspage.h"
#include "ui_saveandrestoreoptionspage.h"
#include <QtCore/QByteArray>

namespace Ui
{
    class SaveAndRestoreOptionsPage;
}

/**
 * @brief Options page for save and restore settings including main password management.
 * 
 * Matches C# XenAdmin.Dialogs.OptionsPages.SaveAndRestoreOptionsPage
 */
class SaveAndRestoreOptionsPage : public IOptionsPage
{
    Q_OBJECT

    public:
        explicit SaveAndRestoreOptionsPage(QWidget* parent = nullptr);
        ~SaveAndRestoreOptionsPage();

        // IOptionsPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;
        void Build() override;
        bool IsValidToSave(QWidget** control, QString& invalidReason) override;
        void ShowValidationMessages(QWidget* control, const QString& message) override;
        void HideValidationMessages() override;
        void Save() override;

        /**
         * @brief Set whether to save the server list on OK.
         * 
         * Matches C# SaveAndRestoreOptionsPage.SaveAllAfter
         * @param saveAllAfter True to save server list, false otherwise
         */
        void SetSaveAllAfter(bool saveAllAfter);

    private slots:
        void changeMainPasswordButton_Click();
        void requireMainPasswordCheckBox_Click();
        void saveStateCheckBox_Click();

    private:
        void SaveEverything();

        Ui::SaveAndRestoreOptionsPage* ui;
        QByteArray mainPassword_;
        bool saveAllAfter_;
};

#endif // SAVEANDRESTOREOPTIONSPAGE_H
