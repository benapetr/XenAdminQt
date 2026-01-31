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

#ifndef ADDSELECTEDHOSTTOPOOLMENU_H
#define ADDSELECTEDHOSTTOPOOLMENU_H

#include "../command.h"
#include <QMenu>
#include <QList>

class Host;
class XenConnection;

/**
 * @brief Dynamic menu for adding selected host(s) to a pool
 * 
 * This menu populates itself with available pools when opened.
 * Each pool gets a menu item that will add the selected host(s) to that pool.
 * A "New Pool" option is also provided at the bottom.
 */
class AddSelectedHostToPoolMenu : public QMenu
{
    Q_OBJECT

    public:
        explicit AddSelectedHostToPoolMenu(MainWindow* mainWindow, QWidget* parent = nullptr);

        /**
        * @brief Check if this menu should be enabled
        * @return true if at least one standalone host is selected
        */
        bool CanRun() const;

    private slots:
        /**
        * @brief Populate menu items when about to show
        */
        void onAboutToShow();

    private:
        MainWindow* mainWindow_;
        
        /**
        * @brief Get list of selected hosts from main window
        * @return List of standalone hosts in current selection
        */
        QList<QSharedPointer<Host>> getSelectedHosts() const;
};

/**
 * @brief Private command used by AddSelectedHostToPoolMenu for validation
 * 
 * This command checks if the selected items are all standalone hosts
 * (not already in a pool and pooling not restricted).
 */
class AddSelectedHostToPoolCommand : public Command
{
    Q_OBJECT

    public:
        explicit AddSelectedHostToPoolCommand(MainWindow* mainWindow);

        bool CanRun() const override;
        void Run() override { /* Not called - menu items call their own commands */ }
        
        QString MenuText() const override;

    private:
        /**
        * @brief Check if a single host can be added to a pool
        * @param host Host to check
        * @return true if host is standalone and not restricted
        */
        bool canRunOnHost(QSharedPointer<Host> host) const;
};

#endif // ADDSELECTEDHOSTTOPOOLMENU_H
