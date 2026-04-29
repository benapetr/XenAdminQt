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

#include "ovfpackage.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDomDocument>
#include <QDomNodeList>
#include <QDomElement>
#include <QDataStream>
#include <QDebug>

// ─── TAR extraction ──────────────────────────────────────────────────────────

// Minimal POSIX ustar/GNU TAR reader.
// We only need the OVF descriptor (first .ovf entry in the archive).
//
// TAR block size is always 512 bytes.
// Header layout (POSIX ustar):
//   offset  size  field
//      0   100   filename
//    100     8   permissions (octal, ASCII)
//    ...
//    124    12   file size (octal, ASCII)
//    ...
//    257     6   "ustar" magic
//    ...
// For GNU TAR extended headers (long filename) the first header has type 'L'
// and the data block contains the real filename.

static bool readTarHeader(QFile& file, QString& entryName, qint64& entrySize)
{
    const int BLOCK = 512;
    QByteArray header = file.read(BLOCK);
    if (header.size() < BLOCK)
        return false;

    // All-zero block signals end of archive
    bool allZero = true;
    for (char c : header) { if (c != 0) { allZero = false; break; } }
    if (allZero)
        return false;

    // File name: first 100 bytes, null-terminated
    entryName = QString::fromLatin1(header.constData(), qstrnlen(header.constData(), 100));

    // GNU TAR prefix at offset 345 extends the filename for long paths (ustar format)
    // Prefix is at bytes 345–499 for POSIX ustar
    const QByteArray magic = header.mid(257, 5);
    if (magic == "ustar")
    {
        QByteArray prefix = header.mid(345, 155);
        QString prefixStr = QString::fromLatin1(prefix.constData(), qstrnlen(prefix.constData(), 155));
        if (!prefixStr.isEmpty())
            entryName = prefixStr + "/" + entryName;
    }

    // Size field: 12 bytes at offset 124, octal ASCII
    QByteArray sizeField = header.mid(124, 12);
    bool ok = false;
    entrySize = QString::fromLatin1(sizeField.constData(), qstrnlen(sizeField.constData(), 12)).toLongLong(&ok, 8);
    if (!ok)
        entrySize = 0;

    return true;
}

// ─── OvfPackage ──────────────────────────────────────────────────────────────

OvfPackage::OvfPackage(const QString& filePath)
    : sourceFile_(filePath)
    , valid_(false)
    , hasEncryption_(false)
    , hasManifest_(false)
    , hasSignature_(false)
{
    QFileInfo fi(filePath);
    if (!fi.exists())
    {
        this->parseError_ = QString("File not found: %1").arg(filePath);
        return;
    }

    const QString ext = fi.suffix().toLower();
    const QString ext2 = fi.completeSuffix().toLower(); // e.g. "ova.gz"

    if (ext == "ovf")
    {
        this->packageName_ = fi.completeBaseName();
        this->parseOvfFile(filePath);
    }
    else if (ext == "ova")
    {
        this->packageName_ = fi.completeBaseName();
        this->parseOvaFile(filePath);
    }
    else if (ext2 == "ova.gz" || ext2 == "tar.gz")
    {
        this->packageName_ = fi.baseName(); // strip last extension
        this->parseError_ = "Compressed OVA (.ova.gz) is not yet supported for source inspection. "
                            "The file will be accepted for import but metadata cannot be read.";
        // Mark valid so the wizard can still proceed; metadata will just be empty
        this->valid_ = true;
    }
    else
    {
        this->parseError_ = QString("Unsupported OVF package extension: %1").arg(ext);
    }
}

void OvfPackage::parseOvfFile(const QString& ovfFilePath)
{
    QFile f(ovfFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        this->parseError_ = QString("Cannot open OVF file: %1").arg(f.errorString());
        return;
    }

    const QString xml = QString::fromUtf8(f.readAll());
    f.close();

    // Check for companion .mf and .cert files
    QFileInfo fi(ovfFilePath);
    const QString base = fi.absoluteDir().filePath(fi.completeBaseName());
    this->hasManifest_ = QFile::exists(base + ".mf");
    this->hasSignature_ = QFile::exists(base + ".cert");

    this->parseFromOvfXml(xml);
}

void OvfPackage::parseOvaFile(const QString& ovaFilePath)
{
    // Extract OVF descriptor text from the TAR archive
    const QString xml = this->extractOvfFromTar(ovaFilePath);
    if (xml.isEmpty())
    {
        if (this->parseError_.isEmpty())
            this->parseError_ = "No OVF descriptor found inside OVA archive.";
        return;
    }

    // Manifest/signature are also inside the TAR; we don't extract them here
    // but we note they may exist. A more complete implementation would scan all entries.
    this->parseFromOvfXml(xml);
}

