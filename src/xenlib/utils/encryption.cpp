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
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

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

QString EncryptionUtils::EncryptStringWithKey(const QString& clearString, const QByteArray& keyBytes)
{
    // Matches C# EncryptionUtils.EncryptString()
    // Uses AES-256-CBC with PKCS7 padding
    
    if (clearString.isEmpty() || keyBytes.size() != 32)
    {
        return QString();
    }

    // Generate random 16-byte salt/IV
    unsigned char saltBytes[16];
    if (RAND_bytes(saltBytes, 16) != 1)
    {
        return QString();
    }

    // Convert QString to UTF-16 bytes (matches C# Encoding.Unicode)
    QByteArray clearBytes;
    const ushort* unicode = clearString.utf16();
    int length = clearString.length();
    clearBytes.reserve(length * 2);
    for (int i = 0; i < length; ++i)
    {
        clearBytes.append(unicode[i] & 0xFF);
        clearBytes.append((unicode[i] >> 8) & 0xFF);
    }

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return QString();
    }

    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, 
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()), 
                           saltBytes) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    // Allocate output buffer (input size + block size for padding)
    int maxOutLen = clearBytes.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());
    QByteArray cipherBytes(maxOutLen, '\0');
    int outLen = 0;

    // Encrypt
    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(cipherBytes.data()), 
                          &outLen, 
                          reinterpret_cast<const unsigned char*>(clearBytes.constData()), 
                          clearBytes.size()) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipherBytes.data()) + outLen, 
                            &finalLen) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    EVP_CIPHER_CTX_free(ctx);

    // Resize to actual encrypted size
    cipherBytes.resize(outLen + finalLen);

    // Format: "base64_cipher,base64_salt" (matches C#)
    QString result = QString::fromLatin1(cipherBytes.toBase64()) + "," + 
                     QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(saltBytes), 16).toBase64());
    
    return result;
}

QString EncryptionUtils::DecryptStringWithKey(const QString& cipherText64, const QByteArray& keyBytes)
{
    // Matches C# EncryptionUtils.DecryptString()
    // Uses AES-256-CBC with PKCS7 padding
    
    if (cipherText64.isEmpty() || keyBytes.size() != 32)
    {
        return QString();
    }

    // Parse "cipher,salt" format
    QStringList parts = cipherText64.split(',');
    if (parts.size() != 2)
    {
        // Try legacy format without salt (backwards compatibility)
        parts.clear();
        parts << cipherText64 << QString();
    }

    // Decode base64 cipher and salt
    QByteArray cipherBytes = QByteArray::fromBase64(parts[0].toLatin1());
    QByteArray saltBytes;
    
    if (parts[1].isEmpty())
    {
        // Legacy: use default salt "XenRocks" encoded as UTF-16
        QString defaultSalt = "XenRocks";
        const ushort* unicode = defaultSalt.utf16();
        int length = defaultSalt.length();
        saltBytes.reserve(length * 2);
        for (int i = 0; i < length; ++i)
        {
            saltBytes.append(unicode[i] & 0xFF);
            saltBytes.append((unicode[i] >> 8) & 0xFF);
        }
        // Truncate or pad to 16 bytes
        if (saltBytes.size() > 16)
            saltBytes = saltBytes.left(16);
        else if (saltBytes.size() < 16)
            saltBytes.append(QByteArray(16 - saltBytes.size(), '\0'));
    } else
    {
        saltBytes = QByteArray::fromBase64(parts[1].toLatin1());
    }

    if (cipherBytes.isEmpty() || saltBytes.size() != 16)
    {
        return QString();
    }

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return QString();
    }

    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()),
                           reinterpret_cast<const unsigned char*>(saltBytes.constData())) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    // Allocate output buffer
    QByteArray clearBytes(cipherBytes.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()), '\0');
    int outLen = 0;

    // Decrypt
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(clearBytes.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char*>(cipherBytes.constData()),
                          cipherBytes.size()) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(clearBytes.data()) + outLen,
                            &finalLen) != 1)
    {
        // Decryption failed - wrong key or corrupted data
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    EVP_CIPHER_CTX_free(ctx);

    // Resize to actual decrypted size
    clearBytes.resize(outLen + finalLen);

    // Convert UTF-16 bytes back to QString
    QString result;
    if (clearBytes.size() % 2 != 0)
    {
        // Invalid UTF-16 data
        return QString();
    }

    result.reserve(clearBytes.size() / 2);
    for (int i = 0; i < clearBytes.size(); i += 2)
    {
        unsigned char low = clearBytes[i];
        unsigned char high = clearBytes[i + 1];
        ushort code = (high << 8) | low;
        result.append(QChar(code));
    }

    return result;
}
