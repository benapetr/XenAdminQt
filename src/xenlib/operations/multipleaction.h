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

#ifndef MULTIPLEACTION_H
#define MULTIPLEACTION_H

#include "../xen/asyncoperation.h"
#include <QList>
#include <QString>

/**
 * @brief Run multiple operations sequentially
 *
 * Qt equivalent of C# MultipleAction. Executes a list of AsyncOperations
 * one after another, aggregating their progress and collecting exceptions.
 * The outer operation is asynchronous, but sub-operations run synchronously
 * within it.
 *
 * Usage:
 *   QList<AsyncOperation*> ops = { op1, op2, op3 };
 *   auto multi = new MultipleAction(connection, "Bulk Operation",
 *                                      "Starting...", "Complete", ops);
 *   multi->execute();
 */
class MultipleAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a multiple operation
         * @param connection XenAPI connection (can be nullptr for cross-connection ops)
         * @param title Operation title
         * @param startDescription Description shown when starting
         * @param endDescription Description shown when complete
         * @param subOperations List of operations to run sequentially
         * @param suppressHistory If true, don't add to history
         * @param showSubOperationDetails If true, show details of sub-operations
         * @param stopOnFirstException If true, stop on first error
         * @param parent Parent QObject
         */
        explicit MultipleAction(XenConnection* connection,
                                   const QString& title,
                                   const QString& startDescription,
                                   const QString& endDescription,
                                   const QList<AsyncOperation*>& subOperations,
                                   bool suppressHistory = false,
                                   bool showSubOperationDetails = false,
                                   bool stopOnFirstException = false,
                                   QObject* parent = nullptr);

        /**
         * @brief Destructor
         */
        ~MultipleAction() override;

        /**
         * @brief Get the list of sub-operations
         */
        QList<AsyncOperation*> subOperations() const
        {
            return m_subOperations;
        }

        /**
         * @brief Check if sub-operation details should be shown
         */
        bool showSubOperationDetails() const
        {
            return m_showSubOperationDetails;
        }

        /**
         * @brief Get current sub-operation title
         */
        QString subOperationTitle() const
        {
            return m_subOperationTitle;
        }

        /**
         * @brief Get current sub-operation description
         */
        QString subOperationDescription() const
        {
            return m_subOperationDescription;
        }

    signals:
        /**
         * @brief Emitted when sub-operation details change
         */
        void subOperationChanged(const QString& title, const QString& description);

    protected:
        /**
         * @brief Main execution logic
         */
        void run() override;

        /**
         * @brief Run all sub-operations
         * @param exceptions List to collect exception messages
         */
        virtual void runSubOperations(QStringList& exceptions);

        /**
         * @brief Recalculate overall progress from sub-operations
         */
        virtual void recalculatePercentComplete();

        /**
         * @brief Cancel operation implementation
         */
        void onCancel() override;

        /**
         * @brief Called when operation completes
         */
        virtual void onMultipleOperationCompleted();

    private slots:
        /**
         * @brief Handle sub-operation changes
         */
        void onSubOperationChanged();

        /**
         * @brief Handle operation completion
         */
        void onCompleted();

    private:
        void registerEvents();
        void deregisterEvents();

        QList<AsyncOperation*> m_subOperations;
        QString m_endDescription;
        bool m_showSubOperationDetails;
        bool m_stopOnFirstException;
        QString m_subOperationTitle;
        QString m_subOperationDescription;
};

#endif // MULTIPLEACTION_H
