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

// consolesoptionspage.h - Console settings options page
#ifndef CONSOLESOPTIONSPAGE_H
#define CONSOLESOPTIONSPAGE_H

#include "ioptionspage.h"
#include "ui_consolesoptionspage.h"

namespace Ui
{
    class ConsolesOptionsPage;
}

class ConsolesOptionsPage : public IOptionsPage
{
    Q_OBJECT

public:
    explicit ConsolesOptionsPage(QWidget* parent = nullptr);
    ~ConsolesOptionsPage();

    // IOptionsPage interface
    QString text() const override;
    QString subText() const override;
    QIcon image() const override;
    void build() override;
    bool isValidToSave(QWidget** control, QString& invalidReason) override;
    void showValidationMessages(QWidget* control, const QString& message) override;
    void hideValidationMessages() override;
    void save() override;

private:
    Ui::ConsolesOptionsPage* ui;

    void buildKeyCodeListBox();
    void buildDockKeyCodeComboBox();
    void buildUncaptureKeyCodeComboBox();
    void selectKeyCombo();
    void selectDockKeyCombo();
    void selectUncaptureKeyCombo();
};

#endif // CONSOLESOPTIONSPAGE_H
