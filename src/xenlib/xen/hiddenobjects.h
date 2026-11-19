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

#ifndef HIDDENOBJECTS_H
#define HIDDENOBJECTS_H

#include "../xenlib_global.h"
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>

/**
 * @brief Objects the client keeps out of the tree for a while
 *
 * C# XenAdminConfigManager.Provider.HideObject / ShowObject / ObjectIsHidden
 * (WinformsXenAdminConfigProvider.HiddenObjects). Actions use it to keep an object out of
 * the navigation tree and search results while it is half-made, for example a VM between
 * being cloned and getting its final name. Unlike other_config:HideFromXenCenter this is
 * local to this client and never stored on the server.
 *
 * Safe to call from action worker threads; Changed() is emitted from the calling thread.
 */
class XENLIB_EXPORT HiddenObjects : public QObject
{
    Q_OBJECT

    public:
        static HiddenObjects* instance();

        static void Hide(const QString& opaqueRef);
        static void Show(const QString& opaqueRef);
        static bool IsHidden(const QString& opaqueRef);

    signals:
        //! The set changed; C# calls MainWindow.RequestRefreshTreeView here
        void Changed();

    private:
        explicit HiddenObjects(QObject* parent = nullptr);

        mutable QMutex m_mutex;
        QSet<QString> m_refs;
};

#endif // HIDDENOBJECTS_H
