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

#include "navigationhistory.h"
#include "../mainwindow.h"
#include <QMenu>
#include <QDebug>
#include <QTimer>

// XenModelObjectHistoryItem implementation

XenModelObjectHistoryItem::XenModelObjectHistoryItem(const QString& objectRef,
                                                     const QString& objectType,
                                                     const QString& objectName,
                                                     const QIcon& objectIcon,
                                                     const QString& tabName)
    : m_objectRef(objectRef), m_objectType(objectType), m_objectName(objectName), m_objectIcon(objectIcon), m_tabName(tabName)
{
}

void XenModelObjectHistoryItem::go()
{
    // Navigate to this object and tab (matches C# XenModelObjectHistoryItem.Go)
    // The MainWindow pointer is available through NavigationHistory
    qDebug() << "NavigationHistory: Navigate to" << this->m_objectName << "tab:" << this->m_tabName;

    // This will be called by NavigationHistory::doHistoryItem which has access to MainWindow
    // The actual navigation happens there
}

QString XenModelObjectHistoryItem::name() const
{
    // Format: "ObjectName, (TabName)" - matches C# format
    QString displayName = this->m_objectName.isEmpty() ? "XenAdmin Qt" : this->m_objectName;
    return QString("%1, (%2)").arg(displayName, this->m_tabName);
}

QIcon XenModelObjectHistoryItem::icon() const
{
    return this->m_objectIcon;
}

bool XenModelObjectHistoryItem::equals(const HistoryItem* other) const
{
    const XenModelObjectHistoryItem* otherXen = dynamic_cast<const XenModelObjectHistoryItem*>(other);
    if (!otherXen)
        return false;

    // Items are equal if they refer to same object and same tab
    if (otherXen->m_tabName != this->m_tabName)
        return false;

    // Both null refs means home/overview
    if (otherXen->m_objectRef.isEmpty() && this->m_objectRef.isEmpty())
        return true;

    if (otherXen->m_objectRef.isEmpty() || this->m_objectRef.isEmpty())
        return false;

    return otherXen->m_objectRef == this->m_objectRef; 
}

// SearchHistoryItem implementation

SearchHistoryItem::SearchHistoryItem(const QString& searchQuery)
    : m_searchQuery(searchQuery)
{
}

void SearchHistoryItem::go()
{
    // Navigate to search results (matches C# SearchHistoryItem.Go)
    qDebug() << "NavigationHistory: Navigate to search:" << this->m_searchQuery;

    // This will be handled by NavigationHistory::doHistoryItem
}

QString SearchHistoryItem::name() const
{
    // Format: "Search for 'query'" - matches C# format
    return QString("Search for '%1'").arg(this->m_searchQuery);
}

QIcon SearchHistoryItem::icon() const
{
    // Use search icon
    return QIcon(":/icons/search.png");
}

bool SearchHistoryItem::equals(const HistoryItem* other) const
{
    const SearchHistoryItem* otherSearch = dynamic_cast<const SearchHistoryItem*>(other);
    if (!otherSearch)
        return false;

    return otherSearch->m_searchQuery == this->m_searchQuery;
}

// NavigationHistory implementation

NavigationHistory::NavigationHistory(MainWindow* mainWindow, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow), m_backwardHistory(15) // C# uses LimitedStack<HistoryItem>(15)
      ,
      m_forwardHistory(15) // C# uses LimitedStack<HistoryItem>(15)
      ,
      m_currentHistoryItem(nullptr), m_inHistoryNavigation(false)
{
}

NavigationHistory::~NavigationHistory()
{
    this->m_backwardHistory.clear();
    this->m_forwardHistory.clear();
    this->m_currentHistoryItem.clear();
}

void NavigationHistory::newHistoryItem(const HistoryItemPtr& historyItem)
{
    if (!historyItem)
        return;

    // Don't add history items during history navigation
    if (this->m_inHistoryNavigation)
    {
        return;
    }

    // Don't add duplicate of current item
    if (!this->m_currentHistoryItem.isNull() && historyItem->equals(this->m_currentHistoryItem.data()))
    {
        return;
    }

    // Push current item to backward history
    if (!this->m_currentHistoryItem.isNull())
    {
        this->m_backwardHistory.push(this->m_currentHistoryItem);
    }

    // Clear forward history (new navigation path started)
    this->m_forwardHistory.clear();

    // Set new current item
    this->m_currentHistoryItem = historyItem;

    // Update button states
    this->enableHistoryButtons();
}

