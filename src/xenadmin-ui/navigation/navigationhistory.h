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

#ifndef NAVIGATIONHISTORY_H
#define NAVIGATIONHISTORY_H

#include <QObject>
#include <QStack>
#include <QString>
#include <QIcon>
#include <QVariantMap>
#include <QSharedPointer>

// Forward declarations
class MainWindow;
class QAction;
class QMenu;

/**
 * @brief Base class for history items (matches C# HistoryItem)
 *
 * Represents a navigation point in the application (object + tab, search, etc.)
 * Subclasses implement Go() to navigate to the specific item.
 */
class HistoryItem
{
public:
    virtual ~HistoryItem() = default;

    /**
     * @brief Navigate to this history item
     */
    virtual void go() = 0;

    /**
     * @brief Get display name for this history item
     */
    virtual QString name() const = 0;

    /**
     * @brief Get icon for this history item
     */
    virtual QIcon icon() const = 0;

    /**
     * @brief Check equality with another history item
     */
    virtual bool equals(const HistoryItem* other) const = 0;
};

using HistoryItemPtr = QSharedPointer<HistoryItem>;

/**
 * @brief History item for Xen objects (VM, Host, Pool, etc.) with tab
 *
 * Matches C# XenModelObjectHistoryItem
 */
class XenModelObjectHistoryItem : public HistoryItem
{
public:
    XenModelObjectHistoryItem(const QString& objectRef, const QString& objectType,
                              const QString& objectName, const QIcon& objectIcon,
                              const QString& tabName);

    void go() override;
    QString name() const override;
    QIcon icon() const override;
    bool equals(const HistoryItem* other) const override;

    // Allow NavigationHistory to access members for navigation
    friend class NavigationHistory;

private:
    QString m_objectRef;  // OpaqueRef or empty for home
    QString m_objectType; // "vm", "host", "pool", etc.
    QString m_objectName; // Display name
    QIcon m_objectIcon;   // Icon for tree
    QString m_tabName;    // Tab title (e.g., "General", "Console")
};

/**
 * @brief History item for search queries
 *
 * Matches C# SearchHistoryItem
 */
class SearchHistoryItem : public HistoryItem
{
public:
    explicit SearchHistoryItem(const QString& searchQuery);

    void go() override;
    QString name() const override;
    QIcon icon() const override;
    bool equals(const HistoryItem* other) const override;

    friend class NavigationHistory;

private:
    QString m_searchQuery;
};

/**
 * @brief Limited stack with maximum size (matches C# LimitedStack)
 *
 * When size exceeds limit, oldest items are discarded
 */
template <typename T>
class LimitedStack
{
public:
    explicit LimitedStack(int maxSize)
        : m_maxSize(maxSize)
    {}

    void push(const T& item)
    {
        m_stack.push(item);
        while (m_stack.size() > m_maxSize)
        {
            m_stack.removeFirst();
        }
    }

    T pop()
    {
        if (m_stack.isEmpty())
            return T();
        return m_stack.pop();
    }

    T peek() const
    {
        if (m_stack.isEmpty())
            return T();
        return m_stack.top();
    }

    void clear()
    {
        m_stack.clear();
    }

    bool isEmpty() const
    {
        return m_stack.isEmpty();
    }
    int size() const
    {
        return m_stack.size();
    }

    // Iterator support for dropdown menu population
    typename QStack<T>::const_iterator begin() const
    {
        return m_stack.begin();
    }
    typename QStack<T>::const_iterator end() const
    {
        return m_stack.end();
    }

private:
    QStack<T> m_stack;
    int m_maxSize;
};

/**
 * @brief Navigation history manager (matches C# History class)
 *
 * Manages back/forward navigation through the application's history.
 * Uses two stacks to track backward and forward history items.
 */
class NavigationHistory : public QObject
{
    Q_OBJECT

public:
    explicit NavigationHistory(MainWindow* mainWindow, QObject* parent = nullptr);
    ~NavigationHistory();

    /**
     * @brief Add new history item (matches C# History.NewHistoryItem)
     *
     * Pushes current item to backward history and clears forward history.
     * If item equals current item, does nothing.
     */
    void newHistoryItem(const HistoryItemPtr& historyItem);

    /**
     * @brief Replace current history item (matches C# History.ReplaceHistoryItem)
     *
     * Used when modifying current item (e.g., search refinement)
     */
    void replaceHistoryItem(const HistoryItemPtr& historyItem);

    /**
     * @brief Navigate backward in history (matches C# History.Back)
     * @param steps Number of steps to go back
     */
    void back(int steps = 1);

    /**
     * @brief Navigate forward in history (matches C# History.Forward)
     * @param steps Number of steps to go forward
     */
    void forward(int steps = 1);

    /**
     * @brief Enable/disable back and forward buttons (matches C# History.EnableHistoryButtons)
     */
    void enableHistoryButtons();

    /**
     * @brief Populate back button dropdown menu (matches C# History.PopulateBackDropDown)
     */
    void populateBackDropDown(QMenu* menu);

    /**
     * @brief Populate forward button dropdown menu (matches C# History.PopulateForwardDropDown)
     */
    void populateForwardDropDown(QMenu* menu);

    /**
     * @brief Check if currently in history navigation
     *
     * Used to prevent recursive history updates during Go() execution
     */
    bool isInHistoryNavigation() const
    {
        return m_inHistoryNavigation;
    }

private:
    void doHistoryItem(const HistoryItemPtr& item);
    void populateMenuWith(QMenu* menu, const LimitedStack<HistoryItemPtr>& history,
                          bool isBackward);

    MainWindow* m_mainWindow;
    LimitedStack<HistoryItemPtr> m_backwardHistory;
    LimitedStack<HistoryItemPtr> m_forwardHistory;
    HistoryItemPtr m_currentHistoryItem;
    bool m_inHistoryNavigation;
};

#endif // NAVIGATIONHISTORY_H
