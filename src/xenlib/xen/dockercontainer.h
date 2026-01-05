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
    
    DockerContainer(QObject* parent = nullptr);
    DockerContainer(VM* parent, const QString& uuid, const QString& name, 
                   const QString& description, const QString& status,
                   const QString& container, const QString& created,
                   const QString& image, const QString& command,
                   const QString& ports, QObject* qparent = nullptr);
    
    // Property getters
    QString uuid() const { return this->uuid_; }
    QString nameLabel() const { return this->nameLabel_; }
    QString nameDescription() const { return this->nameDescription_; }
    QString status() const { return this->status_; }
    QString container() const { return this->container_; }
    QString created() const { return this->created_; }
    QString image() const { return this->image_; }
    QString command() const { return this->command_; }
    QString ports() const { return this->ports_; }
    
    VM* parent() const { return this->parent_; }
    int powerState() const;
    QList<DockerContainerPort> portList() const;
    
    // XenObject overrides
    QString GetObjectType() const override { return "dockercontainer"; }
    
    void updateFrom(const DockerContainer& update);
    
    // Property setters
    void setUuid(const QString& value);
    void setNameLabel(const QString& value);
    void setNameDescription(const QString& value);
    void setStatus(const QString& value);
    void setContainer(const QString& value);
    void setCreated(const QString& value);
    void setImage(const QString& value);
    void setCommand(const QString& value);
    void setPorts(const QString& value);
    
signals:
    void propertyChanged(const QString& propertyName);
    
private:
    VM* parent_;
    QString uuid_;
    QString nameLabel_;
    QString nameDescription_;
    QString status_;
    QString container_;
    QString created_;
    QString image_;
    QString command_;
    QString ports_;
};
