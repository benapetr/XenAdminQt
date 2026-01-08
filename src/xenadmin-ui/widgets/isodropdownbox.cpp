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
#include "xenlib/xencache.h"
#include "xenlib/vmhelpers.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/apiversion.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/utils/misc.h"
#include <QStandardItemModel>
#include <QSignalBlocker>
#include <algorithm>

namespace
{
bool isToolsSr(XenCache* cache, const QVariantMap& srData)
{
    if (srData.value("is_tools_sr", false).toBool())
        return true;

    const QString srRef = srData.value("ref").toString();
    QSharedPointer<SR> sr = cache ? cache->ResolveObject<SR>("sr", srRef) : QSharedPointer<SR>();
    return sr && sr->IsToolsSR();
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
}

IsoDropDownBox::IsoDropDownBox(QWidget* parent) : QComboBox(parent), m_connection(nullptr)
{
    this->setEditable(false);
    this->setInsertPolicy(QComboBox::NoInsert);
}

void IsoDropDownBox::SetConnection(XenConnection* connection)
{
    this->m_connection = connection;
} 

void IsoDropDownBox::SetVMRef(const QString& vmRef)
{
    this->m_vmRef = vmRef;
} 

void IsoDropDownBox::Refresh()
{
    QSignalBlocker blocker(this);
    this->clear();

    this->addItem(tr("<empty>"), QString());

    if (!this->m_connection)
        return;

    XenCache* cache = this->m_connection->GetCache();

    bool showHidden = SettingsManager::instance().getShowHiddenObjects();

    QString hostRef;
    QVariantMap vmData;
    if (!this->m_vmRef.isEmpty())
    {
        vmData = cache->ResolveObjectData("vm", this->m_vmRef);
        QString powerState = vmData.value("power_state").toString();
        if (powerState == "Running")
        {
            hostRef = vmData.value("resident_on").toString();
        } else
        {
            hostRef = VMHelpers::getVMStorageHost(this->m_connection, vmData, true);
        }
    }

    bool stockholmOrGreater = false;
    if (this->m_connection->GetSession())
    {
        stockholmOrGreater = this->m_connection->GetSession()->apiVersionMeets(APIVersion::API_2_11);
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

        if (srData.value("is_broken", false).toBool() && this->m_vmRef.isEmpty())
            continue;

        if (!isSrVisibleToHost(srData, cache, hostRef))
            continue;

        // NOTE: C# hides Tools SRs on Stockholm+ (Citrix Xen 8.0+) in the ISO picker.
        // For XCP-ng, the tools SR is often named "XCP-ng Tools" and may not be flagged
        // as tools in the same way as Citrix ("XenServer Tools"/is_tools_sr). When we
        // treated this SR as tools, the Stockholm+ filter hid it entirely, so
        // guest-tools.iso disappeared from the picker. We keep this disabled to
        // preserve access to guest-tools.iso until this logic is fixed.

        //bool toolsSr = isToolsSr(cache, srData);
        //if (toolsSr && stockholmOrGreater)
        //    continue;

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
        QList<QPair<QString, QString>> vdiEntries;
        const QVariantList vdiRefs = srEntry.data.value("VDIs").toList();
        bool toolsSr = isToolsSr(cache, srEntry.data);
        bool toolsIsoOnly = toolsSr && !stockholmOrGreater;

        if (vdiRefs.isEmpty())
            continue;

        this->addItem(srEntry.name, QString());
        if (model)
        {
            QStandardItem* item = model->item(this->count() - 1);
            if (item)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }

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

            if (toolsIsoOnly)
            {
                QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>("vdi", vdiRef);
                if (!vdi || !vdi->IsToolsIso())
                    continue;
            }

            vdiEntries.append({name, vdiRef});
        }

        std::sort(vdiEntries.begin(), vdiEntries.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                      return Misc::NaturalCompare(a.first, b.first) < 0;
                  });

        for (const auto& vdiEntry : vdiEntries)
        {
            this->addItem(QString("    %1").arg(vdiEntry.first), vdiEntry.second);
        }
    }
}

QString IsoDropDownBox::SelectedVdiRef() const
{
    return this->currentData().toString();
}

void IsoDropDownBox::SetSelectedVdiRef(const QString& vdiRef)
{
    if (vdiRef.isEmpty())
    {
        this->setCurrentIndex(0);
        return;
    }

    for (int i = 0; i < this->count(); ++i)
    {
        if (this->itemData(i).toString() == vdiRef)
        {
            this->setCurrentIndex(i);
            return;
        }
    }

    this->setCurrentIndex(0);
}
