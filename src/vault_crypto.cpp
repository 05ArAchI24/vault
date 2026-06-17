#include "vault_crypto.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdexcept>
#include <fstream>

#pragma comment(lib, "advapi32.lib")

// Define missing constants for some SDK versions
#ifndef KP_SECRET_VALUE
#define KP_SECRET_VALUE 5
#endif

#ifndef HP_HASHVALUE
#define HP_HASHVALUE 2
#endif

namespace vault {

std::vector<uint8_t> CryptoEngine::generateRandom(size_t size) {
    HCRYPTPROV hProv = 0;
    std::vector<uint8_t> buffer(size);

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Failed to acquire cryptographic context");
    }

    if (!CryptGenRandom(hProv, static_cast<DWORD>(size), buffer.data())) {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("Failed to generate random bytes");
    }

    CryptReleaseContext(hProv, 0);
    return buffer;
}

std::vector<uint8_t> CryptoEngine::deriveKey(const std::string& password, const std::vector<uint8_t>& salt) {
    // Deterministic KDF: SHA256(password || salt)
    std::vector<uint8_t> data;
    data.reserve(password.size() + salt.size());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(password.data()), reinterpret_cast<const uint8_t*>(password.data()) + password.size());
    data.insert(data.end(), salt.begin(), salt.end());

    auto hash = sha256(data);
    if (hash.size() < KEY_SIZE) throw std::runtime_error("Derived key too small");

    std::vector<uint8_t> key(hash.begin(), hash.begin() + KEY_SIZE);
    return key;
}

std::vector<uint8_t> CryptoEngine::encrypt(const std::vector<uint8_t>& plaintext,
                                          const std::vector<uint8_t>& key) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;

    try {
        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire cryptographic context");
        }

        struct {
            BLOBHEADER header;
            DWORD keySize;
            uint8_t keyData[32];
        } keyBlob;

        keyBlob.header.bType = PLAINTEXTKEYBLOB;
        keyBlob.header.bVersion = CUR_BLOB_VERSION;
        keyBlob.header.reserved = 0;
        keyBlob.header.aiKeyAlg = CALG_AES_256;
        keyBlob.keySize = KEY_SIZE;
        std::fill(std::begin(keyBlob.keyData), std::end(keyBlob.keyData), 0);
        std::copy(key.begin(), key.end(), keyBlob.keyData);

        if (!CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob), 
                           sizeof(keyBlob), 0, 0, &hKey)) {
            throw std::runtime_error("Failed to import key");
        }

        // Use CBC mode
        DWORD dwMode = CRYPT_MODE_CBC;
        if (!CryptSetKeyParam(hKey, KP_MODE, reinterpret_cast<BYTE*>(&dwMode), 0)) {
            throw std::runtime_error("Failed to set cipher mode");
        }

        // Generate random IV
        auto iv = generateRandom(IV_SIZE);
        if (!CryptSetKeyParam(hKey, KP_IV, iv.data(), 0)) {
            throw std::runtime_error("Failed to set IV");
        }

        // Prepare buffer with room for padding
        DWORD dataLen = static_cast<DWORD>(plaintext.size());
        DWORD bufLen = dataLen + 16; // AES block size padding room
        std::vector<uint8_t> buffer(bufLen);
        if (dataLen > 0) memcpy(buffer.data(), plaintext.data(), dataLen);

        if (!CryptEncrypt(hKey, 0, TRUE, 0, buffer.data(), &dataLen, bufLen)) {
            throw std::runtime_error("Encryption failed");
        }

        buffer.resize(dataLen);

        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);

        // Result: IV + ciphertext
        std::vector<uint8_t> result;
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), buffer.begin(), buffer.end());

        return result;
    }
    catch (...) {
        if (hKey) CryptDestroyKey(hKey);
        if (hProv) CryptReleaseContext(hProv, 0);
        throw;
    }
}

