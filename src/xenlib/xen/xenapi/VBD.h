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

#ifndef XENAPI_VBD_H
#define XENAPI_VBD_H

#include <QString>
#include <QVariantMap>
#include <QVariantList>

class XenSession;

namespace XenAPI
{

    /**
     * @brief Static XenAPI VBD (Virtual Block Device) bindings
     *
     * Qt port of C# XenAPI.VBD class. All methods are static and match
     * the C# signatures exactly for easy porting.
     */
    class VBD
    {
    private:
        VBD() = delete; // Static-only class

    public:
        // VBD lifecycle operations
        static QString create(XenSession* session, const QVariantMap& vbdRecord);
        static QString async_plug(XenSession* session, const QString& vbd);
        static QString async_unplug(XenSession* session, const QString& vbd);
        static QString async_destroy(XenSession* session, const QString& vbd);
        static void plug(XenSession* session, const QString& vbd);
        static void unplug(XenSession* session, const QString& vbd);
        static void destroy(XenSession* session, const QString& vbd);

        // VBD query operations
        static QVariantList get_allowed_operations(XenSession* session, const QString& vbd);
        static QString get_VM(XenSession* session, const QString& vbd);
        static QString get_VDI(XenSession* session, const QString& vbd);
        static QString get_device(XenSession* session, const QString& vbd);
        static QString get_userdevice(XenSession* session, const QString& vbd);
        static bool get_bootable(XenSession* session, const QString& vbd);
        static QString get_mode(XenSession* session, const QString& vbd);
        static QString get_type(XenSession* session, const QString& vbd);
        static bool get_unpluggable(XenSession* session, const QString& vbd);
        static bool get_currently_attached(XenSession* session, const QString& vbd);
        static bool get_empty(XenSession* session, const QString& vbd);

        // VBD modification operations
        static void set_bootable(XenSession* session, const QString& vbd, bool bootable);
        static void set_mode(XenSession* session, const QString& vbd, const QString& mode);
        static void set_userdevice(XenSession* session, const QString& vbd, const QString& userdevice);

        // Bulk query operations
        static QVariantMap get_record(XenSession* session, const QString& vbd);
        static QVariantList get_all(XenSession* session);
        static QVariantMap get_all_records(XenSession* session);
    };

} // namespace XenAPI

#endif // XENAPI_VBD_H
