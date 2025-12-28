/* Copyright (c) 2025 Petr Bena
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SEARCHTABPAGE_H
#define SEARCHTABPAGE_H

#include "basetabpage.h"
#include <QTableWidget>
#include <QMap>
#include <QVariant>

class Search;
class Grouping;

/**
 * @brief SearchTabPage - Displays search results in multi-column table
 *
 * This widget displays the results of a Search in a table with columns for
 * Name, CPU Usage, Memory Usage, Disks, Network, IP Address, and Uptime.
 * It's used for the overview panel feature when clicking on grouping tags
 * like "Servers" or "Virtual Machines" in the navigation tree.
 *
 * C# Equivalent: XenAdmin.Controls.XenSearch.QueryPanel
 * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs
 *
 * Architecture:
 * - Takes a Search* object which contains Query (what to match)
 * - Uses QTableWidget with 8 columns matching C# layout
 * - Iterates all objects from XenCache
 * - Filters using Query->match()
 * - Displays in flat table (no grouping hierarchy in table)
 * - Shows metrics: CPU%, Memory%, Network throughput, Disk I/O, Uptime
 *
 * Columns (matching C# DEFAULT_COLUMNS):
 * - Name: Object name + description
 * - CPU Usage: Progress bar + "X% of Y CPUs"
 * - Used Memory: Progress bar + "X GB of Y GB"
 * - Disks: "avg / max KB/s" disk I/O
 * - Network: "avg / max KB/s" network throughput
 * - Address: IP address for VMs, hostname for hosts
 * - Uptime: Formatted uptime string
 */
class SearchTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit SearchTabPage(QWidget* parent = nullptr);
        ~SearchTabPage() override;

        // BaseTabPage interface
        bool IsApplicableForObjectType(const QString& type) const override;
        void SetXenObject(XenConnection *conn, const QString& type, const QString& ref, const QVariantMap& data) override;
        QString GetTitle() const override
        {
            return tr("Search");
        }

        /**
         * @brief Set the search to display
         * @param search The Search object (takes ownership)
         *
         * C# Equivalent: QueryPanel.Search property setter
         * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs line 378
         */
        void setSearch(Search* search);

        /**
         * @brief Get the current search
         * @return The Search object (caller does not take ownership)
         */
        Search* getSearch() const
        {
            return m_search;
        }

        /**
         * @brief Build/rebuild the results list
         *
         * This method fetches all objects from XenCache, filters them using
         * the Query, and populates the table with columns.
         *
         * C# Equivalent: QueryPanel.BuildList()
         * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs line 530
         */
        void buildList();

    signals:
        /**
         * @brief Emitted when user double-clicks an object in the table
         * @param objectType Type of object ("vm", "host", "sr", etc.)
         * @param objectRef Opaque reference
         */
        void objectSelected(const QString& objectType, const QString& objectRef);

    private slots:
        void onItemDoubleClicked(int row, int column);
        void onItemSelectionChanged();
        void onXenLibObjectsChanged();

    private:
        // Column indices
        enum Column
        {
            COL_NAME = 0,
            COL_CPU = 1,
            COL_MEMORY = 2,
            COL_DISKS = 3,
            COL_NETWORK = 4,
            COL_ADDRESS = 5,
            COL_UPTIME = 6,
            COL_COUNT = 7
        };

        /**
         * @brief Populate the table with search results
         *
         * Main algorithm:
         * 1. Get all objects from XenCache (VMs, Hosts, SRs, etc.)
         * 2. Filter each object: if (query->match(objectData, objectType, xenLib)) keep it
         * 3. Add each matching object as a table row with all column data
         *
         * C# Equivalent: QueryPanel.listUpdateManager_Update() + CreateRow()
         * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 500-570
         */
        void populateTable();

        /**
         * @brief Add a row to the table for an object
         * @param objectType Type of object ("vm", "host", "sr", etc.)
         * @param objectRef Opaque reference
         * @param objectData Full object data from cache
         *
         * Creates a table row with all column values:
         * - Name, CPU%, Memory%, Disks, Network, IP, Uptime
         *
         * C# Equivalent: QueryPanel.CreateRow() for IXenObject
         * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 629-679
         */
        void addObjectRow(const QString& objectType, const QString& objectRef,
                          const QVariantMap& objectData);

        /**
         * @brief Get display name for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Display name (name_label)
         */
        QString getObjectName(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get description for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Description (name_description)
         */
        QString getObjectDescription(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get CPU usage text for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return CPU usage string (e.g., "22% of 8 CPUs") or empty
         *
         * C# Equivalent: PropertyAccessorHelper.vmCpuUsageString() / hostCpuUsageString()
         */
        QString getCPUUsageText(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get CPU usage percentage for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return CPU usage percentage (0-100) or -1 if not available
         *
         * C# Equivalent: PropertyAccessorHelper.vmCpuUsageRank() / hostCpuUsageRank()
         */
        int getCPUUsagePercent(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get memory usage text for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Memory usage string (e.g., "63.6 GB of 63.8 GB") or empty
         *
         * C# Equivalent: PropertyAccessorHelper.vmMemoryUsageString() / hostMemoryUsageString()
         */
        QString getMemoryUsageText(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get memory usage percentage for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Memory usage percentage (0-100) or -1 if not available
         */
        int getMemoryUsagePercent(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get disk I/O text for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Disk I/O string (e.g., "avg / max KB/s") or "-"
         *
         * C# Equivalent: PropertyAccessorHelper.vmDiskUsageString()
         */
        QString getDiskUsageText(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get network throughput text for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Network string (e.g., "1663/4466 KB/s") or empty
         *
         * C# Equivalent: PropertyAccessorHelper.vmNetworkUsageString()
         */
        QString getNetworkUsageText(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get IP address for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return IP address or hostname
         *
         * C# Equivalent: VM.IPAddressForDisplay() / Host.address
         */
        QString getIPAddress(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Get uptime string for an object
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return Uptime string (e.g., "784 days 5 hours 1 minute")
         *
         * C# Equivalent: VM.UptimeString() / Host.UptimeString()
         */
        QString getUptime(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Check if an object should be hidden from search results
         * @param objectType Type of object
         * @param objectData Object data from cache
         * @return true if object should be hidden
         *
         * Filters out:
         * - Control domain VMs (Dom0)
         * - Templates (shown separately)
         * - Tools SRs
         *
         * C# Equivalent: GroupAlg.Hide()
         * C# Reference: xenadmin/XenModel/XenSearch/GroupAlg.cs lines 105-148
         */
        bool shouldHideObject(const QString& objectType, const QVariantMap& objectData) const;

        /**
         * @brief Create a default search showing all objects of a given type
         * @param objectType The type of objects to show ("vm", "host", "sr", etc.)
         *
         * Creates a simple TypePropertyQuery-based search that shows all objects
         * of the specified type. This provides useful search results even when
         * not clicking on grouping tags.
         */
        void createDefaultSearchForType(const QString& objectType);

        Search* m_search;            // The search definition (owns pointer)
        QTableWidget* m_tableWidget; // Table view displaying results with columns
        QString m_currentObjectType; // Currently selected object type
        QString m_currentObjectRef;  // Currently selected object ref
};

#endif // SEARCHTABPAGE_H
