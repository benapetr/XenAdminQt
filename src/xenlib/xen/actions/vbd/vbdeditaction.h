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

#ifndef VBDEDITACTION_H
#define VBDEDITACTION_H

#include "../../asyncoperation.h"
#include <QString>

class VBD;
class VM;

/**
 * @brief Action to edit VBD properties (mode, priority, device position)
 *
 * Qt port of C# VbdEditAction. Edits VBD settings including:
 * - VBD mode (RO/RW)
 * - IO priority (QoS)
 * - Device position (userdevice)
 *
 * Can optionally swap device positions with another VBD if both are
 * attached and the VM is running.
 *
 * C# equivalent: XenAdmin.Actions.VbdEditAction
 */
class XENLIB_EXPORT VbdEditAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a VBD edit action
         *
         * @param vbdRef VBD opaque reference
         * @param vbdMode New VBD mode ("RO" or "RW")
         * @param priority New IO priority (0-7, where 7 is highest)
         * @param changeDevicePosition Whether to change device position
         * @param otherVbdRef VBD to swap positions with (empty if no swap)
         * @param devicePosition New device position (userdevice)
         * @param parent Parent object
         *
         * C# equivalent: VbdEditAction(VBD, vbd_mode, int, bool, VBD, string, bool)
         */
        VbdEditAction(const QString& vbdRef,
                      const QString& vbdMode,
                      int priority,
                      bool changeDevicePosition,
                      const QString& otherVbdRef,
                      const QString& devicePosition,
                      QObject* parent = nullptr);

        ~VbdEditAction() override = default;

    protected:
        void run() override;

    private:
        /**
         * @brief Set userdevice and optionally plug/unplug VBD
         *
         * Mimics C# SetUserDevice() helper method. Unplugs VBD if running,
         * sets userdevice, then re-plugs if requested and allowed.
         *
         * @param vmRef VM opaque reference
         * @param vbdRef VBD opaque reference
         * @param userdevice New userdevice value
         * @param plug Whether to plug after setting userdevice
         */
        void setUserDevice(const QString& vmRef, const QString& vbdRef, const QString& userdevice, bool plug);

        QString vbdRef_;              //!< VBD opaque reference
        QString vbdMode_;             //!< New VBD mode ("RO" or "RW")
        int priority_;                //!< New IO priority (0-7)
        bool changeDevicePosition_;   //!< Whether to change device position
        QString otherVbdRef_;         //!< VBD to swap positions with (empty if no swap)
        QString devicePosition_;      //!< New device position (userdevice)
};

#endif // VBDEDITACTION_H
