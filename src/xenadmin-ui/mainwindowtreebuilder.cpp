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

#include "mainwindowtreebuilder.h"
#include "globals.h"
#include "iconmanager.h"
#include "xen/pool.h"
#include "xen/host.h"
#include "xen/vm.h"
#include "xen/vmappliance.h"
#include "xen/sr.h"
#include "xen/network.h"
#include "xen/vdi.h"
#include "xen/folder.h"
#include "xensearch/groupingtag.h"
#include "xensearch/search.h"
#include "xensearch/grouping.h"
#include "xencache.h"
#include "settingsmanager.h"
#include "connectionprofile.h"
#include <QBrush>
#include <QTreeWidgetItem>
#include <QApplication>
#include <QPalette>
#include <QCollator>

//==============================================================================
// MainWindowTreeBuilder Implementation
//==============================================================================

MainWindowTreeBuilder::MainWindowTreeBuilder(QTreeWidget* treeView, QObject* parent) : QObject(parent),
      treeView_(treeView),
      lastSearchMode_(NavigationMode::Infrastructure),
      rootExpanded_(true),
      highlightedDragTarget_(nullptr)
{
    Q_ASSERT(treeView);
    
    // Store original colors
    QPalette palette = treeView->palette();
    this->treeViewForeColor_ = palette.color(QPalette::Text);
    this->treeViewBackColor_ = palette.color(QPalette::Base);
}

MainWindowTreeBuilder::~MainWindowTreeBuilder()
{
}

QObject* MainWindowTreeBuilder::GetHighlightedDragTarget() const
{
    return this->highlightedDragTarget_;
}

void MainWindowTreeBuilder::SetHighlightedDragTarget(QObject* target)
{
    this->highlightedDragTarget_ = target;
}

void MainWindowTreeBuilder::RefreshTreeView(QTreeWidgetItem* newRootNode, const QString& searchText, NavigationMode searchMode)
{
    Q_ASSERT(newRootNode);
    
    this->treeView_->setUpdatesEnabled(false);
    
    this->persistExpandedNodes(searchText);
    
    this->treeView_->clear();
    this->treeView_->addTopLevelItem(newRootNode);
    
    this->restoreExpandedNodes(searchText, searchMode);
    
    bool searchTextCleared = (searchText.isEmpty() && searchText != this->lastSearchText_);
    
    this->lastSearchText_ = searchText;
    this->lastSearchMode_ = searchMode;
    
    this->treeView_->setUpdatesEnabled(true);
    
    // Ensure selected nodes are visible when search text is cleared (CA-102127)
    if (searchTextCleared)
        this->expandSelection();
}

void MainWindowTreeBuilder::expandSelection()
{
    QList<QTreeWidgetItem*> selected = this->treeView_->selectedItems();
    for (QTreeWidgetItem* item : selected)
    {
        this->treeView_->scrollToItem(item);
        // Expand all parents to make item visible
        QTreeWidgetItem* parent = item->parent();
        while (parent)
        {
            parent->setExpanded(true);
            parent = parent->parent();
        }
    }
}

