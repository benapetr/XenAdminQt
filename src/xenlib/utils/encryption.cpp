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
#include <QtCore/QUuid>
#include <QtCore/QRandomGenerator>
#include <QDebug>

// Platform-specific crypto headers
#ifdef Q_OS_WIN
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#elif defined(Q_OS_MACOS)
    #include <CommonCrypto/CommonCryptor.h>
    #include <CommonCrypto/CommonRandom.h>
    #include <CommonCrypto/CommonKeyDerivation.h>
#else
    // Linux/Unix: OpenSSL
    #include <openssl/evp.h>
    #include <openssl/rand.h>
    #include <openssl/err.h>
#endif

namespace
{
    const QString kProtectedPrefix = QStringLiteral("enc:");
    QString s_localKey;
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

    const QString key = s_localKey;
    if (key.isEmpty())
    {
        qWarning() << "EncryptionUtils: local key not set; cannot protect string";
        return QString();
    }
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

    const QString key = s_localKey;
    if (key.isEmpty())
    {
        qWarning() << "EncryptionUtils: local key not set; cannot unprotect string";
        return QString();
    }
    QString decrypted = DecryptString(protectedText.mid(kProtectedPrefix.size()), key);
    if (decrypted.isEmpty())
    {
        return QString();
    }

    return decrypted;
}

void EncryptionUtils::SetLocalKey(const QString& key)
{
    s_localKey = key;
}

QString EncryptionUtils::GetLocalKey()
{
    return s_localKey;
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

bool EncryptionUtils::EncryptionAvailable()
{
    return true;
}

QByteArray EncryptionUtils::GenerateSaltBytes(int length)
{
    QByteArray salt(length, '\0');

#ifdef Q_OS_WIN
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(salt.data()), static_cast<ULONG>(salt.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0)
    {
        return salt;
    }
#elif defined(Q_OS_MACOS)
    if (CCRandomGenerateBytes(salt.data(), salt.size()) == kCCSuccess)
    {
        return salt;
    }
#else
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size()) == 1)
    {
        return salt;
    }
#endif

    for (int i = 0; i < salt.size(); ++i)
    {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return salt;
}

QByteArray EncryptionUtils::DeriveKeyPBKDF2(const QByteArray& passwordBytes, const QByteArray& saltBytes, int iterations, int keyLen)
{
    if (passwordBytes.isEmpty() || saltBytes.isEmpty() || iterations <= 0 || keyLen <= 0)
    {
        return QByteArray();
    }

    QByteArray key(keyLen, '\0');

#ifdef Q_OS_WIN
    NTSTATUS status = BCryptDeriveKeyPBKDF2(BCRYPT_SHA256_ALGORITHM,
                                            reinterpret_cast<PUCHAR>(const_cast<char*>(passwordBytes.constData())),
                                            static_cast<ULONG>(passwordBytes.size()),
                                            reinterpret_cast<PUCHAR>(const_cast<char*>(saltBytes.constData())),
                                            static_cast<ULONG>(saltBytes.size()),
                                            static_cast<ULONG>(iterations),
                                            reinterpret_cast<PUCHAR>(key.data()),
                                            static_cast<ULONG>(key.size()),
                                            0);
    if (!BCRYPT_SUCCESS(status))
    {
        return QByteArray();
    }
#elif defined(Q_OS_MACOS)
    int status = CCKeyDerivationPBKDF(kCCPBKDF2,
                                      passwordBytes.constData(),
                                      passwordBytes.size(),
                                      reinterpret_cast<const uint8_t*>(saltBytes.constData()),
                                      saltBytes.size(),
                                      kCCPRFHmacAlgSHA256,
                                      static_cast<uint>(iterations),
                                      reinterpret_cast<uint8_t*>(key.data()),
                                      key.size());
    if (status != kCCSuccess)
    {
        return QByteArray();
    }
#else
    if (PKCS5_PBKDF2_HMAC(passwordBytes.constData(), passwordBytes.size(),
                          reinterpret_cast<const unsigned char*>(saltBytes.constData()),
                          saltBytes.size(),
                          iterations,
                          EVP_sha256(),
                          key.size(),
                          reinterpret_cast<unsigned char*>(key.data())) != 1)
    {
        return QByteArray();
    }
#endif

    return key;
}

QByteArray EncryptionUtils::DeriveKeyPBKDF2(const QString& password, const QByteArray& saltBytes, int iterations, int keyLen)
{
    return DeriveKeyPBKDF2(password.toUtf8(), saltBytes, iterations, keyLen);
}

QByteArray EncryptionUtils::ComputePasswordHashPBKDF2(const QString& password, const QByteArray& saltBytes, int iterations, int hashLen)
{
    return DeriveKeyPBKDF2(password, saltBytes, iterations, hashLen);
}

bool EncryptionUtils::VerifyPasswordPBKDF2(const QString& password, const QByteArray& expectedHash, const QByteArray& saltBytes, int iterations)
{
    if (expectedHash.isEmpty())
        return false;

    QByteArray computed = ComputePasswordHashPBKDF2(password, saltBytes, iterations, expectedHash.size());
    return ArrayElementsEqual(computed, expectedHash);
}

