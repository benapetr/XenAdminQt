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

// displayoptionspage.h - Display settings options page
#ifndef DISPLAYOPTIONSPAGE_H
#define DISPLAYOPTIONSPAGE_H

#include "ioptionspage.h"
#include "ui_displayoptionspage.h"

namespace Ui
{
    class DisplayOptionsPage;
}

class DisplayOptionsPage : public IOptionsPage
{
    Q_OBJECT

public:
    explicit DisplayOptionsPage(QWidget* parent = nullptr);
    ~DisplayOptionsPage();

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
    Ui::DisplayOptionsPage* ui;
};

#endif // DISPLAYOPTIONSPAGE_H