QString OvfPackage::extractOvfFromTar(const QString& tarFilePath)
{
    QFile file(tarFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        this->parseError_ = QString("Cannot open OVA archive: %1").arg(file.errorString());
        return QString();
    }

    const int BLOCK = 512;
    QString ovfXml;

    while (!file.atEnd())
    {
        QString entryName;
        qint64 entrySize = 0;

        if (!readTarHeader(file, entryName, entrySize))
            break;

        const bool isOvf = entryName.endsWith(".ovf", Qt::CaseInsensitive);
        const bool isMf  = entryName.endsWith(".mf",  Qt::CaseInsensitive);
        const bool isCert = entryName.endsWith(".cert", Qt::CaseInsensitive);

        if (entrySize > 0)
        {
            if (isOvf && ovfXml.isEmpty())
            {
                QByteArray data = file.read(entrySize);
                ovfXml = QString::fromUtf8(data);
                // Advance to next block boundary
                const qint64 padding = (BLOCK - (entrySize % BLOCK)) % BLOCK;
                if (padding > 0)
                    file.skip(padding);
            }
            else if (isMf)
            {
                this->hasManifest_ = true;
                // Advance past data
                const qint64 blocks = (entrySize + BLOCK - 1) / BLOCK * BLOCK;
                file.skip(blocks);
            }
            else if (isCert)
            {
                this->hasSignature_ = true;
                const qint64 blocks = (entrySize + BLOCK - 1) / BLOCK * BLOCK;
                file.skip(blocks);
            }
            else
            {
                // Skip data for other entries
                const qint64 blocks = (entrySize + BLOCK - 1) / BLOCK * BLOCK;
                file.skip(blocks);
            }
        }

        // If we found the OVF and already noted manifest/signature, we can stop
        if (!ovfXml.isEmpty() && (this->hasManifest_ || this->hasSignature_))
            break;
    }

    file.close();
    return ovfXml;
}

// ─── OVF XML parsing ─────────────────────────────────────────────────────────

// Return attribute value by local name regardless of namespace prefix.
// OVF files may use 'ovf:name', 'name', or other prefixed forms.
QString OvfPackage::firstAttributeByLocalName(const QDomElement& element, const QString& localName)
{
    QDomNamedNodeMap attrs = element.attributes();
    for (int i = 0; i < attrs.count(); ++i)
    {
        QDomAttr attr = attrs.item(i).toAttr();
        if (attr.localName() == localName)
            return attr.value();
    }
    return QString();
}

// Return all descendant elements with the given local name, regardless of namespace.
QDomNodeList OvfPackage::elementsByLocalName(QDomDocument doc, const QString& localName)
{
    // QDomDocument::elementsByTagName does prefix-aware matching only when a namespace
    // is not given. Since OVF files may use different prefixes, we use elementsByTagNameNS
    // with the known OVF namespaces and fall back to a wildcard scan.
    // Try standard OVF namespace first, then Citrix/Xen extension namespace.
    // Note: QDomDocument is passed by value (implicit sharing — cheap) because
    // elementsByTagNameNS/elementsByTagName are not marked const in Qt's implementation.
    static const QStringList ovfNamespaces = {
        "http://schemas.dmtf.org/ovf/envelope/1",
        "http://schemas.citrix.com/ovf/envelope/1"
    };

    for (const QString& ns : ovfNamespaces)
    {
        QDomNodeList list = doc.elementsByTagNameNS(ns, localName);
        if (!list.isEmpty())
            return list;
    }

    // Last resort: scan all elements for matching local name
    return doc.elementsByTagName(localName);
}

void OvfPackage::collectEulas(const QDomElement& root, QStringList& eulas)
{
    // EulaSection may appear at envelope level or inside VirtualSystem
    // Child element is <License> containing the EULA text
    QDomNodeList eulaSections;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        eulaSections = root.ownerDocument().elementsByTagNameNS(ns, "EulaSection");
        if (!eulaSections.isEmpty()) break;
    }
    if (eulaSections.isEmpty())
        eulaSections = root.ownerDocument().elementsByTagName("EulaSection");

    for (int i = 0; i < eulaSections.count(); ++i)
    {
        QDomElement section = eulaSections.at(i).toElement();
        // Look for <License> child
        QDomNodeList licenseNodes = section.elementsByTagName("License");
        if (licenseNodes.isEmpty())
        {
            // Try namespace-aware
            for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                                       QString("http://schemas.citrix.com/ovf/envelope/1")})
            {
                licenseNodes = section.elementsByTagNameNS(ns, "License");
                if (!licenseNodes.isEmpty()) break;
            }
        }
        if (!licenseNodes.isEmpty())
        {
            const QString text = licenseNodes.at(0).toElement().text().trimmed();
            if (!text.isEmpty() && !eulas.contains(text))
                eulas << text;
        }
    }
}

