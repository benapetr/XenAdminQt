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

#include "warningdialog.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

WarningDialog::WarningDialog(const QString& message, const QString& title, const QList<QPair<QString, Result>>& buttons, QWidget* parent) : QDialog(parent)
{
    this->setWindowTitle(title);

    auto* mainLayout = new QVBoxLayout(this);
    auto* contentLayout = new QHBoxLayout();

    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(this->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(32, 32));
    iconLabel->setAlignment(Qt::AlignTop);
    contentLayout->addWidget(iconLabel);

    this->m_messageLabel = new QLabel(message, this);
    this->m_messageLabel->setWordWrap(true);
    contentLayout->addWidget(this->m_messageLabel, 1);

    mainLayout->addLayout(contentLayout);

    this->m_buttonBox = new QDialogButtonBox(this);
    for (const auto& button : buttons)
    {
        QPushButton* btn = this->m_buttonBox->addButton(button.first, QDialogButtonBox::ActionRole);
        m_buttonMap.insert(static_cast<QAbstractButton*>(btn), button.second);
    }

    connect(this->m_buttonBox, &QDialogButtonBox::clicked, this, &WarningDialog::onButtonClicked);
    mainLayout->addWidget(this->m_buttonBox);

    this->setLayout(mainLayout);
}

WarningDialog::Result WarningDialog::ShowYesNo(const QString& message, const QString& title, QWidget* parent)
{
    WarningDialog dialog(message, title,
                         { {QObject::tr("Yes"), Result::Yes},
                           {QObject::tr("No"), Result::No} },
                         parent);
    dialog.exec();
    return dialog.GetResult();
}

WarningDialog::Result WarningDialog::ShowThreeButton(const QString& message, const QString& title, const QString& yesText, const QString& noText, const QString& cancelText, QWidget* parent)
{
    WarningDialog dialog(message, title,
                         { {yesText, Result::Yes},
                           {noText, Result::No},
                           {cancelText, Result::Cancel} },
                         parent);
    dialog.exec();
    return dialog.GetResult();
}

void WarningDialog::onButtonClicked(QAbstractButton* button)
{
    if (this->m_buttonMap.contains(button))
        this->m_result = m_buttonMap.value(button, Result::Cancel);
    else
        this->m_result = Result::Cancel;

    this->accept();
}
