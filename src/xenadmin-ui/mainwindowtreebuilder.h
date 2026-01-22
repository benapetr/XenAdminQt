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

#ifndef MAINWINDOWTREEBUILDER_H
#define MAINWINDOWTREEBUILDER_H

#include <QTreeWidget>
#include <QList>
#include <QString>
#include <QColor>
#include <QIcon>
#include <QObject>
#include <QSharedPointer>
#include "../xenlib/xensearch/iacceptgroups.h"

class Search;
class Grouping;
class MainWindowTreeNodeGroupAcceptor;

/**
 * @brief Builds and manages the main window tree view
 * 
 * Ported from C# XenAdmin.MainWindowTreeBuilder
 * Handles tree construction, expansion state persistence, and organization modes.
 */
class MainWindowTreeBuilder : public QObject
{
    Q_OBJECT

    public:
        /**
         * @brief Navigation modes for the tree view
         *
         * Matches NavigationPane.NavigationMode from C#
         */
        enum class NavigationMode
        {
            Infrastructure,  // Default infrastructure view
            Objects,        // Organize by object types
            Tags,           // Organize by tags
            Folders,        // Organize by folders
            CustomFields,   // Organize by custom fields
            vApps,          // Organize by vApps
            SavedSearch,    // Saved search view
            Notifications   // Notifications view
        };

        explicit MainWindowTreeBuilder(QTreeWidget* treeView, QObject* parent = nullptr);
        ~MainWindowTreeBuilder();

        /**
         * @brief Gets or sets an object that should be highlighted
         */
        QObject* GetHighlightedDragTarget() const;
        void SetHighlightedDragTarget(QObject* target);

        /**
         * @brief Updates the tree view with a new root node
         *
         * Merges the new root with existing nodes to minimize updates and reduce flicker.
         *
         * @param newRootNode The new root node
         * @param searchText Current search text
         * @param searchMode Current navigation mode
         */
        void RefreshTreeView(QTreeWidgetItem* newRootNode, const QString& searchText, NavigationMode searchMode);

        /**
         * @brief Creates a new root node based on search and mode
         *
         * @param search The search to use for populating the tree
         * @param mode The navigation mode
         * @param conn XenConnection for resolving objects
         * @return New root node
         */
        QTreeWidgetItem* CreateNewRootNode(Search* search, NavigationMode mode, XenConnection* conn);

    private:
        struct PersistenceInfo
        {
            QStringList path;
            QStringList pathToMaximalSubTree;
            QObject* tag;

            PersistenceInfo() : tag(nullptr) {}
        };

        void persistExpandedNodes(const QString& searchText);
        void restoreExpandedNodes(const QString& searchText, NavigationMode searchMode);
        void expandSelection();
        QList<PersistenceInfo>& assignList(NavigationMode mode);
        MainWindowTreeNodeGroupAcceptor* createGroupAcceptor(QTreeWidgetItem* parent);

        QTreeWidget* treeView_;
        QColor treeViewForeColor_;
        QColor treeViewBackColor_;
        QString lastSearchText_;
        NavigationMode lastSearchMode_;

        // Expansion state for different views
        QList<PersistenceInfo> infraViewExpanded_;
        QList<PersistenceInfo> objectViewExpanded_;
        QList<PersistenceInfo> tagsViewExpanded_;
        QList<PersistenceInfo> foldersViewExpanded_;
        QList<PersistenceInfo> fieldsViewExpanded_;
        QList<PersistenceInfo> vappsViewExpanded_;

        bool rootExpanded_;
        QObject* highlightedDragTarget_;

        // Organization views (will be implemented later)
        class OrganizationViewFields* viewFields_;
        class OrganizationViewFolders* viewFolders_;
        class OrganizationViewTags* viewTags_;
        class OrganizationViewObjects* viewObjects_;
        class OrganizationViewVapps* viewVapps_;
    };

    /**
     * @brief Accepts groups and builds tree nodes
     *
     * Ported from C# MainWindowTreeNodeGroupAcceptor
     * Implements IAcceptGroups interface for building hierarchical tree structure.
     */
    class MainWindowTreeNodeGroupAcceptor : public IAcceptGroups
    {
        public:
            MainWindowTreeNodeGroupAcceptor(QObject* highlightedDragTarget,
                                        const QColor& treeViewForeColor,
                                        const QColor& treeViewBackColor,
                                        QTreeWidgetItem* parent);

            // IAcceptGroups interface implementation
            IAcceptGroups* Add(Grouping* grouping, const QVariant& group,
                             const QString& objectType, const QVariantMap& objectData,
                             int indent, XenConnection* conn) override;
            void FinishedInThisGroup(bool defaultExpand) override;

        private:
            QTreeWidgetItem* addPoolNode(const QSharedPointer<class Pool>& pool);
            QTreeWidgetItem* addHostNode(const QSharedPointer<class Host>& host);
            QTreeWidgetItem* addVMNode(const QSharedPointer<class VM>& vm);
            QTreeWidgetItem* addVmApplianceNode(const QSharedPointer<class VMAppliance>& appliance);
            QTreeWidgetItem* addSRNode(const QSharedPointer<class SR>& sr);
            QTreeWidgetItem* addNetworkNode(const QSharedPointer<class Network>& network);
            QTreeWidgetItem* addVDINode(const QSharedPointer<class VDI>& vdi);
            QTreeWidgetItem* addFolderNode(const QSharedPointer<class Folder>& folder);
            QVariant getGroupingTagFromNode(QTreeWidgetItem* node);

            QTreeWidgetItem* addNode(const QString& name, const QIcon& icon, bool grayed, const QVariant& tagData);
            
            QTreeWidgetItem* parent_;
            QColor treeViewForeColor_;
            QColor treeViewBackColor_;
            QObject* highlightedDragTarget_;
            int index_;
};

#endif // MAINWINDOWTREEBUILDER_H
