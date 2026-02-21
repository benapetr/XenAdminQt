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

#ifndef CONTEXTMENUBUILDER_H
#define CONTEXTMENUBUILDER_H

#include <QObject>
#include <QMenu>
#include <QTreeWidgetItem>

class MainWindow;
class Command;
class XenObject;
class VM;
class Host;
class Pool;
class SR;
class Network;
class VDI;
class VMAppliance;
class XenConnection;
class GroupingTag;

/**
 * @brief Builds context menus for different XenObject types
 *
 * This class follows the same pattern as the original C# ContextMenuBuilder.
 * It creates appropriate context menus based on the selected object type.
 */
class ContextMenuBuilder : public QObject
{
    Q_OBJECT

    public:
        explicit ContextMenuBuilder(MainWindow* mainWindow, QObject* parent = nullptr);

        /**
         * @brief Build context menu for the given tree item
         */
        QMenu* BuildContextMenu(QTreeWidgetItem* item, QWidget* parent = nullptr);

    private:
        MainWindow* m_mainWindow;

        // Builders for specific object types
        void buildVMContextMenu(QMenu* menu, QSharedPointer<VM> vm);
        void buildSnapshotContextMenu(QMenu* menu, QSharedPointer<VM> snapshot);
        void buildTemplateContextMenu(QMenu* menu, QSharedPointer<VM> templateVM);
        void buildHostContextMenu(QMenu* menu, QSharedPointer<Host> host);
        void buildDisconnectedHostContextMenu(QMenu* menu, QTreeWidgetItem* item);
        void buildSRContextMenu(QMenu* menu, QSharedPointer<SR> sr);
        void buildPoolContextMenu(QMenu* menu, QSharedPointer<Pool> pool);
        void buildNetworkContextMenu(QMenu* menu, QSharedPointer<Network> network);
        void buildVDIContextMenu(QMenu* menu, QSharedPointer<VDI> vdi);
        void buildVMApplianceContextMenu(QMenu* menu, QSharedPointer<VMAppliance> appliance);
        void buildFolderContextMenu(QMenu* menu, QSharedPointer<XenObject> folderObj);
        void buildTagGroupingContextMenu(QMenu* menu, GroupingTag* groupingTag);
        void buildFolderGroupingContextMenu(QMenu* menu, GroupingTag* groupingTag);

        QList<QSharedPointer<VM>> getSelectedVMs() const;
        QList<QSharedPointer<Host>> getSelectedHosts() const;
        QList<XenConnection*> getSelectedConnections() const;

        // Helper methods
        void addCommand(QMenu* menu, Command* command);
        void addCommandAlways(QMenu* menu, Command* command);
        void addSeparator(QMenu* menu);
};

#endif // CONTEXTMENUBUILDER_H
