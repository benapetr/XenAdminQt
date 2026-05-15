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

#include "ovfwriter.h"

#include <QXmlStreamWriter>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QTextStream>
#include <QByteArray>
#include <cstring>

// ── OVF namespace constants ──────────────────────────────────────────────────

static const QString NS_OVF   = QStringLiteral("http://schemas.dmtf.org/ovf/envelope/1");
static const QString NS_RASD  = QStringLiteral("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData");
static const QString NS_VSSD  = QStringLiteral("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData");
static const QString NS_XEN   = QStringLiteral("http://www.citrix.com/xencenter/2009/xen");
static const QString NS_XSI   = QStringLiteral("http://www.w3.org/2001/XMLSchema-instance");

// Format URI used by XenCenter for VHD files (matches C# XenOvfApi)
static const QString VHD_FORMAT_URI = QStringLiteral("http://www.vmware.com/interfaces/specifications/vmdk.html#streamOptimized");

// ── XML generation ───────────────────────────────────────────────────────────

QString OvfWriter::generateXml(const OvfEnvelopeData& envelope) const
{
    QString xmlText;
    QXmlStreamWriter xml(&xmlText);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument();
    xml.writeStartElement(NS_OVF, QStringLiteral("Envelope"));
    xml.writeNamespace(NS_OVF,  QStringLiteral("ovf"));
    xml.writeNamespace(NS_RASD, QStringLiteral("rasd"));
    xml.writeNamespace(NS_VSSD, QStringLiteral("vssd"));
    xml.writeNamespace(NS_XEN,  QStringLiteral("xen"));
    xml.writeNamespace(NS_XSI,  QStringLiteral("xsi"));

    this->writeReferences(xml, envelope);
    this->writeDiskSection(xml, envelope);
    this->writeNetworkSection(xml, envelope);
    this->writeEulaSections(xml, envelope);

    for (const OvfVirtualSystemEntry& sys : envelope.systems)
        this->writeVirtualSystem(xml, sys);

    xml.writeEndElement(); // Envelope
    xml.writeEndDocument();
    return xmlText;
}