void NavigationHistory::replaceHistoryItem(const HistoryItemPtr& historyItem)
{
    if (!historyItem)
        return;

    // Used when modifying current item (e.g., search refinement)
    if (this->m_inHistoryNavigation)
    {
        return;
    }

    // Replace current item without affecting history stacks
    this->m_currentHistoryItem = historyItem;

    this->enableHistoryButtons();
}

void NavigationHistory::back(int steps)
{
    while (steps > 0 && !this->m_backwardHistory.isEmpty())
    {
        // Push current to forward history
        if (!this->m_currentHistoryItem.isNull())
        {
            this->m_forwardHistory.push(this->m_currentHistoryItem);
        }

        // Pop from backward history to current
        this->m_currentHistoryItem = this->m_backwardHistory.pop();

        steps--;
    }

    // Navigate to the history item
    if (!this->m_currentHistoryItem.isNull())
    {
        this->doHistoryItem(this->m_currentHistoryItem);
    }
}

void NavigationHistory::forward(int steps)
{
    while (steps > 0 && !this->m_forwardHistory.isEmpty())
    {
        // Push current to backward history
        if (!this->m_currentHistoryItem.isNull())
        {
            this->m_backwardHistory.push(this->m_currentHistoryItem);
        }

        // Pop from forward history to current
        this->m_currentHistoryItem = this->m_forwardHistory.pop();

        steps--;
    }

    // Navigate to the history item
    if (!this->m_currentHistoryItem.isNull())
    {
        this->doHistoryItem(this->m_currentHistoryItem);
    }
}

void NavigationHistory::doHistoryItem(const HistoryItemPtr& item)
{
    if (item.isNull())
        return;

    // Set flag to prevent recursive history updates
    this->m_inHistoryNavigation = true;

    try
    {
        // Perform actual navigation (matches C# History.Go)
        auto xenItem = qSharedPointerDynamicCast<XenModelObjectHistoryItem>(item);
        if (xenItem && this->m_mainWindow)
        {
            // Navigate to the object in the tree
            this->m_mainWindow->selectObjectInTree(xenItem->m_objectRef, xenItem->m_objectType);

            // Switch to the correct tab (will happen after object data loads)
            // We need to delay this slightly to allow tree selection to trigger
            QTimer::singleShot(100, this, [this, xenItem]() {
                if (this->m_mainWindow)
                {
                    this->m_mainWindow->setCurrentTab(xenItem->m_tabName);
                }
            });
        } else
        {
            // Handle SearchHistoryItem
            auto searchItem = qSharedPointerDynamicCast<SearchHistoryItem>(item);
            if (searchItem && this->m_mainWindow)
            {
                // TODO: Set search text in search box and trigger search
                // For now, just log it
                qDebug() << "NavigationHistory: Navigate to search:" << searchItem->m_searchQuery;
                // this->m_mainWindow->setSearchText(searchItem->m_searchQuery);
            } else
            {
                // Call go() for unknown item types
                item->go();
            }
        }
    } catch (...)
    {
        qWarning() << "NavigationHistory: Exception during history navigation";
    }

    // Clear flag and update buttons
    this->m_inHistoryNavigation = false;
    this->enableHistoryButtons();
}

void NavigationHistory::enableHistoryButtons()
{
    // Update back/forward button enabled state based on history availability
    bool canGoBack = !this->m_backwardHistory.isEmpty();
    bool canGoForward = !this->m_forwardHistory.isEmpty();

    if (this->m_mainWindow)
    {
        this->m_mainWindow->updateHistoryButtons(canGoBack, canGoForward);
    }
}

void NavigationHistory::populateBackDropDown(QMenu* menu)
{
    populateMenuWith(menu, this->m_backwardHistory, true);
}

void NavigationHistory::populateForwardDropDown(QMenu* menu)
{
    populateMenuWith(menu, this->m_forwardHistory, false);
}

void NavigationHistory::populateMenuWith(QMenu* menu,
                                         const LimitedStack<HistoryItemPtr>& history,
                                         bool isBackward)
{
    menu->clear();

    int i = 0;
    for (auto it = history.begin(); it != history.end(); ++it)
    {
        HistoryItemPtr item = *it;
        if (item.isNull())
        {
            continue;
        }

        int steps = ++i;

        QAction* action = menu->addAction(item->icon(), item->name());

        // Connect action to navigate the correct number of steps
        if (isBackward)
        {
            connect(action, &QAction::triggered, this, [this, steps]() {
                this->back(steps);
            });
        } else
        {
            connect(action, &QAction::triggered, this, [this, steps]() {
                this->forward(steps);
            });
        }
    }
}
