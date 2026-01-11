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

#ifndef GENERALEDITPAGEACTION_H
#define GENERALEDITPAGEACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QStringList>

class XenObject;

/**
 * @brief GeneralEditPageAction - Save folder and tag changes for any XenObject
 *
 * Qt equivalent of C# XenAdmin.Actions.GUIActions.GeneralEditPageAction
 *
 * Saves folder path and tag changes to a XenServer object's other_config.
 * This action handles the two types of metadata changes that require
 * API calls instead of simple property sets:
 *
 * 1. **Folder changes**: Moves object to new folder or removes from folder
 *    - Stored in other_config["folder"] as path string (e.g. "/MyFolder/SubFolder")
 *    - Empty string means object is at root (unfoldered)
 *
 * 2. **Tag changes**: Adds/removes tags on the object
 *    - Tags are string labels that can be added to any XenAPI object
 *    - Used for organization, grouping, and custom metadata
 *
 * Key features:
 * - Handles both folder movement and tag changes in single action
 * - RBAC permission checks for folder and tag operations
 * - Compares old vs new tags to only make necessary API calls
 * - Uses XenAPI add_tags/remove_tags methods
 * - Uses other_config["folder"] for folder path storage
 *
 * Usage pattern (matching C# GeneralEditPage.SaveSettings()):
 *   // Simple properties (name, description) changed via direct sets on xenObjectCopy
 *   if (nameChanged)
 *       xenObjectCopy->set("name_label", newName);
 *
 *   // Complex operations (folders, tags) require action
 *   if (folderChanged || tagsChanged)
 *       return new GeneralEditPageAction(xenObjectOrig, xenObjectCopy, newFolder, newTags);
 *
 * C# source: XenAdmin/Actions/GUIActions/GeneralEditPageAction.cs
 */
class XENLIB_EXPORT GeneralEditPageAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param objectRef OpaqueRef of the object being edited
         * @param objectType Type string ("vm", "host", "pool", "sr", etc.)
         * @param oldFolder Previous folder path (can be empty)
         * @param newFolder New folder path (empty string = unfolder/root)
         * @param oldTags Previous tags list
         * @param newTags New tags list
         * @param suppressHistory Whether to suppress history for this action
         * @param parent Parent QObject
         *
         * C# equivalent: GeneralEditPageAction(IXenObject xenObjectOrig, IXenObject xenObjectCopy,
         *                                       string newFolder, List<string> newTags, bool suppressHistory)
         *
         * Note: In C#, it takes IXenObject references. In Qt, we pass connection + objectRef + objectType
         * since we don't maintain persistent object wrappers.
         *
         * The old/new folder and tag values are used to determine what API calls to make:
         * - Folder changed: Set other_config["folder"] to newFolder (or remove key if empty)
         * - Tags added: Call add_tags for each new tag not in oldTags
         * - Tags removed: Call remove_tags for each old tag not in newTags
         */
        explicit GeneralEditPageAction(QSharedPointer<XenObject> object,
                                       const QString& oldFolder,
                                       const QString& newFolder,
                                       const QStringList& oldTags,
                                       const QStringList& newTags,
                                       bool suppressHistory = true,
                                       QObject* parent = nullptr);

    protected:
        /**
         * @brief Execute the folder and tag changes
         *
         * Steps:
         * 1. Folder changes:
         *    - If folder changed and newFolder not empty: Set other_config["folder"] = newFolder
         *    - If folder changed and newFolder empty: Remove other_config["folder"] (unfolder)
         *
         * 2. Tag removal:
         *    - For each tag in oldTags not in newTags: Call remove_tags(session, ref, tag)
         *
         * 3. Tag addition:
         *    - For each tag in newTags not in oldTags: Call add_tags(session, ref, tag)
         *
         * C# equivalent: Run() override
         *
         * C# implementation:
         *   if (newFolder != xenObjectCopy.Path)
         *   {
         *       if (!String.IsNullOrEmpty(newFolder))
         *           Folders.Move(Session, xenObjectOrig, newFolder);
         *       else
         *           Folders.Unfolder(Session, xenObjectOrig);
         *   }
         *
         *   foreach (string tag in oldTags)
         *       if (newTags.BinarySearch(tag) < 0)
         *           Tags.RemoveTag(Session, xenObjectOrig, tag);
         *
         *   foreach (string tag in newTags)
         *       if (oldTags.BinarySearch(tag) < 0)
         *           Tags.AddTag(Session, xenObjectOrig, tag);
         */
        void run() override;

    private:
        QSharedPointer<XenObject> m_object;
        QString m_oldFolder;   ///< Previous folder path
        QString m_newFolder;   ///< New folder path (empty = unfolder)
        QStringList m_oldTags; ///< Previous tags (sorted for efficient comparison)
        QStringList m_newTags; ///< New tags (sorted for efficient comparison)

        /**
         * @brief Set folder path in object's other_config
         * @param folderPath New folder path (empty to remove folder)
         *
         * Sets or removes the "folder" key in the object's other_config:
         * - Non-empty path: other_config["folder"] = folderPath
         * - Empty path: Remove other_config["folder"] key
         *
         * Uses XenAPI::{Type}.remove_from_other_config() / add_to_other_config()
         */
        void setFolderPath(const QString& folderPath);

        /**
         * @brief Remove a tag from the object
         * @param tag Tag string to remove
         *
         * Calls XenAPI::{Type}.remove_tags(session, ref, tag)
         *
         * C# equivalent: Tags.RemoveTag(session, o, tag)
         */
        void removeTag(const QString& tag);

        /**
         * @brief Add a tag to the object
         * @param tag Tag string to add
         *
         * Calls XenAPI::{Type}.add_tags(session, ref, tag)
         *
         * C# equivalent: Tags.AddTag(session, o, tag)
         */
        void addTag(const QString& tag);
};

#endif // GENERALEDITPAGEACTION_H
