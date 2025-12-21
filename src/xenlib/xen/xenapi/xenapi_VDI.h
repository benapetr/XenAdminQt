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

#ifndef XENAPI_VDI_H
#define XENAPI_VDI_H

#include <QString>
#include <QVariantMap>
#include <QVariantList>

class XenSession;

namespace XenAPI
{

    /**
     * @brief Static XenAPI VDI (Virtual Disk Image) bindings
     *
     * Qt port of C# XenAPI.VDI class. All methods are static and match
     * the C# signatures exactly for easy porting.
     */
    class VDI
    {
        private:
            VDI() = delete; // Static-only class

        public:
            // VDI lifecycle operations
            static QString create(XenSession* session, const QVariantMap& vdiRecord);
            static QString async_destroy(XenSession* session, const QString& vdi);
            static void destroy(XenSession* session, const QString& vdi);
            static QString async_copy(XenSession* session, const QString& vdi, const QString& sr);
            static QString async_pool_migrate(XenSession* session, const QString& vdi, const QString& sr, const QVariantMap& options);

            // VDI query operations
            static QVariantList get_VBDs(XenSession* session, const QString& vdi);
            static QString get_SR(XenSession* session, const QString& vdi);
            static QString get_name_label(XenSession* session, const QString& vdi);
            static QString get_name_description(XenSession* session, const QString& vdi);
            static qint64 get_virtual_size(XenSession* session, const QString& vdi);
            static bool get_read_only(XenSession* session, const QString& vdi);
            static QString get_type(XenSession* session, const QString& vdi);
            static bool get_sharable(XenSession* session, const QString& vdi);

            // VDI modification operations
            static void set_name_label(XenSession* session, const QString& vdi, const QString& label);
            static void set_name_description(XenSession* session, const QString& vdi, const QString& description);
            static void resize(XenSession* session, const QString& vdi, qint64 size);
            static void resize_online(XenSession* session, const QString& vdi, qint64 size);

            // Changed Block Tracking (CBT) operations
            static QString async_disable_cbt(XenSession* session, const QString& vdi);
            static bool get_cbt_enabled(XenSession* session, const QString& vdi);

            // Bulk query operations
            static QVariantMap get_record(XenSession* session, const QString& vdi);
            static QVariantList get_all(XenSession* session);
            static QVariantMap get_all_records(XenSession* session);
    };

} // namespace XenAPI

#endif // XENAPI_VDI_H