bool EncryptionUtils::DerivePasswordSecrets(const QString& password, int iterations,
                                            QByteArray& outKey, QByteArray& outKeySalt,
                                            QByteArray& outVerifyHash, QByteArray& outVerifySalt)
{
    if (password.isEmpty() || iterations <= 0)
        return false;

    outKeySalt = GenerateSaltBytes(16);
    outVerifySalt = GenerateSaltBytes(16);

    outKey = DeriveKeyPBKDF2(password, outKeySalt, iterations, 32);
    outVerifyHash = ComputePasswordHashPBKDF2(password, outVerifySalt, iterations, 32);

    if (outKey.isEmpty() || outVerifyHash.isEmpty())
    {
        outKey.clear();
        outKeySalt.clear();
        outVerifyHash.clear();
        outVerifySalt.clear();
        return false;
    }

    return true;
}

bool EncryptionUtils::VerifyPasswordAndDeriveKey(const QString& password,
                                                 const QByteArray& expectedHash, const QByteArray& verifySalt,
                                                 const QByteArray& keySalt, int iterations,
                                                 QByteArray& outKey)
{
    if (!VerifyPasswordPBKDF2(password, expectedHash, verifySalt, iterations))
        return false;

    outKey = DeriveKeyPBKDF2(password, keySalt, iterations, 32);
    return !outKey.isEmpty();
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

#ifdef Q_OS_WIN
    // Windows: BCrypt API
    unsigned char saltBytes[16];
    BCryptGenRandom(nullptr, saltBytes, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status))
        return QString();
    
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, 
                               (PUCHAR)BCRYPT_CHAIN_MODE_CBC, 
                               sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)keyBytes.constData(), 32, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    DWORD cbCipherText = 0;
    status = BCryptEncrypt(hKey, (PUCHAR)clearBytes.constData(), clearBytes.size(),
                          nullptr, saltBytes, 16, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    QByteArray cipherBytes(cbCipherText, '\0');
    status = BCryptEncrypt(hKey, (PUCHAR)clearBytes.constData(), clearBytes.size(),
                          nullptr, saltBytes, 16, (PUCHAR)cipherBytes.data(), 
                          cbCipherText, &cbCipherText, BCRYPT_BLOCK_PADDING);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (!BCRYPT_SUCCESS(status))
        return QString();
    
    cipherBytes.resize(cbCipherText);
    QString result = QString::fromLatin1(cipherBytes.toBase64()) + "," + 
                     QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(saltBytes), 16).toBase64());
    return result;
    
#elif defined(Q_OS_MACOS)
    // macOS: CommonCrypto
    unsigned char saltBytes[16];
    CCRandomGenerateBytes(saltBytes, 16);
    
    size_t dataOutMoved = 0;
    size_t maxOutLen = clearBytes.size() + kCCBlockSizeAES128;
    QByteArray cipherBytes(maxOutLen, '\0');
    
    CCCryptorStatus status = CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                                     keyBytes.constData(), 32, saltBytes,
                                     clearBytes.constData(), clearBytes.size(),
                                     cipherBytes.data(), maxOutLen, &dataOutMoved);
    
    if (status != kCCSuccess)
        return QString();
    
    cipherBytes.resize(dataOutMoved);
    QString result = QString::fromLatin1(cipherBytes.toBase64()) + "," + 
                     QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(saltBytes), 16).toBase64());
    return result;
    
#else
    // Linux/Unix: OpenSSL
    unsigned char saltBytes[16];
    if (RAND_bytes(saltBytes, 16) != 1)
        return QString();
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return QString();
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, 
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()), 
                           saltBytes) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }
    
    int maxOutLen = clearBytes.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());
    QByteArray cipherBytes(maxOutLen, '\0');
    int outLen = 0;
    
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
    cipherBytes.resize(outLen + finalLen);
    
    QString result = QString::fromLatin1(cipherBytes.toBase64()) + "," + 
                     QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(saltBytes), 16).toBase64());
    return result;
#endif
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

#ifdef Q_OS_WIN
    // Windows: BCrypt API
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status))
        return QString();
    
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
                               sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)keyBytes.constData(), 32, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    DWORD cbPlainText = 0;
    status = BCryptDecrypt(hKey, (PUCHAR)cipherBytes.constData(), cipherBytes.size(),
                          nullptr, (PUCHAR)saltBytes.constData(), 16, 
                          nullptr, 0, &cbPlainText, BCRYPT_BLOCK_PADDING);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return QString();
    }
    
    QByteArray clearBytes(cbPlainText, '\0');
    status = BCryptDecrypt(hKey, (PUCHAR)cipherBytes.constData(), cipherBytes.size(),
                          nullptr, (PUCHAR)saltBytes.constData(), 16,
                          (PUCHAR)clearBytes.data(), cbPlainText, &cbPlainText, 
                          BCRYPT_BLOCK_PADDING);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (!BCRYPT_SUCCESS(status))
        return QString();
    
    clearBytes.resize(cbPlainText);
    
#elif defined(Q_OS_MACOS)
    // macOS: CommonCrypto
    size_t dataOutMoved = 0;
    QByteArray clearBytes(cipherBytes.size() + kCCBlockSizeAES128, '\0');
    
    CCCryptorStatus status = CCCrypt(kCCDecrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                                     keyBytes.constData(), 32, 
                                     saltBytes.constData(),
                                     cipherBytes.constData(), cipherBytes.size(),
                                     clearBytes.data(), clearBytes.size(), &dataOutMoved);
    
    if (status != kCCSuccess)
        return QString();
    
    clearBytes.resize(dataOutMoved);
    
#else
    // Linux/Unix: OpenSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return QString();
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(keyBytes.constData()),
                           reinterpret_cast<const unsigned char*>(saltBytes.constData())) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    QByteArray clearBytes(cipherBytes.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()), '\0');
    int outLen = 0;

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
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    EVP_CIPHER_CTX_free(ctx);
    clearBytes.resize(outLen + finalLen);
#endif

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
