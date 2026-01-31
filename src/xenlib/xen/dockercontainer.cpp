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

#include "dockercontainer.h"
#include "vm.h"
#include "network/connection.h"
#include "../xencache.h"
#include <QXmlStreamReader>

QString DockerContainer::DockerContainerPort::description() const
{
    QStringList list;
    
    if (!this->address.isEmpty())
        list.append(QObject::tr("Address: %1").arg(this->address));
    if (!this->publicPort.isEmpty())
        list.append(QObject::tr("Public Port: %1").arg(this->publicPort));
    if (!this->privatePort.isEmpty())
        list.append(QObject::tr("Private Port: %1").arg(this->privatePort));
    if (!this->protocol.isEmpty())
        list.append(QObject::tr("Protocol: %1").arg(this->protocol));
    
    return list.join("; ");
}

DockerContainer::DockerContainer(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString DockerContainer::ParentRef() const
{
    return this->stringProperty("parent");
}

QSharedPointer<VM> DockerContainer::GetParent() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VM>();

    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<VM>();

    QString parentRef = this->ParentRef();
    if (parentRef.isEmpty())
        return QSharedPointer<VM>();

    return cache->ResolveObject<VM>(XenObjectType::VM, parentRef);
}

QString DockerContainer::Status() const
{
    return this->stringProperty("status");
}

QString DockerContainer::Container() const
{
    return this->stringProperty("container");
}

QString DockerContainer::Created() const
{
    return this->stringProperty("created");
}

QString DockerContainer::Image() const
{
    return this->stringProperty("image");
}

QString DockerContainer::Command() const
{
    return this->stringProperty("command");
}

QString DockerContainer::Ports() const
{
    return this->stringProperty("ports");
}

int DockerContainer::PowerState() const
{
    // C# code: status.Contains("Paused") ? vm_power_state.Paused : status.StartsWith("Up") ? vm_power_state.Running : vm_power_state.Halted;
    QString status = this->Status();
    if (status.contains("Paused"))
        return 1; // vm_power_state::Paused
    else if (status.startsWith("Up"))
        return 2; // vm_power_state::Running
    else
        return 0; // vm_power_state::Halted
}

QList<DockerContainer::DockerContainerPort> DockerContainer::PortList() const
{
    QList<DockerContainerPort> portList;
    
    QString portsXml = this->Ports();
    if (portsXml.isEmpty())
        return portList;
    
    // Parse XML: wrap the ports into a root node
    QString xml = "<items>" + portsXml + "</items>";
    QXmlStreamReader reader(xml);
    
    DockerContainerPort currentPort;
    bool inItem = false;
    
    while (!reader.atEnd())
    {
        reader.readNext();
        
        if (reader.isStartElement())
        {
            if (reader.name() == QLatin1String("item"))
            {
                inItem = true;
                currentPort = DockerContainerPort(); // Reset
            }
            else if (inItem)
            {
                QString elementName = reader.name().toString();
                QString text = reader.readElementText();
                
                if (elementName == "IP")
                    currentPort.address = text;
                else if (elementName == "PublicPort")
                    currentPort.publicPort = text;
                else if (elementName == "PrivatePort")
                    currentPort.privatePort = text;
                else if (elementName == "Type")
                    currentPort.protocol = text;
            }
        }
        else if (reader.isEndElement() && reader.name() == QString("item"))
        {
            inItem = false;
            portList.append(currentPort);
        }
    }
    
    if (reader.hasError())
    {
        // Ignore XML parsing errors
        portList.clear();
    }
    
    return portList;
}
