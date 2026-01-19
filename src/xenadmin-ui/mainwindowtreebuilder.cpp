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

// Forward declarations for organization views (to be implemented)
class OrganizationViewFields { public: QTreeWidgetItem* rootNode() { return nullptr; } };
class OrganizationViewFolders { public: QTreeWidgetItem* rootNode() { return nullptr; } };
class OrganizationViewTags { public: QTreeWidgetItem* rootNode() { return nullptr; } };
class OrganizationViewObjects { public: QTreeWidgetItem* rootNode() { return nullptr; } };
class OrganizationViewVapps { public: QTreeWidgetItem* rootNode() { return nullptr; } };

//==============================================================================
// MainWindowTreeBuilder Implementation
//==============================================================================

MainWindowTreeBuilder::MainWindowTreeBuilder(QTreeWidget* treeView, QObject* parent)
    : QObject(parent),
      treeView_(treeView),
      lastSearchMode_(NavigationMode::Infrastructure),
      rootExpanded_(true),
      highlightedDragTarget_(nullptr),
      viewFields_(new OrganizationViewFields()),
      viewFolders_(new OrganizationViewFolders()),
      viewTags_(new OrganizationViewTags()),
      viewObjects_(new OrganizationViewObjects()),
      viewVapps_(new OrganizationViewVapps())
{
    Q_ASSERT(treeView);
    
    // Store original colors
    QPalette palette = treeView->palette();
    this->treeViewForeColor_ = palette.color(QPalette::Text);
    this->treeViewBackColor_ = palette.color(QPalette::Base);
}

MainWindowTreeBuilder::~MainWindowTreeBuilder()
{
    delete this->viewFields_;
    delete this->viewFolders_;
    delete this->viewTags_;
    delete this->viewObjects_;
    delete this->viewVapps_;
}

QObject* MainWindowTreeBuilder::highlightedDragTarget() const
{
    return this->highlightedDragTarget_;
}

void MainWindowTreeBuilder::setHighlightedDragTarget(QObject* target)
{
    this->highlightedDragTarget_ = target;
}

void MainWindowTreeBuilder::refreshTreeView(QTreeWidgetItem* newRootNode, 
                                            const QString& searchText, 
                                            NavigationMode searchMode)
{
    Q_ASSERT(newRootNode);
    
    this->treeView_->setUpdatesEnabled(false);
    
    this->persistExpandedNodes(searchText);
    
    this->treeView_->clear();
    if (newRootNode)
        this->treeView_->addTopLevelItem(newRootNode);
    
    this->restoreExpandedNodes(searchText, searchMode);
    
    bool searchTextCleared = (searchText.isEmpty() && searchText != this->lastSearchText_);
    
    this->lastSearchText_ = searchText;
    this->lastSearchMode_ = searchMode;
    
    this->treeView_->setUpdatesEnabled(true);
    
    // Ensure selected nodes are visible when search text is cleared (CA-102127)
    if (searchTextCleared)
    {
        this->expandSelection();
    }
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

QTreeWidgetItem* MainWindowTreeBuilder::createNewRootNode(Search* search, NavigationMode mode, XenConnection* conn)
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
            newRootNode = this->viewTags_->rootNode();
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            // TODO: viewTags_->populate(search, groupAcceptor);
            break;
            
        case NavigationMode::Folders:
            newRootNode = this->viewFolders_->rootNode();
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            // TODO: viewFolders_->populate(search, groupAcceptor);
            break;
            
        case NavigationMode::CustomFields:
            newRootNode = this->viewFields_->rootNode();
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            // TODO: viewFields_->populate(search, groupAcceptor);
            break;
            
        case NavigationMode::vApps:
            newRootNode = this->viewVapps_->rootNode();
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            // TODO: viewVapps_->populate(search, groupAcceptor);
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
            newRootNode->setText(0, "XenAdmin"); // TODO: Use BrandManager
            groupAcceptor = this->createGroupAcceptor(newRootNode);
            search->PopulateAdapters(conn, QList<IAcceptGroups*>() << groupAcceptor);
            break;
    }
    
    delete groupAcceptor;
    return newRootNode;
}

