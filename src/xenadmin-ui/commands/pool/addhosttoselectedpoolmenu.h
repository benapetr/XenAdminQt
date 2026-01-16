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

#ifndef ADDHOSTTOSELECTEDPOOLMENU_H
#define ADDHOSTTOSELECTEDPOOLMENU_H

#include "../command.h"
#include <QMenu>
#include <QList>

class Host;
class Pool;
class XenConnection;

/**
 * @brief Dynamic menu for adding a host to the selected pool
 * 
 * This menu populates itself with available standalone hosts when opened.
 * Each standalone host gets a menu item that will add it to the selected pool.
 * A "Connect and Add to Pool" option is also provided at the bottom.
 */
class AddHostToSelectedPoolMenu : public QMenu
{
    Q_OBJECT

    public:
        explicit AddHostToSelectedPoolMenu(MainWindow* mainWindow, QWidget* parent = nullptr);

        /**
        * @brief Check if this menu should be enabled
        * @return true if exactly one pool is selected
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
        * @brief Get sorted list of standalone hosts
        * @return List of hosts not in any pool, sorted by name
        */
        QList<QSharedPointer<Host>> getSortedStandaloneHosts() const;
        
        /**
        * @brief Get the selected pool from main window
        * @return Selected pool or nullptr if none/multiple selected
        */
        QSharedPointer<Pool> getSelectedPool() const;
};

/**
 * @brief Private command used by AddHostToSelectedPoolMenu for validation
 * 
 * This command checks if exactly one pool is selected.
 */
class AddHostToSelectedPoolCommand : public Command
{
    Q_OBJECT

    public:
        explicit AddHostToSelectedPoolCommand(MainWindow* mainWindow);

        bool CanRun() const override;
        void Run() override { /* Not called - menu items call their own commands */ }
        
        QString MenuText() const override;
};

#endif // ADDHOSTTOSELECTEDPOOLMENU_H
