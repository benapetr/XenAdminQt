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

#include <QSet>
#include "foldersmanager.h"
#include "../xencache.h"
#include "../xen/api.h"
#include "../xen/network/connection.h"
#include "../xen/network/connectionsmanager.h"
#include "../xen/pool.h"
#include "../xen/session.h"

FoldersManager* FoldersManager::s_instance = nullptr;
const QString FoldersManager::FOLDER_KEY = QStringLiteral("folder");
const QString FoldersManager::PATH_SEPARATOR = QStringLiteral("/");
const QString FoldersManager::EMPTY_FOLDERS_KEY = QStringLiteral("EMPTY_FOLDERS");
const QString FoldersManager::EMPTY_FOLDERS_SEPARATOR = QStringLiteral(";");

FoldersManager::FoldersManager(QObject* parent) : QObject(parent)
{
}

FoldersManager* FoldersManager::instance()
{
    if (!s_instance)
        s_instance = new FoldersManager();
    return s_instance;
}

void FoldersManager::RegisterEventHandlers()
{
    if (this->m_registered)
        return;

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    connect(manager, &Xen::ConnectionsManager::connectionAdded, this, &FoldersManager::onConnectionAdded);
    connect(manager, &Xen::ConnectionsManager::connectionRemoved, this, &FoldersManager::onConnectionRemoved);

    const QList<XenConnection*> connections = manager->GetAllConnections();
    for (XenConnection* connection : connections)
        this->onConnectionAdded(connection);

    this->m_registered = true;
}

void FoldersManager::DeregisterEventHandlers()
{
    if (!this->m_registered)
        return;

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    disconnect(manager, &Xen::ConnectionsManager::connectionAdded, this, &FoldersManager::onConnectionAdded);
    disconnect(manager, &Xen::ConnectionsManager::connectionRemoved, this, &FoldersManager::onConnectionRemoved);

    for (auto it = this->m_handlers.begin(); it != this->m_handlers.end(); ++it)
    {
        disconnect(it->xenObjectsUpdated);
        disconnect(it->stateChanged);
    }
    this->m_handlers.clear();
    this->m_rebuildInProgress.clear();
    this->m_registered = false;
}

QStringList FoldersManager::PointToPath(const QString& path)
{
    QString fixed = path.trimmed();
    if (fixed.isEmpty())
        return QStringList();

    while (fixed.size() > 0 && fixed.startsWith(PATH_SEPARATOR))
        fixed.remove(0, 1);

    return fixed.split(PATH_SEPARATOR, Qt::SkipEmptyParts);
}

QString FoldersManager::PathToPoint(const QStringList& path, int depth)
{
    if (depth <= 0)
        return PATH_SEPARATOR;
    const int maxDepth = qMin(depth, path.size());
    return PATH_SEPARATOR + path.mid(0, maxDepth).join(PATH_SEPARATOR);
}

QString FoldersManager::AppendPath(const QString& first, const QString& second)
{
    if (first.endsWith(PATH_SEPARATOR))
        return first + second;
    return first + PATH_SEPARATOR + second;
}

QString FoldersManager::GetParent(const QString& path)
{
    const QStringList points = PointToPath(path);
    if (points.isEmpty())
        return QString();
    return PathToPoint(points, points.size() - 1);
}

QString FoldersManager::FixupRelativePath(const QString& path)
{
    QString name = path;
    name = name.trimmed();
    name.replace('\t', ' ');
    int oldLength = -1;

    while (oldLength != name.length())
    {
        oldLength = name.length();
        name.replace("//", "/");
        name.replace("/ ", "/");
        name.replace(" /", "/");
    }

    while (name.startsWith('/'))
        name.remove(0, 1);

    while (name.endsWith('/'))
        name.chop(1);

    return name;
}

QString FoldersManager::FolderPathFromRecord(const QVariantMap& objectData)
{
    const QVariantMap otherConfig = objectData.value("other_config").toMap();
    return otherConfig.value(FOLDER_KEY).toString().trimmed();
}

QStringList FoldersManager::AncestorFolders(const QString& path)
{
    QStringList ancestors;
    const QStringList parts = PointToPath(path);
    for (int i = 1; i <= parts.size(); ++i)
        ancestors.append(PathToPoint(parts, i));
    return ancestors;
}

QStringList FoldersManager::Descendants(XenConnection* connection, const QString& path) const
{
    QStringList descendants;
    if (!connection || !connection->GetCache())
        return descendants;

    const QList<QVariantMap> folders = connection->GetCache()->GetAllData(XenObjectType::Folder);
    const QString prefix = path.endsWith(PATH_SEPARATOR) ? path : path + PATH_SEPARATOR;
    for (const QVariantMap& folder : folders)
    {
        const QString ref = folder.value("ref").toString();
        if (ref.startsWith(prefix))
            descendants.append(ref);
    }
    return descendants;
}

bool FoldersManager::HasSubfolders(XenConnection* connection, const QString& path) const
{
    return !this->Descendants(connection, path).isEmpty();
}

