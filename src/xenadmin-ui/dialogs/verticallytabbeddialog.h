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

// verticallytabbeddialog.h - Base class for properties dialogs with vertical tabs
// Matches C# XenAdmin.Dialogs.VerticallyTabbedDialog
#ifndef VERTICALLYTABBEDDIALOG_H
#define VERTICALLYTABBEDDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>
#include <QVariantMap>
#include "../settingspanels/ieditpage.h"

namespace Ui
{
    class VerticallyTabbedDialog;
}

class XenConnection;
class AsyncOperation;
class MultipleOperation;

/**
 * @brief VerticallyTabbedDialog - Base class for property editor dialogs
 *
 * Qt equivalent of C# XenAdmin.Dialogs.VerticallyTabbedDialog
 *
 * This is the base class for all properties dialogs (Host, VM, Pool, SR, etc.).
 * It provides the standard vertical tab layout and save workflow using Actions.
 *
 * Key features (matching C# implementation):
 * 1. Vertical tab layout using VerticalTabWidget
 * 2. IEditPage management - adds pages dynamically based on object type
 * 3. Object cloning - creates objectDataCopy for editing without affecting cache
 * 4. AsyncOperation-based save:
 *    - Collects actions from pages via saveSettings()
 *    - Applies simple property changes first
 *    - Executes complex actions via MultipleOperation
 * 5. Validation - checks all pages before allowing save
 * 6. Change tracking - only saves if pages have changed
 *
 * C# source: XenAdmin/Dialogs/VerticallyTabbedDialog.cs
 */
class VerticallyTabbedDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection to the XenServer
     * @param objectRef OpaqueRef of object being edited
     * @param objectType Type string ("vm", "host", "pool", "sr", etc.)
     * @param parent Parent widget
     *
     * C# equivalent: Constructor that takes IXenObject
     */
    explicit VerticallyTabbedDialog(XenConnection* connection,
                                    const QString& objectRef,
                                    const QString& objectType,
                                    QWidget* parent = nullptr);
    ~VerticallyTabbedDialog();

protected:
    /**
     * @brief Build the page list - override in subclasses
     *
     * C# equivalent: Build() method
     */
    virtual void build() = 0;

    /**
     * @brief Add a page to the dialog
     * @param page IEditPage instance (takes ownership)
     *
     * C# equivalent: ShowTab(IEditPage) method
     */
    void showTab(IEditPage* page);

    /**
     * @brief Select a specific page
     * @param page IEditPage to select
     */
    void selectPage(IEditPage* page);

    // Accessors for subclasses
    XenConnection* connection() const
    {
        return m_connection;
    }
    QString objectRef() const
    {
        return m_objectRef;
    }
    QString objectType() const
    {
        return m_objectType;
    }
    QVariantMap objectDataBefore() const
    {
        return m_objectDataBefore;
    }
    QVariantMap objectDataCopy() const
    {
        return m_objectDataCopy;
    }

    /**
     * @brief Override accept to implement save logic with Actions
     *
     * C# equivalent: btnOK_Click()
     */
    void accept() override;

    /**
     * @brief Override reject to cleanup
     */
    void reject() override;

    /**
     * @brief Override closeEvent to cleanup pages
     */
    void closeEvent(QCloseEvent* event) override;

protected:
    // Allow subclasses to access UI and page list
    Ui::VerticallyTabbedDialog* ui;
    QList<IEditPage*> m_pages;

private slots:
    void onVerticalTabsCurrentChanged(int index);
    void onMultipleOperationCompleted();

private:
    void loadObjectData();
    QList<AsyncOperation*> collectActions();
    void applySimpleChanges();

    XenConnection* m_connection;
    QString m_objectRef;
    QString m_objectType;
    QVariantMap m_objectDataBefore; // Original data (read-only)
    QVariantMap m_objectDataCopy;   // Clone being edited

    MultipleOperation* m_multiOp; // Tracks running save operation
};

#endif // VERTICALLYTABBEDDIALOG_H
