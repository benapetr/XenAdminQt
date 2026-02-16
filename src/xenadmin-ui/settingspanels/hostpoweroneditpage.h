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

#ifndef HOSTPOWERONEDITPAGE_H
#define HOSTPOWERONEDITPAGE_H

#include "ieditpage.h"
#include <QWidget>
#include <QMap>
#include <QVariantMap>

namespace Ui {
class HostPowerONEditPage;
}

// Include PowerOnMode definition from action
#include "../../xenlib/xen/actions/host/savepoweronsettingsaction.h"

class XenConnection;
class AsyncOperation;

/*!
 * \brief Host Power-On configuration edit page
 * 
 * Allows configuration of remote power management for hosts:
 * - Disabled: No remote power-on
 * - Wake-on-LAN: Standard WOL protocol
 * - iLO: HP Integrated Lights-Out (hidden on Stockholm+)
 * - DRAC: Dell Remote Access Controller
 * - Custom: User-defined power-on method
 * 
 * C# Reference: xenadmin/XenAdmin/SettingsPanels/HostPowerONEditPage.cs
 */
class HostPowerONEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit HostPowerONEditPage(QWidget *parent = nullptr);
        ~HostPowerONEditPage() override;

        // IEditPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;
        void SetXenObject(QSharedPointer<XenObject> object,
                          const QVariantMap& objectDataBefore,
                          const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool HasChanged() const override;
        bool IsValidToSave() const override;
        void Cleanup() override;

        // IEditPage pure virtual methods
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;

    signals:
        void populated();

    private slots:
        void onRadioDisabledToggled(bool checked);
        void onRadioWakeOnLANToggled(bool checked);
        void onRadioILOToggled(bool checked);
        void onRadioDRACToggled(bool checked);
        void onRadioCustomToggled(bool checked);
        void onTextInterfaceChanged(const QString& text);
        void onTextUserChanged(const QString& text);
        void onTextPasswordChanged(const QString& text);
        void onTextCustomModeChanged(const QString& text);
        void onCustomParamsCellChanged(int row, int column);

    private:
        void updateModeFromCredentials();
        void updateModeFromCustom();
        bool hasHostChanged(const PowerOnMode& originalMode, const PowerOnMode& currentMode) const;
        void updateUIForMode(PowerOnMode::Type type);

        Ui::HostPowerONEditPage *ui;

        QString m_hostRef;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;

        PowerOnMode m_currentMode;
        PowerOnMode m_originalMode;

        bool m_programmaticUpdate;
        bool m_loaded;

        // Validation
        QString m_validationMessage;
        QWidget* m_validationWidget;
};

#endif // HOSTPOWERONEDITPAGE_H
