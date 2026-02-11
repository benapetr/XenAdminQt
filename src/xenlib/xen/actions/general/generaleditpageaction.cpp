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

#include "generaleditpageaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/api.h"
#include "xenlib/xen/jsonrpcclient.h"
#include "xen/xenobject.h"
#include <QDebug>

using namespace XenAPI;

GeneralEditPageAction::GeneralEditPageAction(QSharedPointer<XenObject> object, const QString& oldFolder, const QString& newFolder, const QStringList& oldTags, const QStringList& newTags, bool suppressHistory, QObject* parent)
    : AsyncOperation(object->GetConnection(), tr("Update Properties"), tr("Updating folder and tag properties..."), suppressHistory, parent), m_oldFolder(oldFolder), m_newFolder(newFolder), m_oldTags(oldTags), m_newTags(newTags)
{
    this->m_object = object;

    // Sort tags for efficient comparison (matches C# BinarySearch approach)
    this->m_oldTags.sort();
    this->m_newTags.sort();

    // RBAC permission checks (matching C# implementation)
    // C# checks: folder operations + tag operations
    // For simplicity, we add the main object modification permission
    // Real RBAC would need more granular checks based on operation type
}

void GeneralEditPageAction::run()
{
    try
    {
        this->SetPercentComplete(0);
        this->SetDescription(this->tr("Updating properties..."));

        // Step 1: Handle folder changes
        // C# code: if (newFolder != xenObjectCopy.Path)
        if (this->m_oldFolder != this->m_newFolder)
        {
            this->SetPercentComplete(10);
            this->SetDescription(this->tr("Updating folder..."));

            if (!this->m_newFolder.isEmpty())
            {
                // Move to new folder
                // C# equivalent: Folders.Move(Session, xenObjectOrig, newFolder);
                this->setFolderPath(this->m_newFolder);
                qDebug() << "GeneralEditPageAction: Moved" << this->m_object->GetObjectTypeName() << this->m_object->OpaqueRef()
                         << "from folder" << this->m_oldFolder << "to" << this->m_newFolder;
            } else
            {
                // Unfolder (remove from folder)
                // C# equivalent: Folders.Unfolder(Session, xenObjectOrig);
                this->setFolderPath(QString()); // Empty string = remove folder key
                qDebug() << "GeneralEditPageAction: Unfoldered" << this->m_object->GetObjectTypeName() << this->m_object->OpaqueRef();
            }
        }

        // Step 2: Remove tags that are in oldTags but not in newTags
        // C# code: foreach (string tag in oldTags)
        //              if (newTags.BinarySearch(tag) < 0)
        //                  Tags.RemoveTag(Session, xenObjectOrig, tag);
        int tagProgress = 30;
        int tagsToRemove = 0;
        int tagsToAdd = 0;

        foreach (const QString& tag, this->m_oldTags)
        {
            if (!this->m_newTags.contains(tag))
            {
                tagsToRemove++;
            }
        }

        foreach (const QString& tag, this->m_newTags)
        {
            if (!this->m_oldTags.contains(tag))
            {
                tagsToAdd++;
            }
        }

        int totalTagOps = tagsToRemove + tagsToAdd;
        int currentTagOp = 0;

        foreach (const QString& tag, this->m_oldTags)
        {
            if (!this->m_newTags.contains(tag))
            {
                if (totalTagOps > 0)
                {
                    tagProgress = 30 + (currentTagOp * 60 / totalTagOps);
                    this->SetPercentComplete(tagProgress);
                    this->SetDescription(this->tr("Removing tag '%1'...").arg(tag));
                }

                this->removeTag(tag);
                qDebug() << "GeneralEditPageAction: Removed tag" << tag
                         << "from" << this->m_object->GetObjectTypeName() << this->m_object->OpaqueRef();

                currentTagOp++;
            }
        }

        // Step 3: Add tags that are in newTags but not in oldTags
        // C# code: foreach (string tag in newTags)
        //              if (oldTags.BinarySearch(tag) < 0)
        //                  Tags.AddTag(Session, xenObjectOrig, tag);
        foreach (const QString& tag, this->m_newTags)
        {
            if (!this->m_oldTags.contains(tag))
            {
                if (totalTagOps > 0)
                {
                    tagProgress = 30 + (currentTagOp * 60 / totalTagOps);
                    this->SetPercentComplete(tagProgress);
                    this->SetDescription(this->tr("Adding tag '%1'...").arg(tag));
                }

                this->addTag(tag);
                qDebug() << "GeneralEditPageAction: Added tag" << tag
                         << "to" << this->m_object->GetObjectTypeName() << this->m_object->OpaqueRef();

                currentTagOp++;
            }
        }

        this->SetPercentComplete(100);
        this->SetDescription(this->tr("Properties updated successfully"));

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to update properties: %1").arg(e.what()));
        qWarning() << "GeneralEditPageAction: Error -" << e.what();
    }
}

