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

#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#include "../xenlib_global.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class Search;
class XenConnection;

/**
 * The saved searches: the built-in ones and those stored in each pool's gui_config
 *
 * Qt port of the static search list of C# XenAdmin.XenSearch.Search (Searches, SearchesChanged,
 * InitSearch, SynchroniseSearches, InitDefaultSearches). Searches are kept as their XML; each
 * CreateSearch() returns a new Search, so a later resynchronisation never invalidates a search in
 * use. Saved searches are interchangeable with XenCenter / XCP-ng Center (same gui_config keys).
 */
class XENLIB_EXPORT SearchManager : public QObject
{
    Q_OBJECT

    public:
        struct Entry
        {
            QString uuid;
            QString name;
            bool isDefault = false;
            QPointer<XenConnection> connection; // the pool it is saved on (null for built-in ones)
            QString xml;
        };

        static SearchManager* instance();

        //! C# SearchPrefix: gui_config key prefix of saved searches
        static QString SearchPrefix();
        static QString GuiConfigKey(const QString& uuid);

        //! Built-in searches first, then saved ones, each sorted by name
        QList<Entry> GetSearches() const;
        bool Contains(const QString& uuid) const;

        //! A new Search (owned by the caller) for an entry, or null
        Search* CreateSearch(const QString& uuid) const;

        //! The connections whose pool holds a saved search with this UUID
        QList<XenConnection*> ConnectionsHolding(const QString& uuid) const;

    signals:
        void SearchesChanged();

    private slots:
        void onConnectionAdded(XenConnection* connection);
        void scheduleSynchronise();
        void synchronise();

    private:
        SearchManager();
        void initDefaultSearches();
        void watch(XenConnection* connection);

        QList<Entry> defaults_;
        QList<Entry> saved_;
        QTimer synchroniseTimer_;
};

#endif // SEARCHMANAGER_H
