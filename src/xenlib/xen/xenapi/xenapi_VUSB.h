/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef XENAPI_VUSB_H
#define XENAPI_VUSB_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "xenlib_global.h"

namespace XenAPI
{
    class Session;

    //! Static-only class providing XenServer VUSB API calls (C# XenAPI.VUSB)
    class XENLIB_EXPORT VUSB
    {
        public:
            VUSB() = delete;

            //! VUSB.create: attaches a USB group to a VM; returns the new VUSB ref
            static QString create(Session* session, const QString& vm, const QString& usbGroup, const QVariantMap& otherConfig);

            //! Async.VUSB.unplug; returns the task ref
            static QString async_unplug(Session* session, const QString& vusb);

            //! Async.VUSB.destroy; returns the task ref
            static QString async_destroy(Session* session, const QString& vusb);

            static QStringList get_allowed_operations(Session* session, const QString& vusb);
    };
}

#endif // XENAPI_VUSB_H
