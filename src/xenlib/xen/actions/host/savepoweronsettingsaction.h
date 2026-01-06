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

#ifndef SAVEPOWERONSETTINGSACTION_H
#define SAVEPOWERONSETTINGSACTION_H

#include "../../asyncoperation.h"
#include <QPair>
#include <QList>
#include <QString>
#include <QMap>

class XenConnection;

// Forward declaration - PowerOnMode is defined in hostpoweroneditpage.h
// We only need the struct name here, actual implementation is in the UI layer
struct PowerOnMode
{
    enum Type {
        Disabled,
        WakeOnLAN,
        iLO,
        DRAC,
        Custom
    };

    Type type;
    QString customMode;
    QString ipAddress;
    QString username;
    QString password;
    QMap<QString, QString> customConfig;

    PowerOnMode() : type(Disabled) {}
    QString toString() const;
};

/*!
 * \brief Action to save power-on configuration for hosts
 * 
 * Configures remote power management settings:
 * - Disabled: No remote power-on
 * - Wake-on-LAN: Standard WOL protocol
 * - iLO: HP Integrated Lights-Out (creates secret for password)
 * - DRAC: Dell Remote Access Controller (creates secret for password)
 * - Custom: User-defined power-on method with custom parameters
 * 
 * For iLO/DRAC/Custom modes with passwords, this action:
 * 1. Creates a Secret object to store the password
 * 2. Stores the secret UUID in power_on_config
 * 3. Calls Host.set_power_on_mode with the mode string and config
 * 
 * C# Reference: xenadmin/XenModel/Actions/Host/SavePowerOnSettingsAction.cs
 */
class SavePowerOnSettingsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit SavePowerOnSettingsAction(XenConnection* connection,
                                           const QList<QPair<QString, PowerOnMode>>& hostModes,
                                           QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        void saveHostConfig(const QString& hostRef, const PowerOnMode& mode);
        QString createSecret(const QString& value);
        void destroySecret(const QString& secretRef);

        QList<QPair<QString, PowerOnMode>> m_hostModes;
};

#endif // SAVEPOWERONSETTINGSACTION_H
