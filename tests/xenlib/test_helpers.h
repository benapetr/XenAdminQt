/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#ifndef XENLIB_TEST_HELPERS_H
#define XENLIB_TEST_HELPERS_H

#include <QString>
#include <QSharedPointer>

class XenCache;

XenCache* LoadCacheFromEventJson(const QString& resourcePath);

template <typename T>
QSharedPointer<T> MakeObjectFromDummyCache(const QString& ref)
{
    return QSharedPointer<T>(new T(nullptr, ref));
}

#endif // XENLIB_TEST_HELPERS_H