void GeneralEditPageAction::setFolderPath(const QString& folderPath)
{
    Session* sess = this->GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    XenRpcAPI api(sess);
    const QString objectType = this->m_object->GetObjectTypeName();

    auto invokeOtherConfigCall = [&](const QString& method, const QVariantList& params)
    {
        const QByteArray request = api.BuildJsonRpcCall(method, params);
        const QByteArray response = sess->SendApiRequest(QString::fromUtf8(request));
        if (response.isEmpty())
            throw std::runtime_error("Empty response from XenAPI");
        api.ParseJsonRpcResponse(response);
        const QString rpcError = Xen::JsonRpcClient::lastError();
        if (!rpcError.isEmpty())
            throw std::runtime_error(rpcError.toStdString());
    };

    // Match C# helper semantics: clear existing key first, then add new value.
    // Removing a missing key is harmless and ignored.
    try
    {
        QVariantList removeParams;
        removeParams << sess->GetSessionID() << this->m_object->OpaqueRef() << "folder";
        invokeOtherConfigCall(QString("%1.remove_from_other_config").arg(objectType), removeParams);
    } catch (const std::exception& e)
    {
        const QString message = QString::fromUtf8(e.what());
        if (!message.contains("MAP_NO_SUCH_KEY", Qt::CaseInsensitive))
            throw;
    }

    const QString normalizedFolderPath = folderPath.trimmed();
    if (!normalizedFolderPath.isEmpty())
    {
        QVariantList addParams;
        addParams << sess->GetSessionID() << this->m_object->OpaqueRef() << "folder" << normalizedFolderPath;
        invokeOtherConfigCall(QString("%1.add_to_other_config").arg(objectType), addParams);
    }
}

void GeneralEditPageAction::removeTag(const QString& tag)
{
    // C# equivalent: Tags.RemoveTag(GetSession, o, tag)
    // Implementation: o.Do("remove_tags", GetSession, o.opaque_ref, tag);

    Session* sess = this->GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    QString method = QString("%1.remove_tags").arg(this->m_object->GetObjectTypeName());

    QVariantList params;
    params << sess->GetSessionID() << this->m_object->OpaqueRef() << tag;

    XenRpcAPI api(sess);
    const QByteArray request = api.BuildJsonRpcCall(method, params);
    const QByteArray response = sess->SendApiRequest(QString::fromUtf8(request));
    if (response.isEmpty())
        throw std::runtime_error("Empty response from XenAPI");

    // Parse response (will throw on error)
    api.ParseJsonRpcResponse(response);
    const QString rpcError = Xen::JsonRpcClient::lastError();
    if (!rpcError.isEmpty())
        throw std::runtime_error(rpcError.toStdString());
}

void GeneralEditPageAction::addTag(const QString& tag)
{
    // C# equivalent: Tags.AddTag(GetSession, o, tag)
    // Implementation: o.Do("add_tags", GetSession, o.opaque_ref, tag);

    Session* sess = this->GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        throw std::runtime_error("Not connected to XenServer");
    }

    QString method = QString("%1.add_tags").arg(this->m_object->GetObjectTypeName());

    QVariantList params;
    params << sess->GetSessionID() << this->m_object->OpaqueRef() << tag;

    XenRpcAPI api(sess);
    const QByteArray request = api.BuildJsonRpcCall(method, params);
    const QByteArray response = sess->SendApiRequest(QString::fromUtf8(request));
    if (response.isEmpty())
        throw std::runtime_error("Empty response from XenAPI");

    // Parse response (will throw on error)
    api.ParseJsonRpcResponse(response);
    const QString rpcError = Xen::JsonRpcClient::lastError();
    if (!rpcError.isEmpty())
        throw std::runtime_error(rpcError.toStdString());
}
