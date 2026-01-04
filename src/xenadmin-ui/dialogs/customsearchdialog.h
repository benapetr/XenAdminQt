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

#ifndef CUSTOMSEARCHDIALOG_H
#define CUSTOMSEARCHDIALOG_H

#include <QDialog>
#include "../controls/xensearch/searcher.h"

namespace Ui {
class CustomSearchDialog;
}

/**
 * CustomSearchDialog - Custom object type selection for search scope
 * 
 * Allows users to select which object types to search for:
 * - Pool
 * - Host (Server)
 * - VM
 * - Storage Repository (Local SR, Remote SR)
 * - Virtual Disk Image (VDI)
 * - Network
 * - Folder
 * 
 * Returns a QueryScope with selected ObjectTypes.
 * 
 * NOTE: C# XenCenter doesn't have a visible CustomSearchDialog class,
 * but the functionality is referenced in SearchFor dropdown.
 */
class CustomSearchDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit CustomSearchDialog(QWidget* parent = nullptr);
        ~CustomSearchDialog();

        /**
         * Get the selected object types as QueryScope
         */
        QueryScope* getQueryScope() const;

        /**
         * Set the initial object types
         */
        void setObjectTypes(XenSearch::ObjectTypes types);

    private slots:
        void onSelectAllClicked();
        void onClearAllClicked();

    private:
        Ui::CustomSearchDialog* ui;
};

#endif // CUSTOMSEARCHDIALOG_H
