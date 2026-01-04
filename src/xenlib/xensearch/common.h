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

#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <QIcon>
#include <functional>

class XenObject;
class XenConnection;

/**
 * @brief XenSearch namespace - Search and filtering infrastructure
 * 
 * Qt equivalent of C# XenAdmin.XenSearch namespace.
 * Contains property accessors, type definitions, and search/filter helpers.
 * 
 * C# equivalent: xenadmin/XenModel/XenSearch/Common.cs
 */
namespace XenSearch
{

/**
 * @brief Object types for XenServer objects
 * 
 * The order of this enum determines the order of types in the tree in Folder View.
 * This is a flags enum - values can be combined with bitwise OR.
 * 
 * C# equivalent: ObjectTypes enum in Common.cs
 */
enum class ObjectTypes
{
    None = 0,
    Pool = 1 << 0,
    Server = 1 << 1,
    DisconnectedServer = 1 << 2,
    VM = 1 << 3,
    Snapshot = 1 << 4,
    UserTemplate = 1 << 5,
    DefaultTemplate = 1 << 6,
    RemoteSR = 1 << 7,
    LocalSR = 1 << 8,
    VDI = 1 << 9,
    Network = 1 << 10,
    Folder = 1 << 11,
    AllIncFolders = (1 << 12) - 1,
    AllExcFolders = AllIncFolders & ~static_cast<int>(Folder),
    Appliance = 1 << 13,
    DockerContainer = 1 << 14
};

// Enable bitwise operations for ObjectTypes
inline ObjectTypes operator|(ObjectTypes a, ObjectTypes b)
{
    return static_cast<ObjectTypes>(static_cast<int>(a) | static_cast<int>(b));
}

inline ObjectTypes operator&(ObjectTypes a, ObjectTypes b)
{
    return static_cast<ObjectTypes>(static_cast<int>(a) & static_cast<int>(b));
}

inline ObjectTypes operator~(ObjectTypes a)
{
    return static_cast<ObjectTypes>(~static_cast<int>(a));
}

inline ObjectTypes& operator|=(ObjectTypes& a, ObjectTypes b)
{
    a = a | b;
    return a;
}

inline ObjectTypes& operator&=(ObjectTypes& a, ObjectTypes b)
{
    a = a & b;
    return a;
}

inline bool operator==(ObjectTypes a, int b)
{
    return static_cast<int>(a) == b;
}

inline bool operator!=(ObjectTypes a, int b)
{
    return static_cast<int>(a) != b;
}

/**
 * @brief Property names for XenServer object properties
 * 
 * These are the canonical property names used for search, grouping, and filtering.
 * 
 * C# equivalent: PropertyNames enum in Common.cs
 */
enum class PropertyNames
{
    // Core properties
    type,                    // The type of the selected object (VM, Network, etc.)
    label,                   // The label/name of the selected object
    uuid,                    // The UUID of the object, or full pathname for folders
    description,             // The description of the object
    tags,                    // Comma-separated list of tags
    
    // Relationship properties
    host,                    // The host name
    pool,                    // The pool name
    networks,                // Comma-separated list of network names attached to object
    storage,                 // Comma-separated list of storage attached to object
    disks,                   // Comma-separated list of storage types
    
    // VM-specific properties
    memory,                  // Host memory in bytes
    os_name,                 // Operating system name (for VMs)
    power_state,             // VM power state (Halted, Running, etc.)
    virtualisation_status,   // PV drivers installation state
    start_time,              // Date/time VM was started
    ha_restart_priority,     // HA restart priority
    size,                    // Size in bytes of attached disks
    ip_address,              // Comma-separated list of IP addresses
    uptime,                  // Uptime string ("2 days, 1 hour, 26 minutes")
    
    // Pool/HA properties
    ha_enabled,              // true if HA is enabled
    isNotFullyUpgraded,      // true if pool has mixed versions
    appliance,               // VM appliance (logical set of VMs)
    
    // Storage properties
    shared,                  // true if storage is shared
    sr_type,                 // Storage type
    
    // VM lists
    vm,                      // Comma-separated list of VM names
    dockervm,                // List of Docker host-VM names
    
    // VM features
    read_caching_enabled,    // Whether VM is using read caching
    
    // Folder properties
    folder,                  // Immediate parent folder
    folders,                 // All ancestor folders
    
    // Internal properties (used for populating UI)
    memoryText,              // Memory usage text for display
    memoryValue,             // Memory usage value for sorting
    memoryRank,              // Memory usage rank for comparison
    cpuText,                 // CPU usage text for display
    cpuValue,                // CPU usage value for sorting
    diskText,                // Disk usage text for display
    networkText,             // Network usage text for display
    haText,                  // HA status text for display
    
    // Hidden properties (used for plugins)
    connection_hostname,     // Comma-separated list of host names
    license,                 // Pool license string representation
    has_custom_fields,       // Whether object has custom fields defined
    in_any_appliance,        // Whether VM is in any vApp
    vendor_device_state      // Windows Update capability
};

/**
 * @brief Column names for search results display
 * 
 * C# equivalent: ColumnNames enum in Common.cs
 */
enum class ColumnNames
{
    name,
    cpu,
    memory,
    disks,
    network,
    ha,
    ip,
    uptime
};

/**
 * @brief Property accessors for XenServer objects
 * 
 * Provides centralized access to object properties for search, grouping, and filtering.
 * This class maintains mappings between property names, types, display strings, and icons.
 * 
 * C# equivalent: PropertyAccessors class in Common.cs
 */
class PropertyAccessors
{
    public:
        /**
         * @brief Initialize property accessor maps
         * 
         * Called automatically on first use. Sets up all property type mappings,
         * i18n strings, and accessor functions.
         */
        static void Initialize();
        
