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
#include <QSharedPointer>

class XenConnection;
class XenRpcAPI;
class XenObject;

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
        enum class Type
        {
            Unknown,
            General,
            Memory,
            VmStorage,
            SrStorage,
            PhysicalStorage,
            Network,
            Nics,
            Performance,
            Snapshots,
            BootOptions,
            Console,
            CvmConsole,
            Search
        };

        explicit BaseTabPage(QWidget* parent = nullptr);
        virtual ~BaseTabPage();

        /**
         * Set the XenLib object for this tab page
         */
        virtual void SetObject(QSharedPointer<XenObject> object);

        /**
         * Called when the tab page becomes visible.
         * Override to implement lazy loading or start updates.
         */
        virtual void OnPageShown();

        /**
         * Called when the tab page is hidden.
         * Override to stop updates or clean up resources.
         */
        virtual void OnPageHidden();

        /**
         * Get the title for this tab page.
         */
        virtual QString GetTitle() const = 0;

        /**
         * Get a stable identifier for this tab page.
         */
        virtual Type GetType() const = 0;

        /**
         * Get the help ID for this tab page.
         */
        virtual QString HelpID() const
        {
            return "";
        }

        /**
         * Check if this tab page is applicable for the given object type.
         */
        virtual bool IsApplicableForObjectType(const QString& objectType) const = 0;

    protected:
        QString m_objectType;
        QString m_objectRef;
        QSharedPointer<XenObject> m_object;
        QVariantMap m_objectData;
        XenConnection* m_connection = nullptr;

        /**
         * Refresh the tab page content with current object data.
         * Override to implement tab-specific display logic.
         */
        virtual void refreshContent();

        //! Called before object is replaced while old refs to old connection still exist
        //! tabs that created some cache connections should use this to remove them
        virtual void removeObject();

        //! Refresh all signals when object changes
        virtual void updateObject();
};

#endif // BASETABPAGE_H
