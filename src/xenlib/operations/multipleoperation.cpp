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

#include "multipleoperation.h"
#include <QDebug>

MultipleOperation::MultipleOperation(XenConnection* connection,
                                     const QString& title,
                                     const QString& startDescription,
                                     const QString& endDescription,
                                     const QList<AsyncOperation*>& subOperations,
                                     bool suppressHistory,
                                     bool showSubOperationDetails,
                                     bool stopOnFirstException,
                                     QObject* parent)
    : AsyncOperation(connection, title, startDescription, parent), m_subOperations(subOperations), m_endDescription(endDescription), m_showSubOperationDetails(showSubOperationDetails), m_stopOnFirstException(stopOnFirstException)
{
    // Note: suppressHistory is not currently used in AsyncOperation
    Q_UNUSED(suppressHistory);
    
    // Take ownership of all sub-operations
    // This ensures they are destroyed when MultipleOperation is destroyed
    for (AsyncOperation* subOp : this->m_subOperations)
    {
        if (subOp)
        {
            subOp->setParent(this);
        }
    }
    
    this->registerEvents();
    connect(this, &AsyncOperation::completed, this, &MultipleOperation::onCompleted);
}

MultipleOperation::~MultipleOperation()
{
    // Qt automatically disconnects signals when objects are destroyed
    // No need to manually deregister - in fact, doing so can cause crashes
    // if sub-operations have already been deleted
    // deregisterEvents();
}

void MultipleOperation::registerEvents()
{
    for (AsyncOperation* subOp : m_subOperations)
    {
        connect(subOp, &AsyncOperation::progressChanged,
                this, &MultipleOperation::onSubOperationChanged);
        connect(subOp, &AsyncOperation::descriptionChanged,
                this, &MultipleOperation::onSubOperationChanged);
    }
}

void MultipleOperation::deregisterEvents()
{
    // Qt automatically disconnects signals when sender or receiver is destroyed
    // We only need to explicitly disconnect if objects are still alive
    // Use try-catch to handle any potential issues during disconnection
    for (AsyncOperation* subOp : this->m_subOperations)
    {
        if (subOp)
        {
            // Qt will handle already-destroyed objects gracefully
            QObject::disconnect(subOp, nullptr, this, nullptr);
        }
    }
}

void MultipleOperation::onSubOperationChanged()
{
    AsyncOperation* subOp = qobject_cast<AsyncOperation*>(sender());
    if (subOp)
    {
        m_subOperationTitle = subOp->GetTitle();
        m_subOperationDescription = subOp->GetDescription();
        recalculatePercentComplete();
        emit subOperationChanged(m_subOperationTitle, m_subOperationDescription);
    }
}

void MultipleOperation::run()
{
    this->SetPercentComplete(0);
    QStringList exceptions;

    this->runSubOperations(exceptions);

    this->SetPercentComplete(100);
    this->SetDescription(this->m_endDescription);

    // Handle multiple exceptions
    if (exceptions.size() > 1)
    {
        for (const QString& e : exceptions)
        {
            qWarning() << "MultipleOperation: Exception:" << e;
        }
        this->setError(tr("Some errors were encountered during the operation"));
    } else if (exceptions.size() == 1)
    {
        // Single exception - already set in runSubOperations
    }

    if (this->IsCancelled())
    {
        this->setError(tr("Operation cancelled"));
    }
}

void MultipleOperation::runSubOperations(QStringList& exceptions)
{
    for (AsyncOperation* subOp : m_subOperations)
    {
        if (IsCancelled())
        {
            break; // Don't start any more operations
        }

        try
        {
            m_subOperationTitle = subOp->GetTitle();

            // Run sub-operation synchronously
            subOp->RunSync(GetSession());

            // Check if sub-operation failed
            if (subOp->HasError())
            {
                QString errorMsg = subOp->GetErrorMessage();
                if (!errorMsg.isEmpty())
                {
                    exceptions.append(errorMsg);

                    // Record first exception
                    if (!HasError())
                    {
                        setError(errorMsg);
                    }

                    if (m_stopOnFirstException)
                    {
                        break;
                    }
                }
            }
        } catch (const std::exception& e)
        {
            QString errorMsg = QString::fromStdString(e.what());
            exceptions.append(errorMsg);

            // Record first exception
            if (!HasError())
            {
                setError(errorMsg);
            }

            if (m_stopOnFirstException)
            {
                break;
            }
        } catch (...)
        {
            QString errorMsg = tr("Unknown error occurred");
            exceptions.append(errorMsg);

            if (!HasError())
            {
                setError(errorMsg);
            }

            if (m_stopOnFirstException)
            {
                break;
            }
        }
    }
}

void MultipleOperation::recalculatePercentComplete()
{
    if (m_subOperations.isEmpty())
    {
        return;
    }

    int total = 0;
    for (AsyncOperation* subOp : m_subOperations)
    {
        total += subOp->GetPercentComplete();
    }

    int avgProgress = total / m_subOperations.size();
    SetPercentComplete(avgProgress);
}

void MultipleOperation::onCancel()
{
    // Cancel all incomplete sub-operations
    for (AsyncOperation* subOp : m_subOperations)
    {
        if (!subOp->IsCompleted())
        {
            subOp->Cancel();
        }
    }
}

void MultipleOperation::onCompleted()
{
    onMultipleOperationCompleted();
}

void MultipleOperation::onMultipleOperationCompleted()
{
    // Cancel any incomplete sub-operations
    // (in case MultipleOperation completed prematurely)
    for (AsyncOperation* subOp : m_subOperations)
    {
        if (!subOp->IsCompleted())
        {
            subOp->Cancel();
        }
    }
}