void OvfPackage::collectVirtualSystemNames(const QDomElement& root, QStringList& names)
{
    // VirtualSystem elements may be direct children of Envelope, or nested inside
    // VirtualSystemCollection for multi-VM appliances.
    QDomNodeList vsList;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        vsList = root.ownerDocument().elementsByTagNameNS(ns, "VirtualSystem");
        if (!vsList.isEmpty()) break;
    }
    if (vsList.isEmpty())
        vsList = root.ownerDocument().elementsByTagName("VirtualSystem");

    for (int i = 0; i < vsList.count(); ++i)
    {
        QDomElement vs = vsList.at(i).toElement();

        // Try <Name> child element first
        QDomNodeList nameNodes = vs.elementsByTagName("Name");
        if (nameNodes.isEmpty())
        {
            for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                                       QString("http://schemas.citrix.com/ovf/envelope/1")})
            {
                nameNodes = vs.elementsByTagNameNS(ns, "Name");
                if (!nameNodes.isEmpty()) break;
            }
        }

        if (!nameNodes.isEmpty())
        {
            const QString name = nameNodes.at(0).toElement().text().trimmed();
            if (!name.isEmpty())
            {
                names << name;
                continue;
            }
        }

        // Fall back to ovf:id attribute
        QString id = firstAttributeByLocalName(vs, "id");
        if (!id.isEmpty())
            names << id;
    }
}

// ─── RASD virtual hardware parsing ───────────────────────────────────────────

// CIM 28 ResourceType values used in OVF RASD elements
namespace RasdType
{
    static const int CPU           = 3;
    static const int Memory        = 4;
    static const int Ethernet      = 10;
    static const int DiskDrive     = 17;
    static const int StorageExtent = 19;
    static const int VirtualDisk   = 22;
}

// Return the text content of the first child element matching any of the given local names.
// Handles namespace-prefixed elements (e.g. "rasd:VirtualQuantity", "VirtualQuantity").
static QString rasdChildText(const QDomElement& rasdItem, const QString& localName)
{
    QDomNodeList children = rasdItem.childNodes();
    for (int i = 0; i < children.count(); ++i)
    {
        QDomElement child = children.at(i).toElement();
        if (child.isNull())
            continue;
        if (child.localName() == localName)
            return child.text().trimmed();
    }
    return QString();
}

