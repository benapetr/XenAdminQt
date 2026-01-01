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

#ifndef POOLADVANCEDEDITPAGE_H
#define POOLADVANCEDEDITPAGE_H

#include "ieditpage.h"
#include <QVariantMap>

namespace Ui {
class PoolAdvancedEditPage;
}

class AsyncOperation;

/**
 * @brief Advanced pool settings edit page
 *
 * Qt equivalent of C# XenAdmin.SettingsPanels.PoolAdvancedEditPage
 * Allows configuration of advanced pool settings (currently migration compression).
 *
 * C# Reference: xenadmin/XenAdmin/SettingsPanels/PoolAdvancedEditPage.cs
 */
class PoolAdvancedEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit PoolAdvancedEditPage(QWidget* parent = nullptr);
        ~PoolAdvancedEditPage() override;

        // IEditPage interface
        QString text() const override;
        QString subText() const override;
        QIcon image() const override;

        void setXenObjects(const QString& objectRef,
                        const QString& objectType,
                        const QVariantMap& objectDataBefore,
                        const QVariantMap& objectDataCopy) override;

        AsyncOperation* saveSettings() override;
        bool hasChanged() const override;
        bool isValidToSave() const override;
        void showLocalValidationMessages() override;
        void hideLocalValidationMessages() override;
        void cleanup() override;

    private:
        Ui::PoolAdvancedEditPage* ui;

        QString m_poolRef_;
        QVariantMap m_objectDataBefore_;
        QVariantMap m_objectDataCopy_;
};

#endif // POOLADVANCEDEDITPAGE_H