bool OvfWriter::saveToFile(const OvfEnvelopeData& envelope, const QString& ovfFilePath) const
{
    const QString xml = this->generateXml(envelope);
    QFile f(ovfFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << xml;
    return true;
}

// ── Private: XML section writers ────────────────────────────────────────────

void OvfWriter::writeReferences(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const
{
    xml.writeStartElement(NS_OVF, QStringLiteral("References"));
    for (const OvfVirtualSystemEntry& sys : env.systems)
    {
        for (const OvfDiskEntry& disk : sys.disks)
        {
            if (disk.isCdrom)
                continue;
            xml.writeStartElement(NS_OVF, QStringLiteral("File"));
            xml.writeAttribute(NS_OVF, QStringLiteral("id"),   disk.fileId);
            xml.writeAttribute(NS_OVF, QStringLiteral("href"), disk.href);
            xml.writeEndElement();
        }
    }
    xml.writeEndElement(); // References
}

void OvfWriter::writeDiskSection(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const
{
    xml.writeStartElement(NS_OVF, QStringLiteral("DiskSection"));
    xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                         QStringLiteral("Virtual disk information"));
    for (const OvfVirtualSystemEntry& sys : env.systems)
    {
        for (const OvfDiskEntry& disk : sys.disks)
        {
            if (disk.isCdrom)
                continue;
            xml.writeStartElement(NS_OVF, QStringLiteral("Disk"));
            xml.writeAttribute(NS_OVF, QStringLiteral("capacity"),
                               QString::number(disk.virtualBytes));
            xml.writeAttribute(NS_OVF, QStringLiteral("capacityAllocationUnits"),
                               QStringLiteral("byte"));
            xml.writeAttribute(NS_OVF, QStringLiteral("diskId"),  disk.diskId);
            xml.writeAttribute(NS_OVF, QStringLiteral("fileRef"), disk.fileId);
            xml.writeAttribute(NS_OVF, QStringLiteral("format"),  VHD_FORMAT_URI);
            xml.writeAttribute(NS_OVF, QStringLiteral("populatedSize"),
                               QString::number(disk.physicalBytes));
            xml.writeEndElement(); // Disk
        }
    }
    xml.writeEndElement(); // DiskSection
}

void OvfWriter::writeNetworkSection(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const
{
    // Collect unique network names across all VMs
    QStringList seen;
    QList<OvfNetworkEntry> allNets;
    for (const OvfVirtualSystemEntry& sys : env.systems)
    {
        for (const OvfNetworkEntry& net : sys.networks)
        {
            if (!seen.contains(net.name))
            {
                seen << net.name;
                allNets << net;
            }
        }
    }
    if (allNets.isEmpty())
        return;

    xml.writeStartElement(NS_OVF, QStringLiteral("NetworkSection"));
    xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                         QStringLiteral("Network logical information"));
    for (const OvfNetworkEntry& net : allNets)
    {
        xml.writeStartElement(NS_OVF, QStringLiteral("Network"));
        xml.writeAttribute(NS_OVF, QStringLiteral("name"), net.name);
        if (!net.description.isEmpty())
            xml.writeTextElement(NS_OVF, QStringLiteral("Description"), net.description);
        xml.writeEndElement(); // Network
    }
    xml.writeEndElement(); // NetworkSection
}

void OvfWriter::writeEulaSections(QXmlStreamWriter& xml, const OvfEnvelopeData& env) const
{
    for (const QString& text : env.eulaTexts)
    {
        xml.writeStartElement(NS_OVF, QStringLiteral("EulaSection"));
        xml.writeAttribute(NS_OVF, QStringLiteral("required"), QStringLiteral("false"));
        xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                             QStringLiteral("End User License Agreement"));
        xml.writeStartElement(NS_OVF, QStringLiteral("License"));
        xml.writeAttribute(QStringLiteral("xml:lang"), QStringLiteral("en"));
        xml.writeCharacters(text);
        xml.writeEndElement(); // License
        xml.writeEndElement(); // EulaSection
    }
}

void OvfWriter::writeVirtualSystem(QXmlStreamWriter& xml, const OvfVirtualSystemEntry& sys) const
{
    xml.writeStartElement(NS_OVF, QStringLiteral("VirtualSystem"));
    xml.writeAttribute(NS_OVF, QStringLiteral("id"), sys.id);
    xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                         QStringLiteral("A virtual machine"));
    xml.writeTextElement(NS_OVF, QStringLiteral("Name"), sys.name);

    // OperatingSystemSection
    xml.writeStartElement(NS_OVF, QStringLiteral("OperatingSystemSection"));
    xml.writeAttribute(NS_OVF, QStringLiteral("id"), QString::number(sys.osId));
    xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                         QStringLiteral("Operating system"));
    if (!sys.osName.isEmpty())
        xml.writeTextElement(NS_OVF, QStringLiteral("Description"), sys.osName);
    xml.writeEndElement(); // OperatingSystemSection

    // VirtualHardwareSection
    xml.writeStartElement(NS_OVF, QStringLiteral("VirtualHardwareSection"));
    xml.writeTextElement(NS_OVF, QStringLiteral("Info"),
                         QStringLiteral("Virtual hardware requirements"));

    // System / VSSD
    xml.writeStartElement(NS_OVF, QStringLiteral("System"));
    xml.writeTextElement(NS_VSSD, QStringLiteral("ElementName"),
                         QStringLiteral("Virtual Hardware Family"));
    xml.writeTextElement(NS_VSSD, QStringLiteral("InstanceID"),
                         QStringLiteral("0"));
    xml.writeTextElement(NS_VSSD, QStringLiteral("VirtualSystemIdentifier"), sys.name);
    xml.writeTextElement(NS_VSSD, QStringLiteral("VirtualSystemType"),
                         sys.vmType.isEmpty() ? QStringLiteral("xen-3.0-unknown") : sys.vmType);
    xml.writeEndElement(); // System

    int instanceId = 1;

    // CPUs
    xml.writeStartElement(NS_OVF, QStringLiteral("Item"));
    xml.writeTextElement(NS_RASD, QStringLiteral("Description"),
                         QStringLiteral("Number of virtual CPUs"));
    xml.writeTextElement(NS_RASD, QStringLiteral("ElementName"),
                         QStringLiteral("%1 virtual CPU(s)").arg(sys.vcpuAtStartup));
    xml.writeTextElement(NS_RASD, QStringLiteral("InstanceID"),
                         QString::number(instanceId++));
    xml.writeTextElement(NS_RASD, QStringLiteral("ResourceType"),
                         QStringLiteral("3"));
    xml.writeTextElement(NS_RASD, QStringLiteral("VirtualQuantity"),
                         QString::number(sys.vcpuAtStartup));
    xml.writeEndElement(); // Item (CPU)

    // Memory
    xml.writeStartElement(NS_OVF, QStringLiteral("Item"));
    xml.writeTextElement(NS_RASD, QStringLiteral("AllocationUnits"),
                         QStringLiteral("MegaBytes"));
    xml.writeTextElement(NS_RASD, QStringLiteral("Description"),
                         QStringLiteral("Memory Size"));
    xml.writeTextElement(NS_RASD, QStringLiteral("ElementName"),
                         QStringLiteral("%1 MB of memory").arg(sys.memoryMB));
    xml.writeTextElement(NS_RASD, QStringLiteral("InstanceID"),
                         QString::number(instanceId++));
    xml.writeTextElement(NS_RASD, QStringLiteral("ResourceType"),
                         QStringLiteral("4"));
    xml.writeTextElement(NS_RASD, QStringLiteral("VirtualQuantity"),
                         QString::number(sys.memoryMB));
    xml.writeEndElement(); // Item (Memory)

    // NICs (ResourceType=10)
    for (const OvfNetworkEntry& net : sys.networks)
    {
        xml.writeStartElement(NS_OVF, QStringLiteral("Item"));
        xml.writeTextElement(NS_RASD, QStringLiteral("AutomaticAllocation"),
                             QStringLiteral("true"));
        xml.writeTextElement(NS_RASD, QStringLiteral("Connection"),  net.name);
        xml.writeTextElement(NS_RASD, QStringLiteral("Description"), net.description);
        xml.writeTextElement(NS_RASD, QStringLiteral("ElementName"), net.name);
        if (!net.mac.isEmpty())
            xml.writeTextElement(NS_RASD, QStringLiteral("Address"), net.mac);
        xml.writeTextElement(NS_RASD, QStringLiteral("InstanceID"),
                             QString::number(instanceId++));
        xml.writeTextElement(NS_RASD, QStringLiteral("ResourceType"),
                             QStringLiteral("10"));
        xml.writeEndElement(); // Item (NIC)
    }

    // Disks
    for (const OvfDiskEntry& disk : sys.disks)
    {
        xml.writeStartElement(NS_OVF, QStringLiteral("Item"));
        if (!disk.addressOnParent.isEmpty())
            xml.writeTextElement(NS_RASD, QStringLiteral("AddressOnParent"),
                                 disk.addressOnParent);
        xml.writeTextElement(NS_RASD, QStringLiteral("Description"),
                             disk.isCdrom ? QStringLiteral("CD-ROM drive") : disk.description);
        xml.writeTextElement(NS_RASD, QStringLiteral("ElementName"),
                             disk.isCdrom ? QStringLiteral("CD/DVD Drive") : disk.name);
        if (!disk.isCdrom)
            xml.writeTextElement(NS_RASD, QStringLiteral("HostResource"),
                                 QStringLiteral("ovf:/disk/%1").arg(disk.diskId));
        xml.writeTextElement(NS_RASD, QStringLiteral("InstanceID"),
                             QString::number(instanceId++));
        xml.writeTextElement(NS_RASD, QStringLiteral("ResourceType"),
                             disk.isCdrom ? QStringLiteral("15") : QStringLiteral("22"));
        xml.writeEndElement(); // Item (Disk/CDROM)
    }

    // Xen-specific config data (HVM_boot_params, platform, PV_*, etc.)
    // Written as xen:Data elements; matches C# OVF.AddOtherSystemSettingData()
    for (const QPair<QString, QString>& kv : sys.xenConfig)
    {
        xml.writeStartElement(NS_XEN, QStringLiteral("Data"));
        xml.writeAttribute(NS_OVF, QStringLiteral("required"), QStringLiteral("false"));
        xml.writeTextElement(NS_XEN, QStringLiteral("Name"),  kv.first);
        xml.writeTextElement(NS_XEN, QStringLiteral("Value"), kv.second);
        xml.writeEndElement(); // xen:Data
    }

    xml.writeEndElement(); // VirtualHardwareSection

    // Startup section (vApp ordering)
    if (sys.startOrder >= 0)
    {
        xml.writeStartElement(NS_XEN, QStringLiteral("StartupSection"));
        xml.writeAttribute(NS_OVF, QStringLiteral("required"), QStringLiteral("false"));
        xml.writeTextElement(NS_XEN, QStringLiteral("Info"),
                             QStringLiteral("vApp startup ordering"));
        xml.writeStartElement(NS_XEN, QStringLiteral("StartupItem"));
        xml.writeAttribute(NS_OVF, QStringLiteral("id"),     sys.id);
        xml.writeAttribute(NS_XEN, QStringLiteral("Order"),  QString::number(sys.startOrder));
        xml.writeAttribute(NS_XEN, QStringLiteral("StartDelay"),
                           QString::number(sys.startDelay));
        xml.writeAttribute(NS_XEN, QStringLiteral("StopDelay"),
                           QString::number(sys.shutdownDelay));
        xml.writeEndElement(); // StartupItem
        xml.writeEndElement(); // StartupSection
    }

    xml.writeEndElement(); // VirtualSystem
}