OvfVirtualHardware OvfPackage::collectVirtualHardware(const QDomElement& virtualSystemElem,
                                                        const QMap<QString,qint64>& capacitiesByRef)
{
    OvfVirtualHardware hw;

    // VirtualHardwareSection may use various namespace prefixes
    QDomNodeList hwSections;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        hwSections = virtualSystemElem.elementsByTagNameNS(ns, "VirtualHardwareSection");
        if (!hwSections.isEmpty()) break;
    }
    if (hwSections.isEmpty())
        hwSections = virtualSystemElem.elementsByTagName("VirtualHardwareSection");

    for (int si = 0; si < hwSections.count(); ++si)
    {
        QDomElement hwSection = hwSections.at(si).toElement();

        // Collect all Item (RASD) elements
        QDomNodeList items;
        for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                                   QString("http://schemas.citrix.com/ovf/envelope/1"),
                                   QString("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData")})
        {
            items = hwSection.elementsByTagNameNS(ns, "Item");
            if (!items.isEmpty()) break;
        }
        if (items.isEmpty())
            items = hwSection.elementsByTagName("Item");

        for (int ii = 0; ii < items.count(); ++ii)
        {
            QDomElement item = items.at(ii).toElement();

            const QString typeStr = rasdChildText(item, "ResourceType");
            if (typeStr.isEmpty())
                continue;
            bool ok = false;
            const int resourceType = typeStr.toInt(&ok);
            if (!ok)
                continue;

            const QString quantity  = rasdChildText(item, "VirtualQuantity");
            const QString allocUnit = rasdChildText(item, "AllocationUnits").toLower();

            if (resourceType == RasdType::CPU && hw.vcpuCount == 0)
            {
                const int q = quantity.toInt();
                if (q > 0)
                    hw.vcpuCount = q;
            }
            else if (resourceType == RasdType::Memory && hw.memoryBytes == 0)
            {
                const qint64 q = quantity.toLongLong(&ok);
                if (ok && q > 0)
                {
                    // AllocationUnits examples: "MegaBytes", "byte * 2^20", "GigaBytes"
                    if (allocUnit.contains("giga") || allocUnit.contains("2^30"))
                        hw.memoryBytes = q * 1024LL * 1024 * 1024;
                    else if (allocUnit.contains("kilo") || allocUnit.contains("2^10"))
                        hw.memoryBytes = q * 1024LL;
                    else if (allocUnit.contains("byte") && !allocUnit.contains("mega") && !allocUnit.contains("kilo"))
                        hw.memoryBytes = q;
                    else
                        hw.memoryBytes = q * 1024LL * 1024; // default: MB
                }
            }
            else if (resourceType == RasdType::Ethernet)
            {
                hw.ethernetCount++;
            }
            else if (resourceType == RasdType::VirtualDisk || resourceType == RasdType::DiskDrive)
            {
                // HostResource attribute links to a disk ref like "ovf:/disk/disk1"
                const QString hostRes = rasdChildText(item, "HostResource");
                // Strip "ovf:/disk/" prefix if present
                const QString diskId = hostRes.section('/', -1);
                if (!diskId.isEmpty() && capacitiesByRef.contains(diskId))
                    hw.diskSizesByHref.insert(diskId, capacitiesByRef.value(diskId));
            }
        }
    }

    return hw;
}

