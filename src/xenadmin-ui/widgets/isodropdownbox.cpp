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

#include "isodropdownbox.h"
#include "../settingsmanager.h"
#include "../../xenlib/xenlib.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/vmhelpers.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/apiversion.h"
#include <QStandardItemModel>
#include <QSignalBlocker>
#include <algorithm>

namespace
{
bool isToolsSr(const QVariantMap& srData)
{
    if (srData.value("is_tools_sr", false).toBool())
        return true;
    return srData.value("type").toString() == "udev";
}

bool isToolsIsoName(const QString& name)
{
    QString lower = name.toLower();
    return lower.contains("tools") && lower.endsWith(".iso");
}

bool isSrVisibleToHost(const QVariantMap& srData, XenCache* cache, const QString& hostRef)
{
    if (hostRef.isEmpty())
        return true;

    if (srData.value("shared").toBool())
        return true;

    const QVariantList pbds = srData.value("PBDs").toList();
    for (const QVariant& pbdRefVar : pbds)
    {
        const QString pbdRef = pbdRefVar.toString();
        if (pbdRef.isEmpty() || pbdRef == "OpaqueRef:NULL")
            continue;

        const QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (pbdData.value("host").toString() == hostRef)
            return true;
    }

    return false;
}

int naturalCompare(const QString& s1, const QString& s2)
{
    if (s1.compare(s2, Qt::CaseInsensitive) == 0)
        return 0;

    if (s1.isEmpty())
        return -1;
    if (s2.isEmpty())
        return 1;

    int i = 0;
    int len1 = s1.length();
    int len2 = s2.length();
    int minLen = qMin(len1, len2);

    while (i < minLen)
    {
        QChar c1 = s1[i];
        QChar c2 = s2[i];

        bool c1IsDigit = c1.isDigit();
        bool c2IsDigit = c2.isDigit();

        if (!c1IsDigit && !c2IsDigit)
        {
            int cmp = s1.mid(i, 1).compare(s2.mid(i, 1), Qt::CaseInsensitive);
            if (cmp != 0)
                return cmp;
            i++;
        } else if (c1IsDigit && c2IsDigit)
        {
            int j = i + 1;
            while (j < len1 && s1[j].isDigit())
                j++;
            int k = i + 1;
            while (k < len2 && s2[k].isDigit())
                k++;

            int numLen1 = j - i;
            int numLen2 = k - i;

            if (numLen1 != numLen2)
                return numLen1 - numLen2;

            QString num1 = s1.mid(i, numLen1);
            QString num2 = s2.mid(i, numLen2);
            int cmp = num1.compare(num2);
            if (cmp != 0)
                return cmp;

            i = j;
        } else
        {
            return c1IsDigit ? 1 : -1;
        }
    }

    return len1 - len2;
}
}

IsoDropDownBox::IsoDropDownBox(QWidget* parent)
    : QComboBox(parent), m_xenLib(nullptr)
{
    setEditable(false);
    setInsertPolicy(QComboBox::NoInsert);
}

void IsoDropDownBox::setXenLib(XenLib* xenLib)
{
    m_xenLib = xenLib;
}

void IsoDropDownBox::setVMRef(const QString& vmRef)
{
    m_vmRef = vmRef;
}

void IsoDropDownBox::refresh()
{
    QSignalBlocker blocker(this);
    clear();

    addItem(tr("<empty>"), QString());

    if (!m_xenLib)
        return;

    XenCache* cache = m_xenLib->getCache();
    if (!cache)
        return;

    bool showHidden = SettingsManager::instance().getShowHiddenObjects();

    QString hostRef;
    QVariantMap vmData;
    if (!m_vmRef.isEmpty())
    {
        vmData = cache->ResolveObjectData("vm", m_vmRef);
        QString powerState = vmData.value("power_state").toString();
        if (powerState == "Running")
        {
            hostRef = vmData.value("resident_on").toString();
        } else
        {
            hostRef = VMHelpers::getVMStorageHost(m_xenLib, vmData, true);
        }
    }

    bool stockholmOrGreater = false;
    XenConnection* connection = m_xenLib->getConnection();
    if (connection && connection->GetSession())
    {
        stockholmOrGreater = connection->GetSession()->apiVersionMeets(APIVersion::API_2_11);
    }

    struct SrEntry
    {
        QString name;
        QVariantMap data;
    };

    QList<SrEntry> srEntries;
    const QList<QVariantMap> allSrs = cache->GetAllData("sr");
    for (const QVariantMap& srData : allSrs)
    {
        if (srData.value("content_type").toString() != "iso")
            continue;

        if (srData.value("is_broken", false).toBool() && m_vmRef.isEmpty())
            continue;

        if (!isSrVisibleToHost(srData, cache, hostRef))
            continue;

        bool toolsSr = isToolsSr(srData);
        if (toolsSr && stockholmOrGreater)
            continue;

        QString name = srData.value("name_label").toString();
        if (name.isEmpty())
            name = tr("ISO SR");

        srEntries.append({name, srData});
    }

    std::sort(srEntries.begin(), srEntries.end(),
              [](const SrEntry& a, const SrEntry& b) {
                  return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
              });

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());

    for (const SrEntry& srEntry : srEntries)
    {
        addItem(srEntry.name, QString());
        if (model)
        {
            QStandardItem* item = model->item(count() - 1);
            if (item)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }

        QList<QPair<QString, QString>> vdiEntries;
        const QVariantList vdiRefs = srEntry.data.value("VDIs").toList();
        bool toolsSr = isToolsSr(srEntry.data);
        bool toolsIsoOnly = toolsSr && !stockholmOrGreater;

        for (const QVariant& vdiRefVar : vdiRefs)
        {
            const QString vdiRef = vdiRefVar.toString();
            if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                continue;

            const QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
            if (vdiData.isEmpty())
                continue;

            if (!showHidden && vdiData.value("is_hidden", false).toBool())
                continue;

            if (vdiData.value("is_a_snapshot", false).toBool())
                continue;

            const QString name = vdiData.value("name_label").toString();
            if (name.isEmpty())
                continue;

            if (toolsIsoOnly && !isToolsIsoName(name))
                continue;

            vdiEntries.append({name, vdiRef});
        }

        std::sort(vdiEntries.begin(), vdiEntries.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                      return naturalCompare(a.first, b.first) < 0;
                  });

        for (const auto& vdiEntry : vdiEntries)
        {
            addItem(QString("    %1").arg(vdiEntry.first), vdiEntry.second);
        }
    }
}

QString IsoDropDownBox::selectedVdiRef() const
{
    return currentData().toString();
}

void IsoDropDownBox::setSelectedVdiRef(const QString& vdiRef)
{
    if (vdiRef.isEmpty())
    {
        setCurrentIndex(0);
        return;
    }

    for (int i = 0; i < count(); ++i)
    {
        if (itemData(i).toString() == vdiRef)
        {
            setCurrentIndex(i);
            return;
        }
    }

    setCurrentIndex(0);
}
