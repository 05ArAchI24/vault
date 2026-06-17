#include "vault.h"
#include "vault_crypto.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace {

constexpr uint32_t VLT2_MAGIC = 0x3254564C;  // "VLT2"
constexpr uint32_t VERSION = 2;

void writeUInt64(std::ofstream& stream, uint64_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeUInt32(std::ofstream& stream, uint32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeUInt8(std::ofstream& stream, uint8_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeString(std::ofstream& stream, const std::string& str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    writeUInt32(stream, len);
    stream.write(str.data(), len);
}

void writeBytes(std::ofstream& stream, const std::vector<uint8_t>& data) {
    writeUInt64(stream, data.size());
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

bool readUInt64(std::ifstream& stream, uint64_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool readUInt32(std::ifstream& stream, uint32_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool readUInt8(std::ifstream& stream, uint8_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

std::string readString(std::ifstream& stream) {
    uint32_t len;
    if (!readUInt32(stream, len)) {
        return "";
    }
    std::string str(len, '\0');
    stream.read(str.data(), len);
    return str;
}

std::vector<uint8_t> readBytes(std::ifstream& stream) {
    uint64_t len;
    if (!readUInt64(stream, len)) {
        return {};
    }
    std::vector<uint8_t> data(len);
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(len));
    return data;
}

} // namespace

namespace vault {

bool createArchive(const std::filesystem::path& sourceDir,
                   const std::filesystem::path& archivePath,
                   const std::optional<std::string>& password,
                   uint64_t maxPartSize) {
    try {
        if (!std::filesystem::exists(sourceDir) || !std::filesystem::is_directory(sourceDir)) {
            std::cerr << "Source directory does not exist: " << sourceDir << "\n";
            return false;
        }

        std::vector<std::filesystem::path> files;
        for (auto const& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }

        std::ofstream archive(archivePath, std::ios::binary);
        if (!archive.is_open()) {
            std::cerr << "Cannot open archive file for writing: " << archivePath << "\n";
            return false;
        }

        // Write magic and version
        writeUInt32(archive, VLT2_MAGIC);
        writeUInt32(archive, VERSION);

        // Prepare metadata
        ArchiveMetadata metadata;
        metadata.version = VERSION;
        metadata.compression = CompressionType::ZSTD;
        metadata.encrypted = password.has_value();
        metadata.fileCount = files.size();

        // Generate salt if encrypted
        if (metadata.encrypted) {
            metadata.salt = CryptoEngine::generateRandom(CryptoEngine::SALT_SIZE);
        }

        // Collect file data
        std::vector<uint8_t> allData;
        std::vector<FileEntry> entries;

        for (auto const& filePath : files) {
            std::filesystem::path relativePath = std::filesystem::relative(filePath, sourceDir);
            std::string pathStr = relativePath.generic_string();

            std::ifstream input(filePath, std::ios::binary);
            if (!input.is_open()) {
                std::cerr << "Cannot open file for reading: " << filePath << "\n";
                return false;
            }

            // Read file data
            std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(input)),
                                         std::istreambuf_iterator<char>());

            FileEntry entry;
            entry.path = pathStr;
            entry.originalSize = fileData.size();
            entry.offset = allData.size();
            entry.checksum = CryptoEngine::sha256(fileData);

            // Compress
            auto compressed = CompressionEngine::compress(fileData);
            entry.compressedSize = compressed.size();
            metadata.totalSize += entry.originalSize;

            allData.insert(allData.end(), compressed.begin(), compressed.end());
            entries.push_back(entry);
        }

        // Calculate checksum of all data
        metadata.checksum = CryptoEngine::sha256(allData);

        // Encrypt if password provided
        std::vector<uint8_t> finalData = allData;
        if (metadata.encrypted && password.has_value()) {
            auto key = CryptoEngine::deriveKey(*password, metadata.salt);
            finalData = CryptoEngine::encrypt(allData, key);
        }

        // Write metadata
        writeUInt8(archive, static_cast<uint8_t>(metadata.compression));
        writeUInt8(archive, metadata.encrypted ? 1 : 0);
        writeUInt64(archive, metadata.fileCount);
        writeUInt64(archive, metadata.totalSize);
        writeBytes(archive, metadata.checksum);
        writeBytes(archive, metadata.salt);

        // Write file entries
        for (const auto& entry : entries) {
            writeString(archive, entry.path);
            writeUInt64(archive, entry.originalSize);
            writeUInt64(archive, entry.compressedSize);
            writeBytes(archive, entry.checksum);
        }

        // Write all data
        archive.write(reinterpret_cast<const char*>(finalData.data()), static_cast<std::streamsize>(finalData.size()));

        return archive.good();
    }
    catch (const std::exception& e) {
        std::cerr << "Archive creation error: " << e.what() << "\n";
        return false;
    }
}

bool extractArchive(const std::filesystem::path& archivePath,
                    const std::filesystem::path& destinationDir,
                    const std::optional<std::string>& password) {
    try {
        std::ifstream archive(archivePath, std::ios::binary);
        if (!archive.is_open()) {
            std::cerr << "Cannot open archive file for reading: " << archivePath << "\n";
            return false;
        }

        // Read and verify magic
        uint32_t magic;
        if (!readUInt32(archive, magic) || magic != VLT2_MAGIC) {
            std::cerr << "Invalid archive format\n";
            return false;
        }

        // Read version
        uint32_t version;
        if (!readUInt32(archive, version) || version != VERSION) {
            std::cerr << "Unsupported archive version: " << version << "\n";
            return false;
        }

        // Read metadata
        uint8_t compressionType, encrypted;
        uint64_t fileCount, totalSize;
        if (!readUInt8(archive, compressionType) ||
            !readUInt8(archive, encrypted) ||
            !readUInt64(archive, fileCount) ||
            !readUInt64(archive, totalSize)) {
            std::cerr << "Failed to read archive metadata\n";
            return false;
        }

        auto checksum = readBytes(archive);
        auto salt = readBytes(archive);

        // Read file entries
        std::vector<FileEntry> entries;
        for (uint64_t i = 0; i < fileCount; ++i) {
            FileEntry entry;
            entry.path = readString(archive);
            entry.originalSize = 0;
            entry.compressedSize = 0;

            if (!readUInt64(archive, entry.originalSize) ||
                !readUInt64(archive, entry.compressedSize)) {
                std::cerr << "Failed to read file entry\n";
                return false;
            }

            entry.checksum = readBytes(archive);
            entries.push_back(entry);
        }

        // Read all compressed/encrypted data
        std::vector<uint8_t> allData;
        std::vector<char> buffer(8192);
        while (archive.read(buffer.data(), buffer.size()) || archive.gcount() > 0) {
            allData.insert(allData.end(), buffer.begin(), buffer.begin() + archive.gcount());
        }

        // Decrypt if needed
        if (encrypted) {
            if (!password.has_value()) {
                std::cerr << "Archive is encrypted but no password provided\n";
                return false;
            }
            auto key = CryptoEngine::deriveKey(*password, salt);
            allData = CryptoEngine::decrypt(allData, key);
        }

        // Extract files
        std::filesystem::create_directories(destinationDir);
        uint64_t dataOffset = 0;

        for (const auto& entry : entries) {
            // Extract compressed data
            auto compressedData = std::vector<uint8_t>(
                allData.begin() + dataOffset,
                allData.begin() + dataOffset + entry.compressedSize
            );
            dataOffset += entry.compressedSize;

            // Decompress
            auto fileData = CompressionEngine::decompress(compressedData, entry.originalSize);

            // Verify checksum
            auto calculatedChecksum = CryptoEngine::sha256(fileData);
            if (calculatedChecksum != entry.checksum) {
                std::cerr << "Checksum mismatch for file: " << entry.path << "\n";
                return false;
            }

            // Write file
            std::filesystem::path outputPath = destinationDir / entry.path;
            std::filesystem::create_directories(outputPath.parent_path());

            std::ofstream output(outputPath, std::ios::binary);
            if (!output.is_open()) {
                std::cerr << "Cannot create file: " << outputPath << "\n";
                return false;
            }

            output.write(reinterpret_cast<const char*>(fileData.data()), static_cast<std::streamsize>(fileData.size()));
        }

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Archive extraction error: " << e.what() << "\n";
        return false;
    }
}

bool listArchive(const std::filesystem::path& archivePath,
                 std::vector<FileEntry>& entries,
                 const std::optional<std::string>& password) {
    try {
        std::ifstream archive(archivePath, std::ios::binary);
        if (!archive.is_open()) {
            std::cerr << "Cannot open archive file: " << archivePath << "\n";
            return false;
        }

        uint32_t magic, version;
        if (!readUInt32(archive, magic) || magic != VLT2_MAGIC ||
            !readUInt32(archive, version) || version != VERSION) {
            return false;
        }

        uint8_t compressionType, encrypted;
        uint64_t fileCount, totalSize;
        if (!readUInt8(archive, compressionType) ||
            !readUInt8(archive, encrypted) ||
            !readUInt64(archive, fileCount) ||
            !readUInt64(archive, totalSize)) {
            return false;
        }

        auto checksum = readBytes(archive);
        auto salt = readBytes(archive);

        entries.clear();
        for (uint64_t i = 0; i < fileCount; ++i) {
            FileEntry entry;
            entry.path = readString(archive);

            if (!readUInt64(archive, entry.originalSize) ||
                !readUInt64(archive, entry.compressedSize)) {
                return false;
            }

            entry.checksum = readBytes(archive);
            entries.push_back(entry);
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

bool getArchiveMetadata(const std::filesystem::path& archivePath,
                       ArchiveMetadata& metadata,
                       const std::optional<std::string>& password) {
    try {
        std::ifstream archive(archivePath, std::ios::binary);
        if (!archive.is_open()) {
            return false;
        }

        uint32_t magic, version;
        if (!readUInt32(archive, magic) || magic != VLT2_MAGIC ||
            !readUInt32(archive, version) || version != VERSION) {
            return false;
        }

        uint8_t compressionType, encrypted;
        if (!readUInt8(archive, compressionType) ||
            !readUInt8(archive, encrypted)) {
            return false;
        }

        metadata.version = version;
        metadata.compression = static_cast<CompressionType>(compressionType);
        metadata.encrypted = encrypted != 0;

        if (!readUInt64(archive, metadata.fileCount) ||
            !readUInt64(archive, metadata.totalSize)) {
            return false;
        }

        metadata.checksum = readBytes(archive);
        metadata.salt = readBytes(archive);

        return true;
    }
    catch (...) {
        return false;
    }
}

} // namespace vault