// ── Manifest ─────────────────────────────────────────────────────────────────

bool OvfWriter::createManifest(const OvfEnvelopeData& envelope,
                               const QString& packageDir,
                               const QString& ovfFileName) const
{
    // Build list of files to hash: .ovf first, then each VHD
    QStringList fileNames;
    fileNames << ovfFileName;
    for (const OvfVirtualSystemEntry& sys : envelope.systems)
        for (const OvfDiskEntry& disk : sys.disks)
            if (!disk.isCdrom)
                fileNames << disk.href;

    // Compute SHA256 for each existing file
    QStringList lines;
    for (const QString& name : fileNames)
    {
        const QString path = packageDir + QStringLiteral("/") + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&f))
            continue;

        const QString hex = QString::fromLatin1(hash.result().toHex());
        lines << QStringLiteral("SHA256 (%1)= %2").arg(name, hex);
    }

    const QString baseName = QFileInfo(ovfFileName).baseName();
    const QString mfPath   = packageDir + QStringLiteral("/") + baseName
                             + QStringLiteral(".mf");
    QFile mf(mfPath);
    if (!mf.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream ts(&mf);
    for (const QString& line : lines)
        ts << line << "\n";

    return true;
}

// ── OVA (POSIX TAR) packaging ────────────────────────────────────────────────

