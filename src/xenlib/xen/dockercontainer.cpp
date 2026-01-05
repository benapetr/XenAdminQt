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

DockerContainer::DockerContainer(QObject* parent)
    : XenObject(nullptr, QString(), parent)
    , parent_(nullptr)
{
}

DockerContainer::DockerContainer(VM* parent, const QString& uuid, const QString& name,
                               const QString& description, const QString& status,
                               const QString& container, const QString& created,
                               const QString& image, const QString& command,
                               const QString& ports, QObject* qparent)
    : XenObject(parent ? parent->GetConnection() : nullptr, 
                parent ? parent->OpaqueRef() + uuid : QString(), 
                qparent)
    , parent_(parent)
    , uuid_(uuid)
    , nameLabel_(name)
    , nameDescription_(description)
    , status_(status)
    , container_(container)
    , created_(created)
    , image_(image)
    , command_(command)
    , ports_(ports)
{
}

int DockerContainer::powerState() const
{
    // C# code: status.Contains("Paused") ? vm_power_state.Paused : status.StartsWith("Up") ? vm_power_state.Running : vm_power_state.Halted;
    if (this->status_.contains("Paused"))
        return 1; // vm_power_state::Paused
    else if (this->status_.startsWith("Up"))
        return 2; // vm_power_state::Running
    else
        return 0; // vm_power_state::Halted
}

QList<DockerContainer::DockerContainerPort> DockerContainer::portList() const
{
    QList<DockerContainerPort> portList;
    
    if (this->ports_.isEmpty())
        return portList;
    
    // Parse XML: wrap the ports into a root node
    QString xml = "<items>" + this->ports_ + "</items>";
    QXmlStreamReader reader(xml);
    
    DockerContainerPort currentPort;
    bool inItem = false;
    
    while (!reader.atEnd())
    {
        reader.readNext();
        
        if (reader.isStartElement())
        {
            if (reader.name() == "item")
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
        else if (reader.isEndElement() && reader.name() == "item")
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

void DockerContainer::updateFrom(const DockerContainer& update)
{
    this->parent_ = update.parent_;
    // Note: connection and opaqueRef are immutable after construction
    // They are set in XenObject constructor and shouldn't be changed
    
    this->setUuid(update.uuid_);
    this->setNameLabel(update.nameLabel_);
    this->setNameDescription(update.nameDescription_);
    this->setStatus(update.status_);
    this->setContainer(update.container_);
    this->setCreated(update.created_);
    this->setImage(update.image_);
    this->setCommand(update.command_);
    this->setPorts(update.ports_);
}

void DockerContainer::setUuid(const QString& value)
{
    if (this->uuid_ != value)
    {
        this->uuid_ = value;
        emit this->propertyChanged("uuid");
    }
}

void DockerContainer::setNameLabel(const QString& value)
{
    if (this->nameLabel_ != value)
    {
        this->nameLabel_ = value;
        emit this->propertyChanged("name_label");
    }
}

void DockerContainer::setNameDescription(const QString& value)
{
    if (this->nameDescription_ != value)
    {
        this->nameDescription_ = value;
        emit this->propertyChanged("name_description");
    }
}

void DockerContainer::setStatus(const QString& value)
{
    if (this->status_ != value)
    {
        this->status_ = value;
        emit this->propertyChanged("status");
    }
}

void DockerContainer::setContainer(const QString& value)
{
    if (this->container_ != value)
    {
        this->container_ = value;
        emit this->propertyChanged("container");
    }
}

void DockerContainer::setCreated(const QString& value)
{
    if (this->created_ != value)
    {
        this->created_ = value;
        emit this->propertyChanged("created");
    }
}

void DockerContainer::setImage(const QString& value)
{
    if (this->image_ != value)
    {
        this->image_ = value;
        emit this->propertyChanged("image");
    }
}

void DockerContainer::setCommand(const QString& value)
{
    if (this->command_ != value)
    {
        this->command_ = value;
        emit this->propertyChanged("command");
    }
}

void DockerContainer::setPorts(const QString& value)
{
    if (this->ports_ != value)
    {
        this->ports_ = value;
        emit this->propertyChanged("ports");
    }
}
