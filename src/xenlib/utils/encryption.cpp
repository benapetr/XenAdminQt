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

#include "encryption.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QSettings>
#include <QtCore/QUuid>
#include <QtCore/QRandomGenerator>

namespace
{
    const QString kProtectedPrefix = QStringLiteral("enc:");

    QString getOrCreateLocalKey()
    {
        QSettings settings;
        const QString keyName = QStringLiteral("Security/LocalKey");
        QString key = settings.value(keyName).toString();

        if (key.isEmpty())
        {
            key = EncryptionUtils::GenerateSessionKey();
            settings.setValue(keyName, key);
            settings.sync();
        }

        return key;
    }
}

QString EncryptionUtils::HashPassword(const QString& password)
{
    QByteArray data = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

QByteArray EncryptionUtils::Encrypt(const QByteArray& data, const QString& key)
{
    if (data.isEmpty() || key.isEmpty())
    {
        return QByteArray();
    }

    // Simple XOR encryption with key cycling
    QByteArray keyBytes = key.toUtf8();
    QByteArray encrypted = data;

    for (int i = 0; i < encrypted.size(); ++i)
    {
        encrypted[i] = encrypted[i] ^ keyBytes[i % keyBytes.size()];
    }

    // Encode as base64 for safe storage/transmission
    return encrypted.toBase64();
}

QByteArray EncryptionUtils::Decrypt(const QByteArray& data, const QString& key)
{
    if (data.isEmpty() || key.isEmpty())
    {
        return QByteArray();
    }

    // Decode from base64 first
    QByteArray encrypted = QByteArray::fromBase64(data);
    if (encrypted.isEmpty())
    {
        return QByteArray();
    }

    // XOR decryption with key cycling (same as encryption)
    QByteArray keyBytes = key.toUtf8();
    QByteArray decrypted = encrypted;

    for (int i = 0; i < decrypted.size(); ++i)
    {
        decrypted[i] = decrypted[i] ^ keyBytes[i % keyBytes.size()];
    }

    return decrypted;
}

QString EncryptionUtils::GenerateSessionKey()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString EncryptionUtils::EncryptString(const QString& text, const QString& key)
{
    if (text.isEmpty() || key.isEmpty())
    {
        return QString();
    }

    QByteArray textBytes = text.toUtf8();
    QByteArray encrypted = Encrypt(textBytes, key);
    return QString::fromLatin1(encrypted);
}

QString EncryptionUtils::DecryptString(const QString& encryptedText, const QString& key)
{
    if (encryptedText.isEmpty() || key.isEmpty())
    {
        return QString();
    }

    QByteArray encryptedBytes = encryptedText.toLatin1();
    QByteArray decrypted = Decrypt(encryptedBytes, key);
    return QString::fromUtf8(decrypted);
}

QString EncryptionUtils::ProtectString(const QString& text)
{
    if (text.isEmpty())
    {
        return QString();
    }

#ifdef _WIN32
    // TODO: replace with DPAPI-backed protect/unprotect.
#elif defined(Q_OS_MACOS)
    // TODO: replace with Keychain-backed protect/unprotect.
#endif
    const QString key = getOrCreateLocalKey();
    return kProtectedPrefix + EncryptString(text, key);
}

QString EncryptionUtils::UnprotectString(const QString& protectedText)
{
    if (protectedText.isEmpty())
    {
        return QString();
    }

    if (!protectedText.startsWith(kProtectedPrefix))
    {
        return protectedText;
    }

#ifdef _WIN32
    // TODO: replace with DPAPI-backed protect/unprotect.
#elif defined(Q_OS_MACOS)
    // TODO: replace with Keychain-backed protect/unprotect.
#endif
    const QString key = getOrCreateLocalKey();
    QString decrypted = DecryptString(protectedText.mid(kProtectedPrefix.size()), key);
    if (decrypted.isEmpty())
    {
        return QString();
    }

    return decrypted;
}

QString EncryptionUtils::GenerateSalt(int length)
{
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString salt;

    for (int i = 0; i < length; ++i)
    {
        int index = QRandomGenerator::global()->bounded(chars.length());
        salt.append(chars.at(index));
    }

    return salt;
}

QString EncryptionUtils::HashPasswordWithSalt(const QString& password, const QString& salt)
{
    QString saltedPassword = password + salt;
    QByteArray data = saltedPassword.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

QByteArray EncryptionUtils::ComputeHash(const QString& input)
{
    if (input.isEmpty())
    {
        return QByteArray();
    }

    // Use UTF-16 encoding to match C# UnicodeEncoding
    QByteArray data;
    const ushort* unicode = input.utf16();
    int length = input.length();

    data.reserve(length * 2);
    for (int i = 0; i < length; ++i)
    {
        data.append(unicode[i] & 0xFF);
        data.append((unicode[i] >> 8) & 0xFF);
    }

    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

bool EncryptionUtils::ArrayElementsEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    for (int i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return true;
}