QTreeWidgetItem* MainWindowTreeBuilder::CreateNewRootNode(Search* search, NavigationMode mode, XenConnection* conn)
{
    MainWindowTreeNodeGroupAcceptor* groupAcceptor = nullptr;
    QTreeWidgetItem* newRootNode = nullptr;
    
    switch (mode)
    {
        case NavigationMode::Objects:
            Q_ASSERT(search);
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, "Objects");
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        case NavigationMode::Tags:
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, "Tags");
            if (search && search->GetGrouping())
                newRootNode->setData(0, Qt::UserRole + 3, QVariant::fromValue(new GroupingTag(search->GetGrouping(), QVariant(), QVariant("Tags"))));
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            if (search)
                search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        case NavigationMode::Folders:
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, "Folders");
            if (search && search->GetGrouping())
                newRootNode->setData(0, Qt::UserRole + 3, QVariant::fromValue(new GroupingTag(search->GetGrouping(), QVariant(), QVariant("Folders"))));
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            if (search)
                search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        case NavigationMode::CustomFields:
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, "Custom Fields");
            if (search && search->GetGrouping())
                newRootNode->setData(0, Qt::UserRole + 3, QVariant::fromValue(new GroupingTag(search->GetGrouping(), QVariant(), QVariant("Custom Fields"))));
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            if (search)
                search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        case NavigationMode::vApps:
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, "vApps");
            if (search && search->GetGrouping())
                newRootNode->setData(0, Qt::UserRole + 3, QVariant::fromValue(new GroupingTag(search->GetGrouping(), QVariant(), QVariant("vApps"))));
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            if (search)
                search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        case NavigationMode::SavedSearch:
            Q_ASSERT(search);
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, search->GetName());
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
            
        default: // Infrastructure and Notifications
            Q_ASSERT(search);
            newRootNode = new QTreeWidgetItem();
            newRootNode->setText(0, XENADMIN_BRANDING_NAME); // TODO: Use BrandManager
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
    }
    
    delete groupAcceptor;
    return newRootNode;
}

MainWindowTreeNodeGroupAcceptor* MainWindowTreeBuilder::createGroupAcceptor(QTreeWidgetItem* parent)
{
    return new MainWindowTreeNodeGroupAcceptor(this->highlightedDragTarget_, this->treeViewForeColor_, this->treeViewBackColor_, parent);
}

QList<MainWindowTreeBuilder::PersistenceInfo>& MainWindowTreeBuilder::assignList(NavigationMode mode)
{
    switch (mode)
    {
        case NavigationMode::Objects:
            return this->objectViewExpanded_;
        case NavigationMode::Tags:
            return this->tagsViewExpanded_;
        case NavigationMode::Folders:
            return this->foldersViewExpanded_;
        case NavigationMode::CustomFields:
            return this->fieldsViewExpanded_;
        case NavigationMode::vApps:
            return this->vappsViewExpanded_;
        default:
            return this->infraViewExpanded_;
    }
}

QList<QTreeWidgetItem*> MainWindowTreeBuilder::allNodes() const
{
    QList<QTreeWidgetItem*> nodes;
    QList<QTreeWidgetItem*> pending;

    for (int i = 0; i < this->treeView_->topLevelItemCount(); ++i)
        pending.append(this->treeView_->topLevelItem(i));

    while (!pending.isEmpty())
    {
        QTreeWidgetItem* node = pending.takeFirst();
        if (!node)
            continue;

        nodes.append(node);
        for (int i = 0; i < node->childCount(); ++i)
            pending.append(node->child(i));
    }

    return nodes;
}

QVariant MainWindowTreeBuilder::nodeTag(QTreeWidgetItem* node) const
{
    if (!node)
        return QVariant();

    const QVariant groupingTag = node->data(0, Qt::UserRole + 3);
    if (groupingTag.canConvert<GroupingTag*>() && groupingTag.value<GroupingTag*>())
        return groupingTag;

    const QVariant objectTag = node->data(0, Qt::UserRole);
    if (objectTag.canConvert<QSharedPointer<XenObject>>() && objectTag.value<QSharedPointer<XenObject>>())
        return objectTag;

    return QVariant();
}

bool MainWindowTreeBuilder::tagsEqual(const QVariant& left, const QVariant& right) const
{
    if (!left.isValid() || !right.isValid())
        return !left.isValid() && !right.isValid();

    if (left.canConvert<QSharedPointer<XenObject>>() && right.canConvert<QSharedPointer<XenObject>>())
    {
        const QSharedPointer<XenObject> leftObject = left.value<QSharedPointer<XenObject>>();
        const QSharedPointer<XenObject> rightObject = right.value<QSharedPointer<XenObject>>();
        return leftObject && rightObject &&
               leftObject->GetObjectType() == rightObject->GetObjectType() &&
               leftObject->OpaqueRef() == rightObject->OpaqueRef();
    }

    if (left.canConvert<GroupingTag*>() && right.canConvert<GroupingTag*>())
    {
        GroupingTag* leftTag = left.value<GroupingTag*>();
        GroupingTag* rightTag = right.value<GroupingTag*>();
        return leftTag && rightTag && *leftTag == *rightTag;
    }

    return left == right;
}

