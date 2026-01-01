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

#ifndef OBSERVABLELIST_H
#define OBSERVABLELIST_H

#include "../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

// Base class for observable list signals (non-template)
class XENLIB_EXPORT ObservableListBase : public QObject
{
    Q_OBJECT

public:
    enum CollectionChangeAction
    {
        Add,
        Remove,
        Refresh,
        Clear
    };

    explicit ObservableListBase(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    virtual ~ObservableListBase() = default;

signals:
    void collectionChanged(CollectionChangeAction action);
    void cleared();

protected:
    void emitCollectionChanged(CollectionChangeAction action)
    {
        emit collectionChanged(action);
    }

    void emitCleared()
    {
        emit cleared();
    }
};

template <typename T>
class XENLIB_EXPORT ObservableList : public ObservableListBase
{
public:
    explicit ObservableList(QObject* parent = nullptr)
        : ObservableListBase(parent)
    {
    }
    virtual ~ObservableList() = default;

    // List interface
    void append(const T& value)
    {
        {
            QMutexLocker locker(&m_mutex);
            m_list.append(value);
        }
        emitCollectionChanged(Add);
    }

    void prepend(const T& value)
    {
        {
            QMutexLocker locker(&m_mutex);
            m_list.prepend(value);
        }
        emitCollectionChanged(Add);
    }

    void insert(int index, const T& value)
    {
        {
            QMutexLocker locker(&m_mutex);
            m_list.insert(index, value);
        }
        emitCollectionChanged(Add);
    }

    bool removeOne(const T& value)
    {
        bool removed = false;
        {
            QMutexLocker locker(&m_mutex);
            removed = m_list.removeOne(value);
        }
        if (removed)
            emitCollectionChanged(Remove);
        return removed;
    }

    void removeAt(int index)
    {
        bool removed = false;
        {
            QMutexLocker locker(&m_mutex);
            if (index >= 0 && index < m_list.size())
            {
                m_list.removeAt(index);
                removed = true;
            }
        }
        if (removed)
        {
            emitCollectionChanged(Remove);
        }
    }

    int removeAll(const T& value)
    {
        int count = 0;
        {
            QMutexLocker locker(&m_mutex);
            count = m_list.removeAll(value);
        }
        if (count > 0)
            emitCollectionChanged(Remove);
        return count;
    }

    void clear()
    {
        bool hadItems = false;
        {
            QMutexLocker locker(&m_mutex);
            hadItems = !m_list.isEmpty();
            if (hadItems)
                m_list.clear();
        }
        if (hadItems)
        {
            emitCleared();
            emitCollectionChanged(Clear);
        }
    }

    // Read operations (thread-safe)
    T at(int index) const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.at(index);
    }

    T value(int index, const T& defaultValue = T()) const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.value(index, defaultValue);
    }

    bool contains(const T& value) const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.contains(value);
    }

    int indexOf(const T& value, int from = 0) const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.indexOf(value, from);
    }

    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.size();
    }

    bool isEmpty() const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.isEmpty();
    }

    // Get a copy of the entire list (thread-safe)
    QList<T> toList() const
    {
        QMutexLocker locker(&m_mutex);
        return m_list;
    }

    // Iterator support (use toList() for safe iteration)
    typename QList<T>::const_iterator constBegin() const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.constBegin();
    }

    typename QList<T>::const_iterator constEnd() const
    {
        QMutexLocker locker(&m_mutex);
        return m_list.constEnd();
    }

    // Operators
    T operator[](int index) const
    {
        return at(index);
    }

private:
    QList<T> m_list;
    mutable QMutex m_mutex;
};

#endif // OBSERVABLELIST_H
