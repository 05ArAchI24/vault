#include "vault.h"
#include <iostream>
#include <filesystem>
#include <string>

void print_usage() {
    std::cout << "Vault Archiver v2.0 (.vlt)\n\n"
              << "Usage:\n"
              << "  vault pack <source_folder> <archive.vlt> [password]\n"
              << "  vault unpack <archive.vlt> <destination_folder> [password]\n"
              << "  vault list <archive.vlt> [password]\n"
              << "  vault info <archive.vlt> [password]\n\n"
              << "Examples:\n"
              << "  vault pack C:\\data output.vlt\n"
              << "  vault pack C:\\data output.vlt mypassword\n"
              << "  vault unpack output.vlt C:\\extracted mypassword\n"
              << "  vault list output.vlt mypassword\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::filesystem::path arg1 = argv[2];
    std::filesystem::path arg2 = (argc > 3) ? std::filesystem::path(argv[3]) : std::filesystem::path();
    std::optional<std::string> password = (argc > 4) ? std::optional<std::string>(argv[4]) : std::nullopt;

    try {
        if (command == "pack") {
            if (arg2.empty()) {
                std::cerr << "Error: archive path not specified\n";
                return 1;
            }

            if (!std::filesystem::exists(arg1) || !std::filesystem::is_directory(arg1)) {
                std::cerr << "Error: source folder does not exist\n";
                return 1;
            }

            std::cout << "Creating archive...\n";
            if (vault::createArchive(arg1, arg2, password)) {
                std::cout << "Archive created successfully: " << arg2 << "\n";
                if (password.has_value()) {
                    std::cout << "Archive is encrypted with password\n";
                }
                return 0;
            } else {
                std::cerr << "Failed to create archive\n";
                return 1;
            }
        }
        else if (command == "unpack") {
            if (arg2.empty()) {
                std::cerr << "Error: destination folder not specified\n";
                return 1;
            }

            if (!std::filesystem::exists(arg1)) {
                std::cerr << "Error: archive file does not exist\n";
                return 1;
            }

            std::cout << "Extracting archive...\n";
            if (vault::extractArchive(arg1, arg2, password)) {
                std::cout << "Archive extracted successfully to: " << arg2 << "\n";
                return 0;
            } else {
                std::cerr << "Failed to extract archive\n";
                return 1;
            }
        }
        else if (command == "list") {
            if (!std::filesystem::exists(arg1)) {
                std::cerr << "Error: archive file does not exist\n";
                return 1;
            }

            std::vector<vault::FileEntry> entries;
            if (vault::listArchive(arg1, entries, password)) {
                std::cout << "Files in archive (" << entries.size() << " total):\n";
                for (const auto& entry : entries) {
                    std::cout << "  " << entry.path << " (" << entry.originalSize << " bytes)\n";
                }
                return 0;
            } else {
                std::cerr << "Failed to list archive\n";
                return 1;
            }
        }
        else if (command == "info") {
            if (!std::filesystem::exists(arg1)) {
                std::cerr << "Error: archive file does not exist\n";
                return 1;
            }

            vault::ArchiveMetadata metadata;
            if (vault::getArchiveMetadata(arg1, metadata, password)) {
                std::cout << "Archive Information:\n";
                std::cout << "  Version: " << metadata.version << "\n";
                std::cout << "  Files: " << metadata.fileCount << "\n";
                std::cout << "  Total Size: " << metadata.totalSize << " bytes\n";
                std::cout << "  Compression: " << (metadata.compression == vault::CompressionType::ZSTD ? "ZSTD" : "None") << "\n";
                std::cout << "  Encrypted: " << (metadata.encrypted ? "Yes" : "No") << "\n";
                return 0;
            } else {
                std::cerr << "Failed to read archive metadata\n";
                return 1;
            }
        }
        else {
            std::cerr << "Unknown command: " << command << "\n";
            print_usage();
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