QList<QVariant> MainWindowTreeBuilder::tagPath(QTreeWidgetItem* node) const
{
    QList<QVariant> path;

    for (QTreeWidgetItem* current = node; current && current->parent(); current = current->parent())
    {
        const QVariant tag = this->nodeTag(current);
        if (tag.isValid())
            path.prepend(tag);
    }

    return path;
}

bool MainWindowTreeBuilder::tagExistsUnder(QTreeWidgetItem* parent, const QVariant& tag) const
{
    if (!parent)
        return false;

    if (this->tagsEqual(this->nodeTag(parent), tag))
        return true;

    for (int i = 0; i < parent->childCount(); ++i)
    {
        if (this->tagExistsUnder(parent->child(i), tag))
            return true;
    }

    return false;
}

QTreeWidgetItem* MainWindowTreeBuilder::maximalSubTree(QTreeWidgetItem* node, const QVariant& tag) const
{
    if (!node || !node->parent())
        return node;

    QTreeWidgetItem* parent = node->parent();
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* sibling = parent->child(i);
        if (sibling == node)
            continue;

        if (this->tagExistsUnder(sibling, tag))
            return node;
    }

    return this->maximalSubTree(parent, tag);
}

MainWindowTreeBuilder::PersistenceInfo MainWindowTreeBuilder::persistenceInfo(QTreeWidgetItem* node) const
{
    PersistenceInfo info;
    info.tag = this->nodeTag(node);
    info.path = this->tagPath(node);

    QTreeWidgetItem* maxSubTree = this->maximalSubTree(node, info.tag);
    if (maxSubTree)
        info.pathToMaximalSubTree = this->tagPath(maxSubTree);

    return info;
}

int MainWindowTreeBuilder::tryExactMatch(const QList<QVariant>& path, QTreeWidgetItem** match) const
{
    if (match)
        *match = nullptr;

    if (!match || this->treeView_->topLevelItemCount() == 0)
        return 0;

    int index = 0;
    *match = this->treeView_->topLevelItem(0);

    while (index < path.count())
    {
        bool found = false;
        for (int i = 0; i < (*match)->childCount(); ++i)
        {
            QTreeWidgetItem* child = (*match)->child(i);
            const QVariant tag = this->nodeTag(child);
            if (tag.isValid() && this->tagsEqual(tag, path.at(index)))
            {
                *match = child;
                found = true;
                ++index;
                break;
            }
        }

        if (!found)
            break;
    }

    return index;
}

QTreeWidgetItem* MainWindowTreeBuilder::findNodeIn(QTreeWidgetItem* parent, const QVariant& tag) const
{
    if (!parent)
        return nullptr;

    if (this->tagsEqual(this->nodeTag(parent), tag))
        return parent;

    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* result = this->findNodeIn(parent->child(i), tag);
        if (result)
            return result;
    }

    return nullptr;
}

void MainWindowTreeBuilder::persistExpandedNodes(const QString& searchText)
{
    if (this->treeView_->topLevelItemCount() == 0)
        return;
    
    // Only persist the expansion state of nodes if there isn't an active search
    // If there's a search then we're just going to expand everything later
    // Also check _lastSearchText and _lastSearchMode to restore nodes to original state
    
    if (searchText.isEmpty() && this->lastSearchText_.isEmpty() && this->lastSearchMode_ != NavigationMode::SavedSearch)
    {
        QList<PersistenceInfo>& list = this->assignList(this->lastSearchMode_);
        list.clear();

        for (QTreeWidgetItem* node : this->allNodes())
        {
            if (node && node->parent() && node->isExpanded() && this->nodeTag(node).isValid())
                list.append(this->persistenceInfo(node));
        }
    }
    
    // Persist root node expansion state separately
    if (this->treeView_->topLevelItemCount() > 0)
    {
        QTreeWidgetItem* root = this->treeView_->topLevelItem(0);
        this->rootExpanded_ = root->isExpanded() || root->childCount() == 0;
    }
}

