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

// verticallytabbeddialog.cpp - Base class for properties dialogs with vertical tabs
#include "verticallytabbeddialog.h"
#include "ui_verticallytabbeddialog.h"
#include "../../xenlib/xenlib.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/operations/multipleoperation.h"
#include "../../xenlib/xen/asyncoperation.h"
#include <QMessageBox>
#include <QDebug>

VerticallyTabbedDialog::VerticallyTabbedDialog(XenConnection* connection,
                                               const QString& objectRef,
                                               const QString& objectType,
                                               QWidget* parent)
    : QDialog(parent), ui(new Ui::VerticallyTabbedDialog), m_connection(connection), m_objectRef(objectRef), m_objectType(objectType.toLower()), m_multiOp(nullptr)
{
    ui->setupUi(this);

    // Connect vertical tabs selection changed
    connect(ui->verticalTabs, &VerticalTabWidget::currentRowChanged,
            this, &VerticallyTabbedDialog::onVerticalTabsCurrentChanged);

    // Connect button box signals
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &VerticallyTabbedDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &VerticallyTabbedDialog::reject);

    // Load object data from XenServer
    loadObjectData();

    // NOTE: Subclass must call build() in its constructor after this base constructor
    // Cannot call pure virtual from base constructor in C++
}

VerticallyTabbedDialog::~VerticallyTabbedDialog()
{
    delete ui;
}

void VerticallyTabbedDialog::loadObjectData()
{
    // Request object data from XenLib
    // For now, we'll use a simplified approach - in full implementation,
    // this should wait for XenLib to provide data via signal

    // TODO: Implement proper async data loading
    // For now, create empty maps that pages will populate
    m_objectDataBefore = QVariantMap();
    m_objectDataCopy = QVariantMap();

    qDebug() << "VerticallyTabbedDialog: Loading data for" << m_objectType << m_objectRef;
}

void VerticallyTabbedDialog::showTab(IEditPage* page)
{
    if (!page)
        return;

    m_pages.append(page);

    // Add to vertical tabs
    ui->verticalTabs->addTab(page->image(), page->text(), page->subText(), page);

    // Add to content panel (stacked widget)
    ui->ContentPanel->addWidget(page);

    // Set connection so pages can create actions
    page->setConnection(m_connection);

    // Initialize page with object data
    // C# equivalent: editPage.SetXenObjects(_xenObjectBefore, _xenObjectCopy);
    page->setXenObjects(m_objectRef, m_objectType, m_objectDataBefore, m_objectDataCopy);

    // Connect populated signal to refresh tabs
    connect(page, &IEditPage::populated, [this]() {
        ui->verticalTabs->viewport()->update();
    });
}

void VerticallyTabbedDialog::selectPage(IEditPage* page)
{
    int index = m_pages.indexOf(page);
    if (index >= 0)
    {
        ui->verticalTabs->setCurrentRow(index);
    }
}

void VerticallyTabbedDialog::accept()
{
    // C# equivalent: btnOK_Click()

    // Step 1: Validate all pages
    // C# code: foreach (IEditPage editPage in verticalTabs.Items)
    //              if (!editPage.ValidToSave) { ... }
    for (IEditPage* page : m_pages)
    {
        if (!page->isValidToSave())
        {
            selectPage(page);
            page->showLocalValidationMessages();
            return; // Abort save
        }
    }

    // Step 2: Check if any changes exist
    // C# code: if (!changedPageExists) { Close(); return; }
    bool hasChanges = false;
    for (IEditPage* page : m_pages)
    {
        if (page->hasChanged())
        {
            hasChanges = true;
            break;
        }
    }

    if (!hasChanges)
    {
        QDialog::accept();
        return;
    }

    // Step 3: Collect actions from all pages
    // C# equivalent: List<AsyncAction> actions = SaveSettings();
    QList<AsyncOperation*> actions = collectActions();

    if (actions.isEmpty())
    {
        // No complex actions, just simple property changes
        // Apply them and close
        applySimpleChanges();
        QDialog::accept();
        return;
    }

    // Step 4: Create MultipleOperation and run
    // C# code:
    //   actions.Insert(index, new SaveChangesAction(_xenObjectCopy, true, _xenObjectBefore));
    //   _action = new MultipleAction(connection, title, desc, completedDesc, actions);
    //   _action.RunAsync();

    // First apply simple changes (equivalent to SaveChangesAction)
    applySimpleChanges();

    // Then run complex actions
    QString objName = m_objectDataBefore.value("name_label", m_objectRef).toString();
    m_multiOp = new MultipleOperation(
        m_connection,
        tr("Update Properties - %1").arg(objName),
        tr("Updating properties..."),
        tr("Properties updated"),
        actions,
        true,  // suppressHistory
        false, // showSubOperationDetails
        false, // stopOnFirstException
        this);

    connect(m_multiOp, &MultipleOperation::completed,
            this, &VerticallyTabbedDialog::onMultipleOperationCompleted);

    // Close dialog - operation runs in background
    QDialog::accept();

    // Run the operation
    m_multiOp->runAsync();
}