std::vector<uint8_t> CryptoEngine::decrypt(const std::vector<uint8_t>& encrypted,
                                          const std::vector<uint8_t>& key) {
    if (encrypted.size() < IV_SIZE) {
        throw std::runtime_error("Invalid encrypted data");
    }
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;

    try {
        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire cryptographic context");
        }

        struct {
            BLOBHEADER header;
            DWORD keySize;
            uint8_t keyData[32];
        } keyBlob;

        keyBlob.header.bType = PLAINTEXTKEYBLOB;
        keyBlob.header.bVersion = CUR_BLOB_VERSION;
        keyBlob.header.reserved = 0;
        keyBlob.header.aiKeyAlg = CALG_AES_256;
        keyBlob.keySize = KEY_SIZE;
        std::fill(std::begin(keyBlob.keyData), std::end(keyBlob.keyData), 0);
        std::copy(key.begin(), key.end(), keyBlob.keyData);

        if (!CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob), 
                           sizeof(keyBlob), 0, 0, &hKey)) {
            throw std::runtime_error("Failed to import key");
        }

        // Use CBC mode
        DWORD dwMode = CRYPT_MODE_CBC;
        if (!CryptSetKeyParam(hKey, KP_MODE, reinterpret_cast<BYTE*>(&dwMode), 0)) {
            throw std::runtime_error("Failed to set cipher mode");
        }

        auto iv = std::vector<uint8_t>(encrypted.begin(), encrypted.begin() + IV_SIZE);
        if (!CryptSetKeyParam(hKey, KP_IV, iv.data(), 0)) {
            throw std::runtime_error("Failed to set IV");
        }

        std::vector<uint8_t> buffer(encrypted.begin() + IV_SIZE, encrypted.end());
        DWORD dataLen = static_cast<DWORD>(buffer.size());
        if (!CryptDecrypt(hKey, 0, TRUE, 0, buffer.data(), &dataLen)) {
            throw std::runtime_error("Decryption failed");
        }

        buffer.resize(dataLen);

        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);

        return buffer;
    }
    catch (...) {
        if (hKey) CryptDestroyKey(hKey);
        if (hProv) CryptReleaseContext(hProv, 0);
        throw;
    }
}

std::vector<uint8_t> CryptoEngine::sha256(const std::vector<uint8_t>& data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    try {
        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire cryptographic context");
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            throw std::runtime_error("Failed to create hash");
        }

        if (!CryptHashData(hHash, const_cast<BYTE*>(data.data()), 
                          static_cast<DWORD>(data.size()), 0)) {
            throw std::runtime_error("Failed to hash data");
        }

        std::vector<uint8_t> hashValue(32);  // SHA256 = 32 bytes
        DWORD hashSize = 32;
        if (!CryptGetHashParam(hHash, HP_HASHVALUE, hashValue.data(), &hashSize, 0)) {
            throw std::runtime_error("Failed to get hash value");
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return hashValue;
    }
    catch (...) {
        if (hHash) CryptDestroyHash(hHash);
        if (hProv) CryptReleaseContext(hProv, 0);
        throw;
    }
}

std::vector<uint8_t> CryptoEngine::sha256File(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for hashing: " + filePath);
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    try {
        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire cryptographic context");
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            throw std::runtime_error("Failed to create hash");
        }

        const size_t bufferSize = 8192;
        std::vector<char> buffer(bufferSize);

        while (file.read(buffer.data(), bufferSize) || file.gcount() > 0) {
            if (!CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer.data()), 
                              static_cast<DWORD>(file.gcount()), 0)) {
                throw std::runtime_error("Failed to hash file data");
            }
        }

        std::vector<uint8_t> hashValue(32);
        DWORD hashSize = 32;
        if (!CryptGetHashParam(hHash, HP_HASHVALUE, hashValue.data(), &hashSize, 0)) {
            throw std::runtime_error("Failed to get hash value");
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return hashValue;
    }
    catch (...) {
        if (hHash) CryptDestroyHash(hHash);
        if (hProv) CryptReleaseContext(hProv, 0);
        throw;
    }
}

// Simple RLE compression
std::vector<uint8_t> CompressionEngine::compress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> compressed;
    
    if (data.empty()) return compressed;

    for (size_t i = 0; i < data.size(); ) {
        uint8_t byte = data[i];
        size_t count = 1;

        while (i + count < data.size() && data[i + count] == byte && count < 255) {
            count++;
        }

        if (count >= 4) {
            compressed.push_back(255);  // RLE marker
            compressed.push_back(static_cast<uint8_t>(count));
            compressed.push_back(byte);
            i += count;
        } else {
            for (size_t j = 0; j < count; ++j) {
                compressed.push_back(byte);
                if (byte == 255) compressed.push_back(0);  // Escape
            }
            i += count;
        }
    }

    return compressed;
}

std::vector<uint8_t> CompressionEngine::decompress(const std::vector<uint8_t>& data, uint64_t originalSize) {
    std::vector<uint8_t> decompressed;
    decompressed.reserve(originalSize);

    for (size_t i = 0; i < data.size(); ) {
        if (data[i] == 255 && i + 2 < data.size()) {
            size_t count = data[i + 1];
            uint8_t byte = data[i + 2];
            for (size_t j = 0; j < count; ++j) {
                decompressed.push_back(byte);
            }
            i += 3;
        } else {
            decompressed.push_back(data[i]);
            if (data[i] == 255 && i + 1 < data.size() && data[i + 1] == 0) {
                i += 2;  // Skip escape
            } else {
                i++;
            }
        }
    }

    return decompressed;
}

} // namespace vault