void MainWindowTreeBuilder::restoreExpandedNodes(const QString& searchText, NavigationMode searchMode)
{
    // Expand all nodes if there's a search and the search has changed
    if ((searchText != this->lastSearchText_ && !searchText.isEmpty()) ||
        (searchMode == NavigationMode::SavedSearch && this->lastSearchMode_ != NavigationMode::SavedSearch))
    {
        this->treeView_->expandAll();
    }
    
    if (searchText.isEmpty() && searchMode != NavigationMode::SavedSearch)
    {
        QList<PersistenceInfo>& list = this->assignList(searchMode);
        QList<QTreeWidgetItem*> unexpandedNodes = this->allNodes();

        for (const PersistenceInfo& info : list)
        {
            QTreeWidgetItem* match = nullptr;

            if (this->tryExactMatch(info.path, &match) >= info.path.count())
            {
                if (match && !match->isExpanded())
                    match->setExpanded(true);
                unexpandedNodes.removeAll(match);
                continue;
            }

            if (this->tryExactMatch(info.pathToMaximalSubTree, &match) >= info.pathToMaximalSubTree.count())
            {
                match = this->findNodeIn(match, info.tag);
                if (match)
                {
                    if (!match->isExpanded())
                        match->setExpanded(true);
                    unexpandedNodes.removeAll(match);
                }
            }
        }

        for (QTreeWidgetItem* node : unexpandedNodes)
        {
            if (node && node->parent() && node->isExpanded() && this->nodeTag(node).isValid())
                node->setExpanded(false);
        }

        // Special case for root node
        if (this->rootExpanded_ && this->treeView_->topLevelItemCount() > 0)
        {
            this->treeView_->topLevelItem(0)->setExpanded(true);
        }
    }
}

//==============================================================================
// MainWindowTreeNodeGroupAcceptor Implementation
//==============================================================================

MainWindowTreeNodeGroupAcceptor::MainWindowTreeNodeGroupAcceptor(QObject* highlightedDragTarget,
                                                                 const QColor& treeViewForeColor,
                                                                 const QColor& treeViewBackColor,
                                                                 QTreeWidgetItem* parent)
    : parent_(parent),
      treeViewForeColor_(treeViewForeColor),
      treeViewBackColor_(treeViewBackColor),
      highlightedDragTarget_(highlightedDragTarget),
      index_(0)
{
}

void MainWindowTreeNodeGroupAcceptor::FinishedInThisGroup(bool /*defaultExpand*/)
{
    // Intentionally no UI-level sorting.
    // Keep insertion order from XenSearch grouping pipeline to match C# behavior,
    // including type-aware ordering within infrastructure groups.
}

