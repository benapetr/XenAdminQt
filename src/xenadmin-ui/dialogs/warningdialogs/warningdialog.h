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

#ifndef WARNINGDIALOG_H
#define WARNINGDIALOG_H

#include <QDialog>
#include <QMap>

class QDialogButtonBox;
class QLabel;
class QAbstractButton;

/**
 * @brief Generic warning dialog matching C# WarningDialog usage.
 *
 * Supports custom button sets (Yes/No/Cancel) and returns a Result.
 */
class WarningDialog : public QDialog
{
    Q_OBJECT

    public:
        enum class Result
        {
            Yes,
            No,
            Cancel
        };

        explicit WarningDialog(const QString& message,
                               const QString& title,
                               const QList<QPair<QString, Result>>& buttons,
                               QWidget* parent = nullptr);

        Result GetResult() const { return m_result; }

        static Result ShowYesNo(const QString& message,
                                const QString& title,
                                QWidget* parent = nullptr);

        static Result ShowThreeButton(const QString& message,
                                      const QString& title,
                                      const QString& yesText,
                                      const QString& noText,
                                      const QString& cancelText,
                                      QWidget* parent = nullptr);

    private slots:
        void onButtonClicked(QAbstractButton* button);

    private:
        Result m_result = Result::Cancel;
        QDialogButtonBox* m_buttonBox = nullptr;
        QMap<QAbstractButton*, Result> m_buttonMap;
        QLabel* m_messageLabel = nullptr;
};

#endif // WARNINGDIALOG_H