        /**
         * @brief Get property accessor function for a property
         * 
         * @param property Property name
         * @return Function that extracts the property value from an object
         */
        static std::function<QVariant(XenObject*)> Get(PropertyNames property);
        
        /**
         * @brief Get the C++ type of a property
         * 
         * @param property Property name
         * @return Type name string (e.g., "QString", "bool", "int")
         */
        static QString GetType(PropertyNames property);
        
        /**
         * @brief Get i18n map for enum property values
         * 
         * Returns a map from display strings to enum values (e.g., "Running" -> vm_power_state::Running)
         * 
         * @param property Property name
         * @return Map of display string to enum value, or empty map if not applicable
         */
        static QMap<QString, QVariant> GetI18nFor(PropertyNames property);
        
        /**
         * @brief Get icon provider function for property values
         * 
         * @param property Property name
         * @return Function that returns icon for a property value, or null if not applicable
         */
        static std::function<QIcon(const QVariant&)> GetImagesFor(PropertyNames property);
        
        /**
         * @brief Get property name used for sorting a column
         * 
         * @param column Column name
         * @return Property name to use for sorting
         */
        static PropertyNames GetSortPropertyName(ColumnNames column);
        
        /**
         * @brief Get localized display name for a property
         * 
         * @param property Property name
         * @return Localized display string
         */
        static QString GetPropertyDisplayName(PropertyNames property);
        
        /**
         * @brief Get localized display name for a property value (for boolean false)
         * 
         * Some boolean properties have different display names for false values.
         * 
         * @param property Property name
         * @return Localized display string for false value, or empty if not applicable
         */
        static QString GetPropertyDisplayNameFalse(PropertyNames property);
        
        /**
         * @brief Get localized display name for an object type
         * 
         * @param type Object type
         * @return Localized display string
         */
        static QString GetObjectTypeDisplayName(ObjectTypes type);
        
        /**
         * @brief Get icon for an object type
         * 
         * @param type Object type
         * @return Icon for the object type
         */
        static QIcon GetObjectTypeIcon(ObjectTypes type);
        
    private:
        static bool initialized_;
        
        // Property type mappings
        static QMap<PropertyNames, QString> propertyTypes_;
        
        // Property accessor functions
        static QMap<PropertyNames, std::function<QVariant(XenObject*)>> properties_;
        
        // i18n mappings
        static QMap<QString, int> vmPowerStateI18n_;
        static QMap<QString, int> virtualisationStatusI18n_;
        static QMap<QString, ObjectTypes> objectTypesI18n_;
        static QMap<QString, int> haRestartPriorityI18n_;
        static QMap<QString, int> srTypeI18n_;
        
        static QMap<PropertyNames, QString> propertyNamesI18n_;
        static QMap<PropertyNames, QString> propertyNamesI18nFalse_;
        
        // Icon mappings
        static QMap<int, QString> vmPowerStateImages_;
        static QMap<ObjectTypes, QString> objectTypesImages_;
        
        // Column sort mappings
        static QMap<ColumnNames, PropertyNames> columnSortBy_;
        
        // Individual property accessor implementations
        static QVariant DescriptionProperty(XenObject* o);
        static QVariant UptimeProperty(XenObject* o);
        static QVariant CPUTextProperty(XenObject* o);
        static QVariant CPUValueProperty(XenObject* o);
        static QVariant MemoryTextProperty(XenObject* o);
        static QVariant MemoryValueProperty(XenObject* o);
        static QVariant MemoryRankProperty(XenObject* o);
        static QVariant NetworkTextProperty(XenObject* o);
        static QVariant DiskTextProperty(XenObject* o);
        static QVariant HATextProperty(XenObject* o);
        static QVariant UUIDProperty(XenObject* o);
        static QVariant ConnectionHostnameProperty(XenObject* o);
        static QVariant SharedProperty(XenObject* o);
        static QVariant TypeProperty(XenObject* o);
        static QVariant NetworksProperty(XenObject* o);
        static QVariant VMProperty(XenObject* o);
        static QVariant HostProperty(XenObject* o);
        static QVariant StorageProperty(XenObject* o);
        static QVariant DisksProperty(XenObject* o);
        static QVariant IPAddressProperty(XenObject* o);
};

/**
 * @brief Wrapper for property values with toString() support
 * 
 * Used to get display-friendly string representation of property values.
 * 
 * C# equivalent: PropertyWrapper class in Common.cs
 */
class PropertyWrapper
{
    public:
        /**
         * @brief Construct property wrapper
         * 
         * @param property Property to wrap
         * @param object Object to get property from
         */
        PropertyWrapper(PropertyNames property, XenObject* object);
        
        /**
         * @brief Get string representation of property value
         * 
         * @return Display string, or "-" if value is null/invalid
         */
        QString ToString() const;
        
    private:
        std::function<QVariant(XenObject*)> property_;
        XenObject* object_;
};

} // namespace XenSearch

#endif // COMMON_H
