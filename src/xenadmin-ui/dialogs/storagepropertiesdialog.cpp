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

#include "storagepropertiesdialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"

StoragePropertiesDialog::StoragePropertiesDialog(XenConnection* connection,
                                                 const QString& srRef,
                                                 QWidget* parent)
    : VerticallyTabbedDialog(connection, srRef, "SR", parent)
{
    setWindowTitle(tr("Storage Properties"));
    resize(700, 550);
    build();
}

void StoragePropertiesDialog::build()
{
    // General tab (name, description, tags, folder)
    showTab(new GeneralEditPage());

    // Custom Fields tab
    showTab(new CustomFieldsDisplayPage());

    // Performance Alerts tab
    showTab(new PerfmonAlertEditPage());

    // TODO: Add SR-specific pages from C# XenAdmin.Dialogs.PropertiesDialog:
    //
    // - SrReadCachingEditPage (line 247) - Read caching settings
    //   Conditional: SR.SupportsReadCaching() && !RestrictReadCaching
    //   See XenAdmin/SettingsPages/SrReadCachingEditPage.cs
    //
    // Notes:
    // - Read caching is only for certain SR types (NFS, EXT, etc.)
    // - Check SR capabilities via Helper methods
    // - Feature may be restricted by license
}
