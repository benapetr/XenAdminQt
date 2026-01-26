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
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/utils/misc.h"
#include <QStandardItemModel>
#include <QSignalBlocker>
#include <algorithm>

namespace
{
bool isSrVisibleToHost(const QSharedPointer<SR>& sr, const QString& hostRef)
{
    if (hostRef.isEmpty())
        return true;
    if (!sr)
        return false;

    if (sr->IsShared())
        return true;

    const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
    for (const QSharedPointer<PBD>& pbd : pbds)
    {
        if (pbd && pbd->GetHostRef() == hostRef)
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
    QSharedPointer<VM> vm;
    if (!this->m_vmRef.isEmpty())
    {
        vm = cache->ResolveObject<VM>("vm", this->m_vmRef);
        if (vm && vm->GetPowerState() == "Running")
        {
            hostRef = vm->GetResidentOnRef();
        }
        else if (vm)
        {
            hostRef = VMHelpers::getVMStorageHost(this->m_connection, vm->GetData(), true);
        }
    }

    bool stockholmOrGreater = false;
    if (this->m_connection->GetSession())
    {
        stockholmOrGreater = this->m_connection->GetSession()->ApiVersionMeets(APIVersion::API_2_11);
    }

    struct SrEntry
    {
        QString name;
        QSharedPointer<SR> sr;
    };

    QList<SrEntry> srEntries;
    const QStringList srRefs = cache->GetAllRefs("sr");
    for (const QString& srRef : srRefs)
    {
        QSharedPointer<SR> sr = cache->ResolveObject<SR>("sr", srRef);
        if (!sr || !sr->IsValid())
            continue;

        if (!sr->IsISOLibrary())
            continue;

        if (sr->IsBroken() && this->m_vmRef.isEmpty())
            continue;

        if (!isSrVisibleToHost(sr, hostRef))
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

        QString name = sr->GetName();
        if (name.isEmpty())
            name = tr("ISO SR");

        srEntries.append({name, sr});
    }

    std::sort(srEntries.begin(), srEntries.end(),
              [](const SrEntry& a, const SrEntry& b) {
                  return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
              });

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());

    for (const SrEntry& srEntry : srEntries)
    {
        QList<QPair<QString, QString>> vdiEntries;
        const QList<QSharedPointer<VDI>> vdis = srEntry.sr->GetVDIs();
        bool toolsSr = srEntry.sr->IsToolsSR();
        bool toolsIsoOnly = toolsSr && !stockholmOrGreater;

        if (vdis.isEmpty())
            continue;

        this->addItem(srEntry.name, QString());
        if (model)
        {
            QStandardItem* item = model->item(this->count() - 1);
            if (item)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }

        for (const QSharedPointer<VDI>& vdi : vdis)
        {
            if (!vdi || !vdi->IsValid())
                continue;

            if (!showHidden && vdi->GetData().value("is_hidden", false).toBool())
                continue;

            if (vdi->IsSnapshot())
                continue;

            const QString name = vdi->GetName();
            if (name.isEmpty())
                continue;

            if (toolsIsoOnly)
            {
                if (!vdi->IsToolsIso())
                    continue;
            }

            vdiEntries.append({name, vdi->OpaqueRef()});
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
