#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace vault {

// Archive compression types
enum class CompressionType : uint8_t {
    NONE = 0,
    ZSTD = 1,
};

// Archive metadata
struct ArchiveMetadata {
    uint32_t version = 2;
    CompressionType compression = CompressionType::ZSTD;
    bool encrypted = false;
    uint64_t fileCount = 0;
    uint64_t totalSize = 0;
    std::vector<uint8_t> checksum;  // SHA256
    std::vector<uint8_t> salt;      // For password derivation (if encrypted)
};

// File metadata in archive
struct FileEntry {
    std::string path;
    uint64_t originalSize = 0;
    uint64_t compressedSize = 0;
    uint64_t offset = 0;
    std::vector<uint8_t> checksum;  // SHA256
};

// Split archive info
struct SplitInfo {
    std::string baseName;
    uint32_t partNumber = 0;
    uint32_t totalParts = 1;
    uint64_t partSize = 0;
};

// Create archive from folder with optional password
bool createArchive(const std::filesystem::path& sourceDir,
                   const std::filesystem::path& archivePath,
                   const std::optional<std::string>& password = std::nullopt,
                   uint64_t maxPartSize = 0);  // 0 = no split

// Extract archive to folder with optional password
bool extractArchive(const std::filesystem::path& archivePath,
                    const std::filesystem::path& destinationDir,
                    const std::optional<std::string>& password = std::nullopt);

// List files in archive
bool listArchive(const std::filesystem::path& archivePath,
                 std::vector<FileEntry>& entries,
                 const std::optional<std::string>& password = std::nullopt);

// Get archive metadata
bool getArchiveMetadata(const std::filesystem::path& archivePath,
                       ArchiveMetadata& metadata,
                       const std::optional<std::string>& password = std::nullopt);

} // namespace vault