void OvfPackage::parseFromOvfXml(const QString& xml){
    this->descriptorXml_ = xml;

    QDomDocument doc;
    QString errorMsg;
    int errorLine = 0, errorCol = 0;
    if (!doc.setContent(xml, true, &errorMsg, &errorLine, &errorCol))
    {
        this->parseError_ = QString("OVF XML parse error at line %1 col %2: %3")
                             .arg(errorLine).arg(errorCol).arg(errorMsg);
        return;
    }

    QDomElement root = doc.documentElement();
    if (root.isNull())
    {
        this->parseError_ = "OVF XML has no root element.";
        return;
    }

    // ── NetworkSection/Network ────────────────────────────────────────────────
    QDomNodeList networkSections;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        networkSections = doc.elementsByTagNameNS(ns, "NetworkSection");
        if (!networkSections.isEmpty()) break;
    }
    if (networkSections.isEmpty())
        networkSections = doc.elementsByTagName("NetworkSection");

    for (int i = 0; i < networkSections.count(); ++i)
    {
        QDomElement section = networkSections.at(i).toElement();
        QDomNodeList networks = section.childNodes();
        for (int j = 0; j < networks.count(); ++j)
        {
            QDomElement net = networks.at(j).toElement();
            if (net.isNull()) continue;
            if (net.localName() != "Network") continue;

            const QString name = firstAttributeByLocalName(net, "name");
            if (!name.isEmpty() && !this->networkNames_.contains(name))
                this->networkNames_ << name;
        }
    }

    // ── References/File ───────────────────────────────────────────────────────
    QDomNodeList refSections;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        refSections = doc.elementsByTagNameNS(ns, "References");
        if (!refSections.isEmpty()) break;
    }
    if (refSections.isEmpty())
        refSections = doc.elementsByTagName("References");

    for (int i = 0; i < refSections.count(); ++i)
    {
        QDomElement section = refSections.at(i).toElement();
        QDomNodeList files = section.childNodes();
        for (int j = 0; j < files.count(); ++j)
        {
            QDomElement fileElem = files.at(j).toElement();
            if (fileElem.isNull() || fileElem.localName() != "File") continue;

            const QString href = firstAttributeByLocalName(fileElem, "href");
            if (!href.isEmpty() && !this->diskFileRefs_.contains(href))
                this->diskFileRefs_ << href;
        }
    }

    // ── VirtualSystem names ───────────────────────────────────────────────────
    collectVirtualSystemNames(root, this->virtualSystemNames_);

    // ── DiskSection capacities (used by RASD parsing) ─────────────────────────
    // DiskSection/Disk elements carry ovf:capacity attribute (in MB by default,
    // or in units specified by ovf:capacityAllocationUnits).
    QMap<QString, qint64> diskCapacities; // diskId → bytes
    QDomNodeList diskSections;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        diskSections = doc.elementsByTagNameNS(ns, "DiskSection");
        if (!diskSections.isEmpty()) break;
    }
    if (diskSections.isEmpty())
        diskSections = doc.elementsByTagName("DiskSection");

    for (int i = 0; i < diskSections.count(); ++i)
    {
        QDomElement section = diskSections.at(i).toElement();
        QDomNodeList disks = section.childNodes();
        for (int j = 0; j < disks.count(); ++j)
        {
            QDomElement disk = disks.at(j).toElement();
            if (disk.isNull() || disk.localName() != "Disk") continue;

            const QString diskId   = firstAttributeByLocalName(disk, "diskId");
            const QString capStr   = firstAttributeByLocalName(disk, "capacity");
            const QString capUnits = firstAttributeByLocalName(disk, "capacityAllocationUnits").toLower();

            bool ok = false;
            const qint64 cap = capStr.toLongLong(&ok);
            if (!ok || diskId.isEmpty()) continue;

            qint64 bytes = cap;
            if (capUnits.contains("giga") || capUnits.contains("2^30"))
                bytes = cap * 1024LL * 1024 * 1024;
            else if (capUnits.contains("mega") || capUnits.contains("2^20") || capUnits.isEmpty())
                bytes = cap * 1024LL * 1024;
            else if (capUnits.contains("kilo") || capUnits.contains("2^10"))
                bytes = cap * 1024LL;
            // else assume bytes

            diskCapacities.insert(diskId, bytes);
        }
    }

    // ── Virtual hardware per system ───────────────────────────────────────────
    QDomNodeList vsList;
    for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                               QString("http://schemas.citrix.com/ovf/envelope/1")})
    {
        vsList = doc.elementsByTagNameNS(ns, "VirtualSystem");
        if (!vsList.isEmpty()) break;
    }
    if (vsList.isEmpty())
        vsList = doc.elementsByTagName("VirtualSystem");

    for (int i = 0; i < vsList.count(); ++i)
    {
        QDomElement vs = vsList.at(i).toElement();

        // Determine system name (match logic from collectVirtualSystemNames)
        QString vsName;
        QDomNodeList nameNodes = vs.elementsByTagName("Name");
        if (nameNodes.isEmpty())
        {
            for (const QString& ns : {QString("http://schemas.dmtf.org/ovf/envelope/1"),
                                       QString("http://schemas.citrix.com/ovf/envelope/1")})
            {
                nameNodes = vs.elementsByTagNameNS(ns, "Name");
                if (!nameNodes.isEmpty()) break;
            }
        }
        if (!nameNodes.isEmpty())
            vsName = nameNodes.at(0).toElement().text().trimmed();
        if (vsName.isEmpty())
            vsName = firstAttributeByLocalName(vs, "id");

        if (vsName.isEmpty())
            vsName = QString("VM %1").arg(i + 1);

        const OvfVirtualHardware hw = collectVirtualHardware(vs, diskCapacities);
        this->virtualHardwareBySystem_.insert(vsName, hw);
    }

    // ── EULA sections ─────────────────────────────────────────────────────────
    collectEulas(root, this->eulas_);

    // ── Encryption detection ─────────────────────────────────────────────────
    // SecuritySection_Type is a Xen/Citrix OVF extension. Also check for
    // xenovf:Encryption elements as a broad heuristic.
    QDomNodeList secNodes = doc.elementsByTagName("SecuritySection");
    if (secNodes.isEmpty())
    {
        for (const QString& ns : {QString("http://schemas.citrix.com/ovf/envelope/1"),
                                   QString("http://schemas.dmtf.org/ovf/envelope/1")})
        {
            secNodes = doc.elementsByTagNameNS(ns, "SecuritySection");
            if (!secNodes.isEmpty()) break;
        }
    }
    if (secNodes.isEmpty())
    {
        // Also check raw XML text as a safety net
        this->hasEncryption_ = xml.contains("SecuritySection", Qt::CaseInsensitive)
                             || xml.contains("Encryption",     Qt::CaseInsensitive);
    }
    else
    {
        this->hasEncryption_ = true;
    }

    this->valid_ = true;
    qDebug() << "OvfPackage: Parsed" << this->packageName_
             << "| VMs:" << this->virtualSystemNames_.size()
             << "| Networks:" << this->networkNames_.size()
             << "| Disks:" << this->diskFileRefs_.size()
             << "| EULAs:" << this->eulas_.size()
             << "| Encrypted:" << this->hasEncryption_;
}