IAcceptGroups* MainWindowTreeNodeGroupAcceptor::Add(Grouping* grouping,
                                                     const QVariant& group,
                                                     const QString& objectType,
                                                     const QVariantMap& objectData,
                                                     int /*indent*/,
                                                     XenConnection* conn)
{
    if (!group.isValid())
        return nullptr;
    
    QTreeWidgetItem* node = nullptr;
    
    // If we have object data, this is a leaf node (actual XenServer object)
    if (!objectData.isEmpty())
    {
        QString ref = objectData.value("ref").toString();
        if (ref.isEmpty())
            ref = objectData.value("opaque_ref").toString();
        if (ref.isEmpty())
            ref = objectData.value("opaqueRef").toString();
        if (ref.isEmpty())
            ref = group.toString();

        QSharedPointer<XenObject> obj;
        if (conn && conn->GetCache() && !ref.isEmpty())
            obj = conn->GetCache()->ResolveObject(objectType, ref);

        if (obj)
        {
            if (QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(obj))
                node = this->addPoolNode(pool);
            else if (QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj))
                node = this->addHostNode(host);
            else if (QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj))
                node = this->addVMNode(vm);
            else if (QSharedPointer<VMAppliance> appliance = qSharedPointerDynamicCast<VMAppliance>(obj))
                node = this->addVmApplianceNode(appliance);
            else if (QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(obj))
                node = this->addSRNode(sr);
            else if (QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(obj))
                node = this->addNetworkNode(network);
            else if (QSharedPointer<VDI> vdi = qSharedPointerDynamicCast<VDI>(obj))
                node = this->addVDINode(vdi);
            else if (QSharedPointer<Folder> folder = qSharedPointerDynamicCast<Folder>(obj))
                node = this->addFolderNode(folder);

            if (!node)
            {
                const QString name = obj->GetName().isEmpty() ? obj->GetUUID() : obj->GetName();
                QIcon icon = IconManager::instance().GetIconForObject(obj);
                node = this->addNode(name, icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(obj));
            }
        } else
        {
            QString name = objectData.value("name_label").toString();
            if (name.isEmpty())
                name = objectData.value("uuid").toString();
            QIcon icon = IconManager::instance().GetIconForObject(conn, XenObject::TypeFromString(objectType), ref);
            node = this->addNode(name, icon, false, QVariant());
        }
    } else
    {
        // This is a group header node
        if (this->parent_)
        {
            for (int i = 0; i < this->parent_->childCount(); ++i)
            {
                QTreeWidgetItem* existingNode = this->parent_->child(i);
                if (!existingNode)
                    continue;

                const QVariant existingTagVar = existingNode->data(0, Qt::UserRole + 3);
                if (!existingTagVar.canConvert<GroupingTag*>())
                    continue;

                GroupingTag* existingTag = existingTagVar.value<GroupingTag*>();
                if (!existingTag || !existingTag->getGrouping() || !grouping)
                    continue;

                if (existingTag->getGrouping()->equals(grouping) && existingTag->getGroup() == group)
                {
                    node = existingNode;
                    break;
                }
            }
        }

        if (!node)
        {
            QString name = grouping ? grouping->getGroupName(group) : QString();
            QIcon icon = grouping ? grouping->getGroupIcon(group) : QIcon();
            GroupingTag* tag = new GroupingTag(grouping, this->getGroupingTagFromNode(this->parent_), group);
            node = this->addNode(name, icon, false, QVariant::fromValue(tag));
        }
    }
    
    if (node)
        return new MainWindowTreeNodeGroupAcceptor(this->highlightedDragTarget_, this->treeViewForeColor_, this->treeViewBackColor_, node);
    
    return nullptr;
}

