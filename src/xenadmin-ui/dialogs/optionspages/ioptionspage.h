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

// ioptionspage.h - Base interface for options pages
// Matches C# IOptionsPage interface
#ifndef IOPTIONSPAGE_H
#define IOPTIONSPAGE_H

#include <QWidget>
#include <QString>
#include <QIcon>

class IOptionsPage : public QWidget
{
    Q_OBJECT

    public:
        explicit IOptionsPage(QWidget* parent = nullptr)
            : QWidget(parent)
        {}
        virtual ~IOptionsPage() = default;

        // IVerticalTab interface (C# VerticalTabs.IVerticalTab)
        virtual QString GetText() const = 0;    // Main text for tab
        virtual QString GetSubText() const = 0; // Sub text for tab
        virtual QIcon GetImage() const = 0;     // Icon for tab

        // IOptionsPage interface
        virtual void Build() = 0;                                                  // Load settings into UI
        virtual bool IsValidToSave(QWidget** control, QString& invalidReason) = 0; // Validate before saving
        virtual void ShowValidationMessages(QWidget* control, const QString& message) = 0;
        virtual void HideValidationMessages() = 0;
        virtual void Save() = 0; // Save settings from UI
};

#endif // IOPTIONSPAGE_H