// Writes a 512-byte POSIX ustar header into buf for the given filename and size.
// Returns the octal checksum value (already embedded in buf).
static void fillTarHeader(char buf[512], const QString& name, qint64 fileSize)
{
    std::memset(buf, 0, 512);

    // name field: 100 bytes
    QByteArray nameBytes = name.toLocal8Bit();
    if (nameBytes.size() > 99)
        nameBytes = nameBytes.left(99);
    std::memcpy(buf, nameBytes.constData(), nameBytes.size());

    // mode: 0000644
    std::memcpy(buf + 100, "0000644\0", 8);

    // uid/gid
    std::memcpy(buf + 108, "0000000\0", 8);
    std::memcpy(buf + 116, "0000000\0", 8);

    // size: octal, 11 digits + space
    QByteArray sizeStr = QByteArray::number(fileSize, 8).rightJustified(11, '0');
    std::memcpy(buf + 124, sizeStr.constData(), 11);
    buf[135] = ' ';

    // mtime: 0
    std::memcpy(buf + 136, "00000000000\0", 12);

    // checksum placeholder: all spaces
    std::memset(buf + 148, ' ', 8);

    // type flag: '0' = regular file
    buf[156] = '0';

    // ustar magic + version
    std::memcpy(buf + 257, "ustar  \0", 8);

    // Calculate checksum over the 512-byte header with spaces in checksum field
    unsigned int checksum = 0;
    for (int i = 0; i < 512; ++i)
        checksum += static_cast<unsigned char>(buf[i]);

    QByteArray csStr = QByteArray::number(checksum, 8).rightJustified(6, '0');
    std::memcpy(buf + 148, csStr.constData(), 6);
    buf[154] = '\0';
    buf[155] = ' ';
}

