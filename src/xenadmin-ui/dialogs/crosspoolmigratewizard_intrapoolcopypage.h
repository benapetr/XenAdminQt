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

#ifndef INTRAPOOLCOPYPAGE_H
#define INTRAPOOLCOPYPAGE_H

#include <QWizardPage>
#include <QSharedPointer>
#include <QRadioButton>
#include <QLabel>

class XenObject;
class VM;

namespace Ui {
class IntraPoolCopyPage;
}

class IntraPoolCopyPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit IntraPoolCopyPage(const QList<QString>& selectedVMs, QWidget *parent = nullptr);
        ~IntraPoolCopyPage();

        // QWizardPage interface
        void initializePage() override;
        bool validatePage() override;
        bool isComplete() const override;
        int nextId() const override;

        // Public properties matching C#
        bool CloneVM() const;
        QString SelectedSR() const;
        QString NewVmName() const;
        QString NewVmDescription() const;

    private slots:
        void onNameTextChanged();
        void onCloneRadioButtonChanged();
        void onCopyRadioButtonChanged();
        void onSrPickerSelectionChanged();
        void onSrPickerCanBeScannedChanged();
        void onButtonRescanClicked();

    private:
        Ui::IntraPoolCopyPage *ui;
        QList<QString> vmsFromSelection_;
        QString originalVmName_;
        QString originalVmDescription_;
        bool fastCloneAvailable_;

        QSharedPointer<VM> m_vm;
        QRadioButton* cloneRadioButton_;
        QLabel* fastCloneDescription_;

        void setupConnections();
        void setupFastClonePanel();
        void populatePage();
        void updateCopyModeControls();
        void updateSrPicker();
        void enableFastClone(bool enable, const QString& reason = QString());
};

#endif // INTRAPOOLCOPYPAGE_H