void VerticallyTabbedDialog::reject()
{
    // C# equivalent: btnCancel_Click()

    // Cleanup pages
    for (IEditPage* page : m_pages)
    {
        page->cleanup();
    }

    QDialog::reject();
}

void VerticallyTabbedDialog::closeEvent(QCloseEvent* event)
{
    // C# equivalent: OnFormClosing()

    // Cleanup pages
    for (IEditPage* page : m_pages)
    {
        page->cleanup();
    }

    QDialog::closeEvent(event);
}

QList<AsyncOperation*> VerticallyTabbedDialog::collectActions()
{
    // C# equivalent: SaveSettings() method
    // C# code:
    //   foreach (IEditPage editPage in verticalTabs.Items) {
    //       if (!editPage.HasChanged) continue;
    //       AsyncAction action = editPage.SaveSettings();
    //       if (action == null) continue;
    //       actions.Add(action);
    //   }

    QList<AsyncOperation*> actions;

    for (IEditPage* page : m_pages)
    {
        if (!page->hasChanged())
        {
            continue;
        }

        AsyncOperation* action = page->saveSettings();
        if (action)
        {
            actions.append(action);
            qDebug() << "VerticallyTabbedDialog: Collected action from page:" << page->text();
        }
    }

    return actions;
}

void VerticallyTabbedDialog::applySimpleChanges()
{
    // Apply simple property changes that pages made directly to objectDataCopy
    // C# equivalent: SaveChangesAction execution

    // In C#, SaveChangesAction calls xenObject.SaveChanges(Session, beforeObject)
    // which iterates changed properties and makes API calls

    // For Qt, we need to detect what changed in objectDataCopy vs objectDataBefore
    // and make appropriate API calls

    // TODO: Implement property change detection and API calls
    // This would make calls like:
    //   - VM.set_name_label() if name changed
    //   - VM.set_name_description() if description changed
    //   - etc.

    qDebug() << "VerticallyTabbedDialog: Applying simple property changes";

    // For now, this is a placeholder
    // Full implementation would iterate objectDataCopy keys and call XenAPI methods
}

void VerticallyTabbedDialog::onVerticalTabsCurrentChanged(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;

    IEditPage* page = m_pages[index];

    // Show selected page in content panel
    ui->ContentPanel->setCurrentWidget(page);

    // Hide validation messages on other pages
    for (IEditPage* otherPage : m_pages)
    {
        if (otherPage != page)
        {
            otherPage->hideLocalValidationMessages();
        }
    }
}

void VerticallyTabbedDialog::onMultipleOperationCompleted()
{
    if (m_multiOp)
    {
        if (m_multiOp->state() == AsyncOperation::Completed)
        {
            qDebug() << "VerticallyTabbedDialog: All actions completed successfully";
        } else
        {
            qDebug() << "VerticallyTabbedDialog: Some actions failed";
            // In C#, errors are shown via ActionProgressDialog
            // For Qt, we could show a message box or use progress dialog
        }

        // Clean up operation
        m_multiOp->deleteLater();
        m_multiOp = nullptr;
    }
}
