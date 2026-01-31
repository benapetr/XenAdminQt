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

#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include "../xenlib_global.h"
#include <QtCore/QString>
#include <QtCore/QByteArray>

class XENLIB_EXPORT EncryptionUtils
{
    public:
        static QString HashPassword(const QString& password);
        static QByteArray Encrypt(const QByteArray& data, const QString& key);
        static QByteArray Decrypt(const QByteArray& data, const QString& key);
        static QString GenerateSessionKey();

        // Additional utility methods
        static QString EncryptString(const QString& text, const QString& key);
        static QString DecryptString(const QString& encryptedText, const QString& key);
        static QString ProtectString(const QString& text);
        static QString UnprotectString(const QString& protectedText);
        static QString GenerateSalt(int length = 32);
        static QString HashPasswordWithSalt(const QString& password, const QString& salt);

        /**
         * @brief Returns a secure hash of the given input string.
         * 
         * Matches C# EncryptionUtils.ComputeHash()
         * @param input The string to hash
         * @return The secure hash as byte array
         */
        static QByteArray ComputeHash(const QString& input);

        /**
         * @brief Compare two byte arrays for equality.
         * 
         * Matches C# Helpers.ArrayElementsEqual()
         * @param a First byte array
         * @param b Second byte array
         * @return True if arrays are equal, false otherwise
         */
        static bool ArrayElementsEqual(const QByteArray& a, const QByteArray& b);

        /**
         * @brief Encrypt a string using AES-256-CBC with a key.
         * 
         * Matches C# EncryptionUtils.EncryptString()
         * @param clearString The string to encrypt
         * @param keyBytes The encryption key (should be 32 bytes for AES-256)
         * @return Base64 encoded cipher text with format "cipher,salt"
         */
        static QString EncryptStringWithKey(const QString& clearString, const QByteArray& keyBytes);

        /**
         * @brief Decrypt a string that was encrypted with EncryptStringWithKey.
         * 
         * Matches C# EncryptionUtils.DecryptString()
         * @param cipherText64 The base64 encoded cipher text (format "cipher,salt")
         * @param keyBytes The decryption key (should be 32 bytes for AES-256)
         * @return The decrypted string
         */
        static QString DecryptStringWithKey(const QString& cipherText64, const QByteArray& keyBytes);

    private:
        EncryptionUtils() = delete;
};

#endif // ENCRYPTION_H
