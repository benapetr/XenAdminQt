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

#ifndef MEMORYTABPAGE_H
#define MEMORYTABPAGE_H

#include "basetabpage.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MemoryTabPage;
}
QT_END_NAMESPACE

/**
 * Memory tab page showing memory configuration and usage for VMs and Hosts.
 * This tab displays memory ballooning settings, dynamic/static memory limits,
 * and current memory usage information.
 */
class MemoryTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit MemoryTabPage(QWidget* parent = nullptr);
        ~MemoryTabPage();

        QString GetTitle() const override
        {
            return "Memory";
        }
        Type GetType() const override
        {
            return Type::Memory;
        }
        QString HelpID() const override
        {
            return "TabPageBallooning";
        }
        bool IsApplicableForObjectType(const QString& objectType) const override;

    protected:
        void refreshContent() override;

    private slots:
        void onEditButtonClicked();

    private:
        Ui::MemoryTabPage* ui;

        void populateVMMemory();
        void populateHostMemory();
        QString formatMemorySize(qint64 bytes) const;
        bool supportsBallooning() const;
};

#endif // MEMORYTABPAGE_H
