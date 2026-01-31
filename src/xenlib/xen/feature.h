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

#ifndef FEATURE_H
#define FEATURE_H

#include "xenobject.h"

/**
 * @brief Feature - A new piece of functionality
 *
 * Qt equivalent of C# XenAPI.Feature class. Represents a feature/capability.
 * First published in XenServer 7.2.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - name_label: Human-readable name
 * - name_description: Human-readable description
 * - enabled: Whether the feature is enabled
 * - experimental: Whether the feature is experimental
 * - version: Feature version string
 * - host: Reference to the host this feature belongs to
 */
class XENLIB_EXPORT Feature : public XenObject
{
    Q_OBJECT

    public:
        explicit Feature(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Feature() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Feature; }

        // Property accessors (read from cache)
        bool IsEnabled() const;
        bool IsExperimental() const;
        QString Version() const;
        QString HostRef() const;
};

#endif // FEATURE_H
