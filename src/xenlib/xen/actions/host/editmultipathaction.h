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

#ifndef EDITMULTIPATHACTION_H
#define EDITMULTIPATHACTION_H

#include "../../asyncoperation.h"
#include <QSharedPointer>

class Host;

/**
 * @brief EditMultipathAction - Enable/disable multipath on a host
 *
 * Qt equivalent of C# XenModel.Actions.EditMultipathAction
 * (xenadmin/XenModel/Actions/Host/EditMultipathAction.cs)
 *
 * Changes the multipath setting on a host. This involves:
 * 1. Unplugging all currently attached PBDs
 * 2. Setting host.multipathing (XenServer 6.0+) or host.other_config["multipathing"]
 * 3. Setting host.other_config["multipath-handle"] to "dmp" if enabling
 * 4. Re-plugging all PBDs
 *
 * The host should be in maintenance mode before running this action to ensure
 * VMs are migrated before storage changes.
 */
class XENLIB_EXPORT EditMultipathAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Enable or disable multipath on a host
         * @param host Host object to configure
         * @param enableMultipath true to enable, false to disable
         * @param parent Parent object
         */
        explicit EditMultipathAction(QSharedPointer<Host> host, bool enableMultipath, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<Host> m_host;
        bool m_enableMultipath;
        
        static const char* DEFAULT_MULTIPATH_HANDLE;
};

#endif // EDITMULTIPATHACTION_H