bool FoldersManager::ContainsResources(XenConnection* connection, const QString& path) const
{
    if (!connection || !connection->GetCache())
        return false;

    for (const QString& type : this->searchableTypes())
    {
        if (type == "folder")
            continue;

        const QList<QVariantMap> records = connection->GetCache()->GetAllData(type);
        for (const QVariantMap& record : records)
        {
            const QString folderPath = FolderPathFromRecord(record);
            if (folderPath == path || folderPath.startsWith(path + PATH_SEPARATOR))
                return true;
        }
    }
    return false;
}

bool FoldersManager::CreateFolder(XenConnection* connection, const QString& path)
{
    if (!connection || !connection->GetCache())
        return false;

    const QString fixed = PATH_SEPARATOR + FixupRelativePath(path);
    QSet<QString> existing;
    const QStringList refs = connection->GetCache()->GetAllRefs(XenObjectType::Folder);
    for (const QString& ref : refs)
        existing.insert(ref);
    this->ensureFolderChain(connection, fixed, existing);

    QStringList emptyFolders = this->getEmptyFolders(connection);
    if (!emptyFolders.contains(fixed))
    {
        emptyFolders.append(fixed);
        emptyFolders.sort();
        this->setEmptyFolders(connection, emptyFolders);
    }
    emit FoldersChanged(connection);
    return true;
}

bool FoldersManager::DeleteFolder(XenConnection* connection, const QString& path)
{
    if (!connection || !connection->GetCache())
        return false;

    const QString target = PATH_SEPARATOR + FixupRelativePath(path);
    const QStringList descendants = this->Descendants(connection, target);
    for (const QString& ref : descendants)
        connection->GetCache()->Remove(XenObjectType::Folder, ref);
    connection->GetCache()->Remove(XenObjectType::Folder, target);

    QStringList emptyFolders = this->getEmptyFolders(connection);
    emptyFolders.removeAll(target);
    this->setEmptyFolders(connection, emptyFolders);

    emit FoldersChanged(connection);
    return true;
}

bool FoldersManager::MoveObjectToFolder(XenConnection* connection, const QString& objectType, const QString& objectRef, const QString& folderPath)
{
    if (!connection || !connection->GetCache())
        return false;

    const XenObjectType type = XenCache::TypeFromString(objectType);
    if (type == XenObjectType::Null)
        return false;

    QVariantMap record = connection->GetCache()->ResolveObjectData(type, objectRef);
    if (record.isEmpty())
        return false;

    QVariantMap otherConfig = record.value("other_config").toMap();
    const QString fixed = PATH_SEPARATOR + FixupRelativePath(folderPath);
    otherConfig[FOLDER_KEY] = fixed;
    record["other_config"] = otherConfig;
    connection->GetCache()->Update(type, objectRef, record);
    this->CreateFolder(connection, fixed);
    return true;
}

bool FoldersManager::UnfolderObject(XenConnection* connection, const QString& objectType, const QString& objectRef)
{
    if (!connection || !connection->GetCache())
        return false;

    const XenObjectType type = XenCache::TypeFromString(objectType);
    if (type == XenObjectType::Null)
        return false;

    QVariantMap record = connection->GetCache()->ResolveObjectData(type, objectRef);
    if (record.isEmpty())
        return false;

    QVariantMap otherConfig = record.value("other_config").toMap();
    otherConfig.remove(FOLDER_KEY);
    record["other_config"] = otherConfig;
    connection->GetCache()->Update(type, objectRef, record);
    return true;
}

void FoldersManager::onConnectionAdded(XenConnection* connection)
{
    if (!connection || this->m_handlers.contains(connection))
        return;

    ConnectionHandlers handlers;
    handlers.xenObjectsUpdated = connect(connection, &XenConnection::XenObjectsUpdated, this, &FoldersManager::onConnectionObjectsUpdated);
    handlers.stateChanged = connect(connection, &XenConnection::ConnectionStateChanged, this, &FoldersManager::onConnectionObjectsUpdated);
    this->m_handlers.insert(connection, handlers);
    this->m_rebuildInProgress.insert(connection, false);
    this->rebuildConnectionFolders(connection);
}

void FoldersManager::onConnectionRemoved(XenConnection* connection)
{
    if (!connection || !this->m_handlers.contains(connection))
        return;

    const ConnectionHandlers handlers = this->m_handlers.take(connection);
    disconnect(handlers.xenObjectsUpdated);
    disconnect(handlers.stateChanged);
    this->m_rebuildInProgress.remove(connection);
}

void FoldersManager::onConnectionObjectsUpdated()
{
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (!connection)
        return;
    this->rebuildConnectionFolders(connection);
}

