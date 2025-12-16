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

#ifndef BASETABPAGE_H
#define BASETABPAGE_H

#include <QWidget>
#include <QVariantMap>

class XenLib;
class XenRpcAPI;

/**
 * Base class for all tab pages in XenAdmin Qt.
 * Each tab page displays information about a specific Xen object type
 * (VM, Host, Pool, SR, Network, etc.) and updates dynamically when
 * the object's properties change.
 */
class BaseTabPage : public QWidget
{
    Q_OBJECT

public:
    explicit BaseTabPage(QWidget* parent = nullptr);
    virtual ~BaseTabPage();

    /**
     * Set the Xen object this tab page should display information about.
     * @param objectType The type of object ("vm", "host", "pool", "sr", "network", etc.)
     * @param objectRef The object reference from XenAPI
     * @param objectData Complete object data as a QVariantMap
     */
    virtual void setXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData);

    /**
     * Set the XenLib for this tab page.
     * This allows tab pages to access XenAPI and make API calls.
     */
    virtual void setXenLib(XenLib* xenLib);

    /**
     * Called when the tab page becomes visible.
     * Override to implement lazy loading or start updates.
     */
    virtual void onPageShown();

    /**
     * Called when the tab page is hidden.
     * Override to stop updates or clean up resources.
     */
    virtual void onPageHidden();

    /**
     * Get the title for this tab page.
     */
    virtual QString tabTitle() const = 0;

    /**
     * Get the help ID for this tab page.
     */
    virtual QString helpID() const
    {
        return "";
    }

    /**
     * Check if this tab page is applicable for the given object type.
     */
    virtual bool isApplicableForObjectType(const QString& objectType) const = 0;

protected:
    QString m_objectType;
    QString m_objectRef;
    QVariantMap m_objectData;
    XenLib* m_xenLib;

    /**
     * Refresh the tab page content with current object data.
     * Override to implement tab-specific display logic.
     */
    virtual void refreshContent();
};

#endif // BASETABPAGE_H