MainWindowTreeNodeGroupAcceptor* MainWindowTreeBuilder::createGroupAcceptor(QTreeWidgetItem* parent)
{
    return new MainWindowTreeNodeGroupAcceptor(this->highlightedDragTarget_,
                                           this->treeViewForeColor_,
                                           this->treeViewBackColor_,
                                           parent);
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

void MainWindowTreeBuilder::persistExpandedNodes(const QString& searchText)
{
    if (this->treeView_->topLevelItemCount() == 0)
        return;
    
    // Only persist the expansion state of nodes if there isn't an active search
    // If there's a search then we're just going to expand everything later
    // Also check _lastSearchText and _lastSearchMode to restore nodes to original state
    
    if (searchText.isEmpty() && 
        this->lastSearchText_.isEmpty() && 
        this->lastSearchMode_ != NavigationMode::SavedSearch)
    {
        QList<PersistenceInfo>& list = this->assignList(this->lastSearchMode_);
        list.clear();
        
        // TODO: Iterate through all nodes and save expanded state
        // For now, this is a stub
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
        // If there isn't a search, persist the user's expanded nodes
        Q_UNUSED(this->assignList(searchMode));  // TODO: Use for restoration
        
        // TODO: Restore expansion state from list
        // This requires implementing path matching logic
        
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
    if (!this->parent_ || this->parent_->childCount() < 2)
        return;

    // Only sort within group headers to preserve grouping order at higher levels.
    const QVariant groupTag = this->parent_->data(0, Qt::UserRole + 3);
    if (!groupTag.isValid() || !groupTag.canConvert<GroupingTag*>())
        return;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    QList<QTreeWidgetItem*> children = this->parent_->takeChildren();
    std::stable_sort(children.begin(), children.end(),
                     [&collator](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                         return collator.compare(a->text(0), b->text(0)) < 0;
                     });
    for (QTreeWidgetItem* child : children)
        this->parent_->addChild(child);
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

            if (!node)
            {
                const QString name = obj->GetName().isEmpty() ? obj->GetUUID() : obj->GetName();
                QIcon icon = IconManager::instance().GetIconForObject(obj.data());
                node = this->addNode(name, icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(obj));
            }
        }
        else
        {
            QString name = objectData.value("name_label").toString();
            if (name.isEmpty())
                name = objectData.value("uuid").toString();
            QIcon icon = IconManager::instance().GetIconForObject(objectType.toLower(), objectData);
            node = this->addNode(name, icon, false, QVariant());
        }
    }
    else
    {
        // This is a group header node
        QString name = grouping ? grouping->getGroupName(group) : QString();
        QIcon icon = grouping ? grouping->getGroupIcon(group) : QIcon();
        GroupingTag* tag = new GroupingTag(grouping, this->getGroupingTagFromNode(this->parent_), group);
        node = this->addNode(name, icon, false, QVariant::fromValue(tag));
    }
    
    if (node)
    {
        return new MainWindowTreeNodeGroupAcceptor(this->highlightedDragTarget_,
                                                   this->treeViewForeColor_,
                                                   this->treeViewBackColor_,
                                                   node);
    }
    
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
    QIcon icon = IconManager::instance().GetIconForObject(pool.data());
    return this->addNode(pool->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(pool));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addHostNode(const QSharedPointer<Host>& host)
{
    if (!host)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(host.data());

    XenConnection* connection = host->GetConnection();
    QString name = host->GetName();
    bool isDisconnected = !host->IsConnected();

    if (isDisconnected && connection)
    {
        const QString hostname = connection->GetHostname();
        const int port = connection->GetPort();
        const QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
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
    QIcon icon = IconManager::instance().GetIconForObject(vm.data());
    return this->addNode(name, icon, hidden, QVariant::fromValue<QSharedPointer<XenObject>>(vm));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addVmApplianceNode(const QSharedPointer<VMAppliance>& appliance)
{
    if (!appliance)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(appliance.data());
    return this->addNode(appliance->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(appliance));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addSRNode(const QSharedPointer<SR>& sr)
{
    bool hidden = sr->IsHidden();
    QString name = hidden ? QString("(%1)").arg(sr->GetName()) : sr->GetName();
    QIcon icon = IconManager::instance().GetIconForObject(sr.data());
    return this->addNode(name, icon, hidden, QVariant::fromValue<QSharedPointer<XenObject>>(sr));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addNetworkNode(const QSharedPointer<Network>& network)
{
    bool hidden = network->IsHidden();
    bool supporter = network->IsMember();
    QString rawName = network->GetName();
    QString name = supporter ? QString("NIC Bonded Member: %1").arg(rawName) :
                   hidden ? QString("(%1)").arg(rawName) : rawName;
    QIcon icon = IconManager::instance().GetIconForObject(network.data());
    return this->addNode(name, icon, supporter || hidden, QVariant::fromValue<QSharedPointer<XenObject>>(network));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addVDINode(const QSharedPointer<VDI>& vdi)
{
    QString name = vdi->GetName().isEmpty() ? QObject::tr("(No name)") : vdi->GetName();
    QIcon icon = IconManager::instance().GetIconForObject(vdi.data());
    return this->addNode(name, icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(vdi));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addFolderNode(const QSharedPointer<Folder>& folder)
{
    if (!folder)
        return nullptr;
    QIcon icon = IconManager::instance().GetIconForObject(folder.data());
    return this->addNode(folder->GetName(), icon, false, QVariant::fromValue<QSharedPointer<XenObject>>(folder));
}

QTreeWidgetItem* MainWindowTreeNodeGroupAcceptor::addNode(const QString& name,
                                                          const QIcon& icon,
                                                          bool grayed,
                                                          const QVariant& tagData)
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
        }
        else if (tagData.canConvert<QSharedPointer<XenObject>>())
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
    }
    else if (grayed)
    {
        result->setBackground(0, QBrush(this->treeViewBackColor_));
        result->setForeground(0, QBrush(Qt::gray));
    }
    else
    {
        result->setBackground(0, QBrush(this->treeViewBackColor_));
        result->setForeground(0, QBrush(this->treeViewForeColor_));
    }
    
    return result;
}
