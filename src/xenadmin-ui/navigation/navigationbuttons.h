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

#ifndef NAVIGATIONBUTTONS_H
#define NAVIGATIONBUTTONS_H

#include <QPushButton>
#include <QToolButton>
#include <QMenu>

/**
 * @brief Interface for navigation buttons (big + small pairs)
 *
 * Matches C# INavigationItem interface
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/INavigationItem.cs
 */
class INavigationItem
{
    public:
        virtual ~INavigationItem() = default;

        virtual void setPairedItem(INavigationItem* item) = 0;
        virtual INavigationItem* pairedItem() const = 0;
        virtual void setChecked(bool checked) = 0;
        virtual bool isChecked() const = 0;
        virtual void setTag(QVariant tag) = 0;
        virtual QVariant tag() const = 0;
};

/**
 * @brief Base class for large navigation buttons
 *
 * Matches C# NavigationButtonBig
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationButtonBig.cs
 */
class NavigationButtonBig : public QToolButton, public INavigationItem
{
    Q_OBJECT

    public:
        explicit NavigationButtonBig(QWidget* parent = nullptr);

        // INavigationItem interface
        void setPairedItem(INavigationItem* item) override
        {
            m_pairedItem = item;
        }
        INavigationItem* pairedItem() const override
        {
            return m_pairedItem;
        }
        void setChecked(bool checked) override;
        bool isChecked() const override
        {
            return QToolButton::isChecked();
        }
        void setTag(QVariant tag) override
        {
            m_tag = tag;
        }
        QVariant tag() const override
        {
            return m_tag;
        }

    signals:
        void navigationViewChanged();

    protected:
        void checkStateChanged();

    private:
        INavigationItem* m_pairedItem;
        QVariant m_tag;
};

/**
 * @brief Base class for small navigation buttons
 *
 * Matches C# NavigationButtonSmall
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationButtonSmall.cs
 */
class NavigationButtonSmall : public QToolButton, public INavigationItem
{
    Q_OBJECT

    public:
        explicit NavigationButtonSmall(QWidget* parent = nullptr);

        // INavigationItem interface
        void setPairedItem(INavigationItem* item) override
        {
            m_pairedItem = item;
        }
        INavigationItem* pairedItem() const override
        {
            return m_pairedItem;
        }
        void setChecked(bool checked) override;
        bool isChecked() const override
        {
            return QToolButton::isChecked();
        }
        void setTag(QVariant tag) override
        {
            m_tag = tag;
        }
        QVariant tag() const override
        {
            return m_tag;
        }

    signals:
        void navigationViewChanged();

    protected:
        void checkStateChanged();

    private:
        INavigationItem* m_pairedItem;
        QVariant m_tag;
};

/**
 * @brief Dropdown navigation button (big variant)
 *
 * Matches C# NavigationDropDownButtonBig
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationDropDownButtonBig.cs
 */
class NavigationDropDownButtonBig : public NavigationButtonBig
{
    Q_OBJECT

    public:
        explicit NavigationDropDownButtonBig(QWidget* parent = nullptr);

        void setItemList(const QList<QAction*>& items);
        QMenu* dropDownMenu() const
        {
            return m_menu;
        }

    private:
        QMenu* m_menu;
};

/**
 * @brief Dropdown navigation button (small variant)
 *
 * Matches C# NavigationDropDownButtonSmall
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationDropDownButtonSmall.cs
 */
class NavigationDropDownButtonSmall : public NavigationButtonSmall
{
    Q_OBJECT

    public:
        explicit NavigationDropDownButtonSmall(QWidget* parent = nullptr);

        void setItemList(const QList<QAction*>& items);
        QMenu* dropDownMenu() const
        {
            return m_menu;
        }

    private:
        QMenu* m_menu;
};

/**
 * @brief Notification button (big variant) with unread count
 *
 * Matches C# NotificationButtonBig
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NotificationButtonBig.cs
 */
class NotificationButtonBig : public NavigationButtonBig
{
    Q_OBJECT

    public:
        explicit NotificationButtonBig(QWidget* parent = nullptr);

        void setUnreadEntries(int count);
        int unreadEntries() const
        {
            return m_unreadCount;
        }

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        int m_unreadCount;
};

/**
 * @brief Notification button (small variant) with unread count
 *
 * Matches C# NotificationButtonSmall
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NotificationButtonSmall.cs
 */
class NotificationButtonSmall : public NavigationButtonSmall
{
    Q_OBJECT

    public:
        explicit NotificationButtonSmall(QWidget* parent = nullptr);

        void setUnreadEntries(int count);
        int unreadEntries() const
        {
            return m_unreadCount;
        }

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        int m_unreadCount;
};

#endif // NAVIGATIONBUTTONS_H
