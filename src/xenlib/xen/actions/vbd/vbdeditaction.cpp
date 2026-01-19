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

#include "vbdeditaction.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../session.h"
#include <QVariantMap>

VbdEditAction::VbdEditAction(const QString& vbdRef,
                             const QString& vbdMode,
                             int priority,
                             bool changeDevicePosition,
                             const QString& otherVbdRef,
                             const QString& devicePosition,
                             QObject* parent)
    : AsyncOperation(tr("Edit VBD"),
                     tr("Editing virtual block device settings..."),
                     parent)
    , vbdRef_(vbdRef)
    , vbdMode_(vbdMode)
    , priority_(priority)
    , changeDevicePosition_(changeDevicePosition)
    , otherVbdRef_(otherVbdRef)
    , devicePosition_(devicePosition)
{
    // Register API methods for RBAC checks
    AddApiMethodToRoleCheck("VBD.set_mode");
    AddApiMethodToRoleCheck("VBD.set_qos_algorithm_params");
    AddApiMethodToRoleCheck("VBD.set_userdevice");
    AddApiMethodToRoleCheck("VBD.plug");
    AddApiMethodToRoleCheck("VBD.unplug");
}

void VbdEditAction::run()
{
    XenAPI::Session* session = this->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        this->setError("Not connected to XenServer");
        return;
    }

    try
    {
        // Get VBD current settings via XenAPI
        QString currentMode = XenAPI::VBD::get_mode(session, this->vbdRef_);
        QVariantMap qosParams = XenAPI::VBD::get_qos_algorithm_params(session, this->vbdRef_);
        
        // Step 1: Set VBD mode if changed
        if (this->vbdMode_ != currentMode)
        {
            XenAPI::VBD::set_mode(session, this->vbdRef_, this->vbdMode_);
        }

        // Step 2: Set IO priority if changed
        int currentPriority = 0;
        if (qosParams.contains("class"))
        {
            currentPriority = qosParams["class"].toInt();
        }
        
        if (this->priority_ != currentPriority)
        {
            // Prepare QoS params
            QVariantMap newQosParams;
            newQosParams["class"] = QString::number(this->priority_);
            newQosParams["sched"] = "be";  // best-effort scheduling

            // Set algorithm type and params
            XenAPI::VBD::set_qos_algorithm_type(session, this->vbdRef_, "ionice");
            XenAPI::VBD::set_qos_algorithm_params(session, this->vbdRef_, newQosParams);
        }

        // Step 3: Change device position if requested
        if (this->changeDevicePosition_)
        {
            QString vmRef = XenAPI::VBD::get_VM(session, this->vbdRef_);
            
            if (!this->otherVbdRef_.isEmpty())
            {
                // We're swapping device positions with another VBD
                QString vbdOldUserdevice = XenAPI::VBD::get_userdevice(session, this->vbdRef_);

                // Set other VBD to our old position (unplug first)
                this->setUserDevice(vmRef, this->otherVbdRef_, vbdOldUserdevice, false);

                // Set our VBD to the new position
                this->setUserDevice(vmRef, this->vbdRef_, this->devicePosition_, true);

                // Re-plug the other VBD if VM is running and plug is allowed
                QString powerState = XenAPI::VM::get_power_state(session, vmRef);
                QVariantList allowedOps = XenAPI::VBD::get_allowed_operations(session, this->otherVbdRef_);
                if (powerState == "Running" && allowedOps.contains("plug"))
                {
                    XenAPI::VBD::plug(session, this->otherVbdRef_);
                }
            } else
            {
                // Simple device position change (no swap)
                this->setUserDevice(vmRef, this->vbdRef_, this->devicePosition_, false);
            }
        }

        this->SetPercentComplete(100);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to edit VBD: %1").arg(e.what()));
    }
}

void VbdEditAction::setUserDevice(const QString& vmRef, const QString& vbdRef, const QString& userdevice, bool plug)
{
    XenAPI::Session* session = this->GetSession();
    QString powerState = XenAPI::VM::get_power_state(session, vmRef);
    bool currentlyAttached = XenAPI::VBD::get_currently_attached(session, vbdRef);
    QVariantList allowedOps = XenAPI::VBD::get_allowed_operations(session, vbdRef);

    // Unplug VBD if running and currently attached
    if (powerState == "Running" && currentlyAttached && allowedOps.contains("unplug"))
    {
        XenAPI::VBD::unplug(session, vbdRef);
    }

    // Set the new userdevice
    XenAPI::VBD::set_userdevice(session, vbdRef, userdevice);

    // Re-plug if requested and allowed
    if (plug && allowedOps.contains("plug") && powerState == "Running")
    {
        XenAPI::VBD::plug(session, vbdRef);
    }
}