bool OvfWriter::writeTarEntry(QFile& tarFile,
                              const QString& memberPath,
                              const QString& memberName) const
{
    QFile src(memberPath);
    if (!src.open(QIODevice::ReadOnly))
        return false;

    const qint64 fileSize = src.size();
    char header[512];
    fillTarHeader(header, memberName, fileSize);

    if (tarFile.write(header, 512) != 512)
        return false;

    // Write file data
    const qint64 BUF = 32768;
    qint64 written = 0;
    char copyBuf[BUF];
    while (written < fileSize)
    {
        const qint64 toRead = qMin(static_cast<qint64>(BUF), fileSize - written);
        const qint64 r = src.read(copyBuf, toRead);
        if (r <= 0)
            return false;
        if (tarFile.write(copyBuf, r) != r)
            return false;
        written += r;
    }

    // Pad to 512-byte boundary
    const qint64 remainder = fileSize % 512;
    if (remainder != 0)
    {
        char pad[512] = {};
        if (tarFile.write(pad, 512 - remainder) != 512 - remainder)
            return false;
    }

    return true;
}

bool OvfWriter::createOva(const OvfEnvelopeData& envelope,
                          const QString& packageDir,
                          const QString& ovfFileName,
                          const QString& ovaPath) const
{
    QFile ova(ovaPath);
    if (!ova.open(QIODevice::WriteOnly))
        return false;

    // OVF spec mandates: .ovf first, then disk files, then .mf, then .cert
    QStringList memberNames;
    memberNames << ovfFileName;
    for (const OvfVirtualSystemEntry& sys : envelope.systems)
        for (const OvfDiskEntry& disk : sys.disks)
            if (!disk.isCdrom)
                memberNames << disk.href;

    const QString baseName  = QFileInfo(ovfFileName).baseName();
    const QString mfFile    = baseName + QStringLiteral(".mf");
    const QString certFile  = baseName + QStringLiteral(".cert");

    const QString mfPath   = packageDir + QStringLiteral("/") + mfFile;
    const QString certPath = packageDir + QStringLiteral("/") + certFile;

    if (QFile::exists(mfPath))
        memberNames << mfFile;
    if (QFile::exists(certPath))
        memberNames << certFile;

    for (const QString& name : memberNames)
    {
        const QString path = packageDir + QStringLiteral("/") + name;
        if (!QFile::exists(path))
            continue;
        if (!this->writeTarEntry(ova, path, name))
            return false;
    }

    // Two 512-byte zero blocks mark end of archive
    char eofBlocks[1024] = {};
    ova.write(eofBlocks, 1024);

    return true;
}
