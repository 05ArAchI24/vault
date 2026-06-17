#pragma once

#include <string>
#include <vector>

namespace vault {

class CryptoEngine {
public:
    static constexpr int KEY_SIZE = 32;        // 256-bit key
    static constexpr int IV_SIZE = 16;         // 128-bit IV
    static constexpr int SALT_SIZE = 16;       // 128-bit salt
    static constexpr int AUTH_TAG_SIZE = 16;   // 128-bit authentication tag

    // Generate random bytes
    static std::vector<uint8_t> generateRandom(size_t size);

    // Derive key from password using PBKDF2
    static std::vector<uint8_t> deriveKey(const std::string& password, const std::vector<uint8_t>& salt);

    // Encrypt data using AES-256-CBC
    static std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                        const std::vector<uint8_t>& key);

    // Decrypt data using AES-256-CBC
    static std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                        const std::vector<uint8_t>& key);

    // Compute SHA256 hash
    static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);

    // Compute SHA256 of file
    static std::vector<uint8_t> sha256File(const std::string& filePath);
};

// Simple RLE compression for file data
class CompressionEngine {
public:
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, uint64_t originalSize);
};

} // namespace vault
