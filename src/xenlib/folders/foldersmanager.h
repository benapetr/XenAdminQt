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

#ifndef FOLDERSMANAGER_H
#define FOLDERSMANAGER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include "../xenlib_global.h"

class XenConnection;

namespace Xen
{
    class ConnectionsManager;
}

/**
 * @brief Client-side virtual folder service used by search/grouping/navigation.
 *
 * High-level model:
 * - Folders are virtual objects built on the client from metadata on real Xen objects.
 * - Object membership is read from each object's other_config["folder"] path.
 * - Empty folders are persisted separately in pool other_config["EMPTY_FOLDERS"].
 *
 * Data flow:
 * - When connection/cache updates arrive, this service rebuilds the folder tree.
 * - It creates/removes synthetic Folder records in XenCache so the rest of the app
 *   can treat folders as normal searchable/groupable objects.
 * - Folder helper APIs (path parsing, ancestry, descendants, move/create/delete)
 *   operate on this virtual tree representation.
 *
 * Related concepts in the same feature area:
 * - Tags are native Xen tags on objects (not virtual folder records), and can be
 *   managed globally across objects/connections.
 * - OtherConfigAndTagsWatcher batches config/tag/gui-config change notifications and
 *   emits them on connection object-update boundaries.
 * - Organization views (Objects/Tags/Folders/Custom Fields/vApps) are built from
 *   real query + grouping pipelines over cache objects, including virtual folders.
 */
class XENLIB_EXPORT FoldersManager : public QObject
{
    Q_OBJECT

    public:
        static FoldersManager* instance();

        static const QString FOLDER_KEY;
        static const QString PATH_SEPARATOR;
        static const QString EMPTY_FOLDERS_KEY;
        static const QString EMPTY_FOLDERS_SEPARATOR;

        void RegisterEventHandlers();
        void DeregisterEventHandlers();

        static QStringList PointToPath(const QString& path);
        static QString PathToPoint(const QStringList& path, int depth);
        static QString AppendPath(const QString& first, const QString& second);
        static QString GetParent(const QString& path);
        static QString FixupRelativePath(const QString& path);

        static QString FolderPathFromRecord(const QVariantMap& objectData);
        static QStringList AncestorFolders(const QString& path);

        QStringList Descendants(XenConnection* connection, const QString& path) const;
        bool HasSubfolders(XenConnection* connection, const QString& path) const;
        bool ContainsResources(XenConnection* connection, const QString& path) const;

        bool CreateFolder(XenConnection* connection, const QString& path);
        bool DeleteFolder(XenConnection* connection, const QString& path);
        bool MoveObjectToFolder(XenConnection* connection, const QString& objectType, const QString& objectRef, const QString& folderPath);
        bool UnfolderObject(XenConnection* connection, const QString& objectType, const QString& objectRef);

    signals:
        void FoldersChanged(XenConnection* connection);

    private slots:
        void onConnectionAdded(XenConnection* connection);
        void onConnectionRemoved(XenConnection* connection);
        void onConnectionObjectsUpdated();

    private:
        explicit FoldersManager(QObject* parent = nullptr);
        static FoldersManager* s_instance;

        struct ConnectionHandlers
        {
            QMetaObject::Connection xenObjectsUpdated;
            QMetaObject::Connection stateChanged;
        };

        void rebuildConnectionFolders(XenConnection* connection);
        void ensureFolderChain(XenConnection* connection, const QString& path, QSet<QString>& existingFolders);
        QStringList getEmptyFolders(XenConnection* connection) const;
        void setEmptyFolders(XenConnection* connection, const QStringList& emptyFolders);
        QStringList searchableTypes() const;

        bool m_registered = false;
        QHash<XenConnection*, ConnectionHandlers> m_handlers;
        QHash<XenConnection*, bool> m_rebuildInProgress;
};

#endif // FOLDERSMANAGER_H