QVariant MainWindowTreeNodeGroupAcceptor::getGroupingTagFromNode(QTreeWidgetItem* node)
{
    if (!node)
        return QVariant();
    
    QVariant tagVar = node->data(0, Qt::UserRole + 3);
    if (tagVar.canConvert<GroupingTag*>())
    {
        GroupingTag* gt = tagVar.value<GroupingTag*>();
        if (gt)
            return gt->getGroup();
    }

    QVariant objVar = node->data(0, Qt::UserRole);
    if (objVar.canConvert<QSharedPointer<XenObject>>())
        return objVar;

    return QVariant();
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addPoolNode(const QSharedPointer<Pool>& pool)
{
    if (!pool)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(pool);
    return this->addNode(pool->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(pool));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addHostNode(const QSharedPointer<Host>& host)
{
    if (!host)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(host);

    XenConnection* connection = host->GetConnection();
    QString name = host->GetName();
    bool isDisconnected = !host->IsConnected();

    if (isDisconnected && connection)
    {
        const QString hostname = connection->GetHostname();
        const int port = connection->GetPort();
        const QList<ConnectionProfile> profiles = SettingsManager::instance().LoadConnectionProfiles();
        for (const ConnectionProfile& profile : profiles)
        {
            if (profile.GetHostname() == hostname && profile.GetPort() == port)
            {
                name = profile.DisplayName();
                break;
            }
        }
    }

    QTreeWidgetItem* node = this->addNode(name, icon, false,
                                          QVariant::fromValue<QSharedPointer<XenObject>>(host));
    if (node && isDisconnected)
        node->setData(0, Qt::UserRole + 1, QStringLiteral("disconnected_host"));
    return node;
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addVMNode(const QSharedPointer<VM>& vm)
{
    // TODO: Check for vTPM restriction and template status
    bool hidden = vm->IsHidden();
    QString name = hidden ? QString("(%1)").arg(vm->GetName()) : vm->GetName();
    QIcon icon = IconManager::instance().GetIconForObject(vm);
    return this->addNode(name, icon, hidden, QVariant::fromValue<QSharedPointer<XenObject>>(vm));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addVmApplianceNode(const QSharedPointer<VMAppliance>& appliance)
{
    if (!appliance)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(appliance);
    return this->addNode(appliance->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(appliance));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addSRNode(const QSharedPointer<SR>& sr)
{
    bool hidden = sr->IsHidden();
    QString name = hidden ? QString("(%1)").arg(sr->GetName()) : sr->GetName();
    QIcon icon = IconManager::instance().GetIconForObject(sr);
    return this->addNode(name, icon, hidden, QVariant::fromValue<QSharedPointer<XenObject>>(sr));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addNetworkNode(const QSharedPointer<Network>& network)
{
    bool hidden = network->IsHidden();
    bool supporter = network->IsMember();
    QString rawName = network->GetName();
    QString name = supporter ? QString("NIC Bonded Member: %1").arg(rawName) :
                   hidden ? QString("(%1)").arg(rawName) : rawName;
    QIcon icon = IconManager::instance().GetIconForObject(network);
    return this->addNode(name, icon, supporter || hidden, QVariant::fromValue<QSharedPointer<XenObject>>(network));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addVDINode(const QSharedPointer<VDI>& vdi)
{
    QString name = vdi->GetName().isEmpty() ? QObject::tr("(No name)") : vdi->GetName();
    QIcon icon = IconManager::instance().GetIconForObject(vdi);
    return this->addNode(name, icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(vdi));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addFolderNode(const QSharedPointer<Folder>& folder)
{
    if (!folder)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(folder);
    return this->addNode(folder->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(folder));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addNode(const QString& name, const QIcon& icon, bool grayed, const QVariant& tagData)
{
    // Ellipsize name if too long (1000 chars in C#)
    QString displayName = name.length() > 1000 ? name.left(997) + "..." : name;
    
    QTreeWidgetItem* result = new QTreeWidgetItem();
    result->setText(0, displayName);
    result->setIcon(0, icon);

    if (tagData.isValid())
    {
        if (tagData.canConvert<GroupingTag*>())
        {
            result->setData(0, Qt::UserRole + 3, tagData);
        } else if (tagData.canConvert<QSharedPointer<XenObject>>())
        {
            result->setData(0, Qt::UserRole, tagData);
        }
    }

    if (this->parent_)
        this->parent_->insertChild(this->index_, result);
    this->index_++;
    
    bool highlighted = false;
    if (this->highlightedDragTarget_ && tagData.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = tagData.value<QSharedPointer<XenObject>>();
        highlighted = obj && obj.data() == this->highlightedDragTarget_;
    }
    
    if (highlighted)
    {
        result->setBackground(0, QApplication::palette().brush(QPalette::Highlight));
        result->setForeground(0, QApplication::palette().brush(QPalette::HighlightedText));
    } else if (grayed)
    {
        result->setBackground(0, QBrush(this->treeViewBackColor_));
        result->setForeground(0, QBrush(Qt::gray));
    } else
    {
        result->setBackground(0, QBrush(this->treeViewBackColor_));
        result->setForeground(0, QBrush(this->treeViewForeColor_));
    }
    
    return result;
}
