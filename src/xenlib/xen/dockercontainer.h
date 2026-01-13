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

#pragma once

#include "xenobject.h"
#include <QString>
#include <QList>

class VM;

/**
 * @brief DockerContainer - A Docker container running in a VM
 *
 * Qt equivalent of C# XenAdmin.Model.DockerContainer class.
 * Represents a Docker container with its configuration and runtime state.
 *
 * Note: Docker containers don't have opaque_ref at server side.
 * We use parent.opaque_ref + uuid as a unique identifier per connection.
 */
class XENLIB_EXPORT DockerContainer : public XenObject
{
    Q_OBJECT
    
    public:
        struct DockerContainerPort
        {
            QString address;
            QString publicPort;
            QString privatePort;
            QString protocol;

            QString description() const;
        };

        explicit DockerContainer(XenConnection* connection,
                                const QString& opaqueRef,
                                QObject* parent = nullptr);

        // XenObject overrides
        QString GetObjectType() const override;

        //! @brief Get parent VM reference
        //! @return VM opaque reference that hosts this container
        QString ParentRef() const;

        //! @brief Get parent VM object
        //! @return Shared pointer to parent VM
        QSharedPointer<VM> GetParent() const;

        //! @brief Get container status
        //! @return Status string (e.g., "Up", "Paused", "Exited")
        QString Status() const;

        //! @brief Get container ID
        //! @return Container ID
        QString Container() const;

        //! @brief Get creation timestamp
        //! @return Creation time string
        QString Created() const;

        //! @brief Get image name
        //! @return Docker image name
        QString Image() const;

        //! @brief Get command
        //! @return Command that container is running
        QString Command() const;

        //! @brief Get ports XML
        //! @return XML string containing port mappings
        QString Ports() const;

        //! @brief Get power state based on status
        //! @return 0=Halted, 1=Paused, 2=Running
        int PowerState() const;

        //! @brief Parse port mappings from XML
        //! @return List of parsed port configurations
        QList<DockerContainerPort> PortList() const;
};
