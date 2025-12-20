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

#ifndef FRIENDLYERRORNAMES_H
#define FRIENDLYERRORNAMES_H

#include "../xenlib_global.h"
#include <QString>
#include <QMap>

/*!
 * \brief XenAPI Friendly Error Names
 *
 * Provides friendly error messages for XenAPI error codes.
 * Matches xenadmin/XenModel/XenAPI/FriendlyErrorNames.Designer.cs pattern.
 *
 * Error messages are extracted from xenadmin/XenModel/XenAPI/FriendlyErrorNames.resx
 * and compiled into a static lookup table.
 */
class XENLIB_EXPORT FriendlyErrorNames
{
    public:
        /*!
        * \brief Get friendly error message for error code
        * \param errorCode XenAPI error code (e.g., "NO_HOSTS_AVAILABLE")
        * \return Friendly error message, or empty string if not found
        */
        static QString getString(const QString& errorCode);

    private:
        // Private constructor - this is a static-only class
        FriendlyErrorNames() = delete;

        // Initialize the error code lookup table
        static QMap<QString, QString> initializeErrorMap();

        // Static error code lookup table
        static const QMap<QString, QString>& getErrorMap();
};

#endif // FRIENDLYERRORNAMES_H
