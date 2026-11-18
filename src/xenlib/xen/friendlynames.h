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

#ifndef FRIENDLYNAMES_H
#define FRIENDLYNAMES_H

#include "../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <QMap>

/*!
 * \brief Friendly names for XenAPI objects, properties and messages
 *
 * C# equivalent: FriendlyNameManager.GetFriendlyName() backed by
 * xenadmin/XenModel/FriendlyNames.resx. Keys are the resx names, e.g.
 * "Message.name-ha_host_failed" or "Label-host.name_label".
 *
 * The entries in friendlynames.cpp are generated from the resx file.
 * Strings keep the C# {0}, {1}... placeholders; use Format() to fill them.
 */
class XENLIB_EXPORT FriendlyNames
{
    public:
        /*!
         * \brief Get the friendly string for a resource key
         * \return Friendly string, or empty string if not found
         */
        static QString getString(const QString& key);

        /*!
         * \brief Replace C#-style {0}, {1}... placeholders with the given arguments
         */
        static QString Format(const QString& format, const QStringList& args);

    private:
        FriendlyNames() = delete;

        static QMap<QString, QString> initializeMap();
        static const QMap<QString, QString>& getMap();
};

#endif // FRIENDLYNAMES_H
