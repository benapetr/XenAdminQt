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

#ifndef SRSTORAGETABPAGE_H
#define SRSTORAGETABPAGE_H

#include "basetabpage.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class SrStorageTabPage;
}
QT_END_NAMESPACE

class SR;

class SrStorageTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit SrStorageTabPage(QWidget* parent = nullptr);
        ~SrStorageTabPage();

        QString GetTitle() const override
        {
            return "Storage";
        }

        QString HelpID() const override
        {
            return "TabPageStorage";
        }

        bool IsApplicableForObjectType(const QString& objectType) const override;
        void SetObject(QSharedPointer<XenObject> object) override;
        QSharedPointer<SR> GetSR();

    protected:
        void refreshContent() override;

    private slots:
        void onRescanButtonClicked();
        void onAddButtonClicked();
        void onMoveButtonClicked();
        void onDeleteButtonClicked();
        void onEditButtonClicked();
        void onStorageTableSelectionChanged();
        void onStorageTableDoubleClicked(const QModelIndex& index);
        void onStorageTableCustomContextMenuRequested(const QPoint& pos);

    private:
        void populateSRStorage();
        QString getSelectedVDIRef() const;
        void updateButtonStates();
        void requestSrRefresh(int delayMs = 0);

        Ui::SrStorageTabPage* ui;
};

#endif // SRSTORAGETABPAGE_H