void FoldersManager::rebuildConnectionFolders(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return;

    if (this->m_rebuildInProgress.value(connection))
        return;

    this->m_rebuildInProgress[connection] = true;

    QSet<QString> existing;
    existing.insert(PATH_SEPARATOR);

    QVariantMap root;
    root["ref"] = PATH_SEPARATOR;
    root["opaque_ref"] = PATH_SEPARATOR;
    root["uuid"] = PATH_SEPARATOR;
    root["name_label"] = QStringLiteral("Folders");
    root["isRootFolder"] = true;
    root["parent"] = QString();
    connection->GetCache()->Update(XenObjectType::Folder, PATH_SEPARATOR, root);

    for (const QString& type : this->searchableTypes())
    {
        const XenObjectType objectType = XenCache::TypeFromString(type);
        const QList<QVariantMap> records = connection->GetCache()->GetAllData(objectType);
        for (const QVariantMap& record : records)
        {
            const QString path = FolderPathFromRecord(record);
            if (path.isEmpty())
                continue;
            this->ensureFolderChain(connection, path, existing);
        }
    }

    const QStringList emptyFolders = this->getEmptyFolders(connection);
    for (const QString& path : emptyFolders)
        this->ensureFolderChain(connection, path, existing);

    const QStringList current = connection->GetCache()->GetAllRefs(XenObjectType::Folder);
    for (const QString& folderRef : current)
    {
        if (!existing.contains(folderRef))
            connection->GetCache()->Remove(XenObjectType::Folder, folderRef);
    }

    this->m_rebuildInProgress[connection] = false;
    emit FoldersChanged(connection);
}

void FoldersManager::ensureFolderChain(XenConnection* connection, const QString& path, QSet<QString>& existingFolders)
{
    if (!connection || !connection->GetCache())
        return;

    QString normalized = path.trimmed();
    if (normalized.isEmpty())
        return;
    if (!normalized.startsWith(PATH_SEPARATOR))
        normalized.prepend(PATH_SEPARATOR);

    const QStringList points = PointToPath(normalized);
    for (int depth = 1; depth <= points.size(); ++depth)
    {
        const QString folderRef = PathToPoint(points, depth);
        existingFolders.insert(folderRef);

        if (connection->GetCache()->Contains(XenObjectType::Folder, folderRef))
            continue;

        QVariantMap folderRecord;
        folderRecord["ref"] = folderRef;
        folderRecord["opaque_ref"] = folderRef;
        folderRecord["uuid"] = folderRef;
        folderRecord["name_label"] = points.at(depth - 1);
        folderRecord["isRootFolder"] = false;
        folderRecord["parent"] = (depth > 1) ? PathToPoint(points, depth - 1) : PATH_SEPARATOR;
        connection->GetCache()->Update(XenObjectType::Folder, folderRef, folderRecord);
    }
}

QStringList FoldersManager::getEmptyFolders(XenConnection* connection) const
{
    QStringList emptyFolders;
    if (!connection || !connection->GetCache())
        return emptyFolders;

    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    if (!pool)
        return emptyFolders;

    const QVariantMap otherConfig = pool->GetOtherConfig();
    const QString raw = otherConfig.value(EMPTY_FOLDERS_KEY).toString();
    if (raw.isEmpty())
        return emptyFolders;

    const QStringList pieces = raw.split(EMPTY_FOLDERS_SEPARATOR, Qt::SkipEmptyParts);
    for (const QString& piece : pieces)
    {
        const QString trimmed = piece.trimmed();
        if (trimmed.startsWith(PATH_SEPARATOR))
            emptyFolders.append(trimmed);
    }
    emptyFolders.removeDuplicates();
    emptyFolders.sort();
    return emptyFolders;
}

void FoldersManager::setEmptyFolders(XenConnection* connection, const QStringList& emptyFolders)
{
    if (!connection || !connection->GetCache())
        return;

    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    if (!pool)
        return;

    QVariantMap poolRecord = pool->GetData();
    QVariantMap otherConfig = poolRecord.value("other_config").toMap();
    if (emptyFolders.isEmpty())
        otherConfig.remove(EMPTY_FOLDERS_KEY);
    else
        otherConfig[EMPTY_FOLDERS_KEY] = emptyFolders.join(EMPTY_FOLDERS_SEPARATOR);

    XenAPI::Session* session = connection->GetSession();
    if (session && session->IsLoggedIn())
    {
        XenRpcAPI api(session);
        QVariantList params;
        params << session->GetSessionID() << pool->OpaqueRef() << otherConfig;
        const QByteArray request = api.BuildJsonRpcCall("pool.set_other_config", params);
        const QByteArray response = session->SendApiRequest(request);
        api.ParseJsonRpcResponse(response);
    }

    poolRecord["other_config"] = otherConfig;
    connection->GetCache()->Update(XenObjectType::Pool, pool->OpaqueRef(), poolRecord);
}

QStringList FoldersManager::searchableTypes() const
{
    return QStringList{QStringLiteral("host"), QStringLiteral("network"), QStringLiteral("pool"), QStringLiteral("sr"), QStringLiteral("vdi"), QStringLiteral("vm")};
}
