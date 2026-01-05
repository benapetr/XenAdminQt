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

#ifndef GENERALTABPAGE_H
#define GENERALTABPAGE_H

#include "basetabpage.h"
#include <QFormLayout>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class GeneralTabPage;
}
QT_END_NAMESPACE

/**
 * General tab page showing basic information about any Xen object.
 * This tab is applicable to all object types and displays common properties
 * like name, description, UUID, and type-specific information.
 */
class GeneralTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit GeneralTabPage(QWidget* parent = nullptr);
        ~GeneralTabPage();

        QString GetTitle() const override
        {
            return "General";
        }
        QString HelpID() const override
        {
            return "TabPageGeneral";
        }
        bool IsApplicableForObjectType(const QString& objectType) const override;

    protected:
        void refreshContent() override;

    private:
        Ui::GeneralTabPage* ui;

        // Layout pointers for each section
        QFormLayout* m_generalLayout;
        QFormLayout* m_biosLayout;
        QFormLayout* m_managementInterfacesLayout;
        QFormLayout* m_memoryLayout;
        QFormLayout* m_cpuLayout;
        QFormLayout* m_versionLayout;
        QFormLayout* m_statusLayout;
        QFormLayout* m_multipathingLayout;

        void clearProperties();
        void addProperty(const QString& label, const QString& value);
        void addPropertyToLayout(QFormLayout* layout, const QString& label, const QString& value);
        void populateVMProperties();
        void populateHostProperties();
        void populatePoolProperties();
        void populateSRProperties();
        void populateNetworkProperties();

        // Host section population methods
        void populateGeneralSection();
        void populateBIOSSection();
        void populateManagementInterfacesSection();
        void populateMemorySection();
        void populateCPUSection();
        void populateVersionSection();

        // SR section population methods (C# GenerateStatusBox, GenerateMultipathBox)
        void populateStatusSection();
        void populateMultipathingSection();

        // Helper methods for formatting (matches C# PrettyTimeSpan)
        QString formatUptime(qint64 seconds) const;
};

#endif // GENERALTABPAGE_H
