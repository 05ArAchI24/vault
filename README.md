# 🔐 Vault Archiver

> **Production-grade file archiver with AES-256 encryption** — fast, secure, and lightweight.

![Version](https://img.shields.io/badge/version-2.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)

## ✨ Features

- 🔒 **AES-256-CBC Encryption** — Military-grade security using Windows CryptoAPI
- 📦 **Efficient Compression** — RLE compression for faster archiving
- 🖥️ **Dual Interface** — CLI (`vault.exe`) + Native Win32 GUI (`vault_gui.exe`)
- 🛡️ **Integrity Verification** — Per-file SHA-256 checksums
- 🔑 **Password Protection** — Deterministic key derivation with salt
- 📋 **Metadata Support** — Archive info, file listing, and versioning
- ⚡ **No External Dependencies** — Uses only Windows native APIs

## 🚀 Quick Start

### Build from Source

```bash
git clone https://github.com/05ArAchI24/vault.git
cd vault
mkdir build && cd build
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Command Line

```bash
# Create archive
vault.exe pack C:\my_data output.vlt

# Create encrypted archive
vault.exe pack C:\my_data output.vlt secret_password

# Extract archive
vault.exe unpack output.vlt C:\extracted

# Extract encrypted archive
vault.exe unpack output.vlt C:\extracted secret_password

# List files
vault.exe list output.vlt [password]

# Get archive info
vault.exe info output.vlt [password]
```

### GUI Application

Simply run `vault_gui.exe` for an intuitive Win32 interface:
- **Create Archive** — Select folder, optional password
- **Extract Archive** — Select .vlt file, auto-detect encryption
- **Real-time Feedback** — Progress bars and status messages

## 📋 Archive Format (VLT2)

```
[Header]
  - Magic: 0x3254564C ("VLT2")
  - Version: 2
  
[Metadata]
  - Compression type (1 byte)
  - Encrypted flag (1 byte)
  - File count (8 bytes)
  - Total size (8 bytes)
  - Checksum SHA256 (variable)
  - Salt (16 bytes, if encrypted)

[File Entries]
  For each file:
    - Path string (variable)
    - Original size (8 bytes)
    - Compressed size (8 bytes)
    - Checksum SHA256 (variable)

[Data Block]
  - Concatenated compressed (and optionally encrypted) file data
  - If encrypted: IV (16 bytes) + Ciphertext
```

## 🔐 Encryption Details

- **Algorithm:** AES-256-CBC
- **Key Derivation:** SHA256(password || salt) → 256-bit key
- **IV:** Random 16 bytes per archive (prepended to ciphertext)
- **Padding:** PKCS#7 (automatic, handled by CryptoAPI)

## 📁 Project Structure

```
vault/
├── src/
│   ├── vault.h               # Public API header
│   ├── vault.cpp             # Archive read/write implementation
│   ├── vault_crypto.h        # Crypto/compression interfaces
│   ├── vault_crypto.cpp      # Windows CryptoAPI + RLE
│   ├── main.cpp              # CLI entry point
│   └── vault_gui.cpp         # Win32 GUI application
├── CMakeLists.txt            # Build configuration
├── README.md                 # This file
├── LICENSE                   # MIT License
└── .gitignore               # Git ignore rules
```

## 🛠️ API Usage

```cpp
#include "vault.h"
#include <optional>

// Create archive
vault::createArchive(
    std::filesystem::path("C:\\data"),
    std::filesystem::path("archive.vlt"),
    std::optional<std::string>("password")
);

// Extract archive
vault::extractArchive(
    std::filesystem::path("archive.vlt"),
    std::filesystem::path("C:\\output"),
    std::optional<std::string>("password")
);

// List files
std::vector<vault::FileEntry> entries;
vault::listArchive(std::filesystem::path("archive.vlt"), entries);

// Get metadata
vault::ArchiveMetadata metadata;
vault::getArchiveMetadata(std::filesystem::path("archive.vlt"), metadata);
```

## 🧪 Testing

```bash
# Basic pack/unpack test
vault.exe pack C:\test_data test.vlt
vault.exe unpack test.vlt C:\test_extracted

# Encrypted archive
vault.exe pack C:\test_data secure.vlt mypassword
vault.exe unpack secure.vlt C:\extracted mypassword

# Verify it requires password
vault.exe unpack secure.vlt C:\fail_test  # Will fail
```

## ✅ Verified Features

- ✅ Password enforcement (encrypted archives require password)
- ✅ AES-256 encryption/decryption via Windows CryptoAPI
- ✅ RLE compression (4+ byte runs)
- ✅ SHA-256 checksums per file
- ✅ Archive metadata versioning
- ✅ GUI password field with validation
- ✅ Both CLI and GUI working correctly

## 📦 Dependencies

- **Windows SDK 10.0+** — for CryptoAPI
- **C++23 compiler** — MSVC 2022+
- **CMake 3.15+** — for build configuration

No external libraries needed! 🎉

## 🚦 Future Enhancements

- [ ] Archive splitting (multi-part archives)
- [ ] Progress callbacks for large operations
- [ ] Dark theme GUI
- [ ] Drag-and-drop support
- [ ] .vlt file icon + shell integration
- [ ] Extract-in-place with incremental verification
- [ ] Archive mounting as virtual drive

## 📄 License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

## 👨‍💻 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📧 Contact

Questions? Open an issue on GitHub or contact the maintainers.

---

**Made with ❤️ for secure file archiving on Windows**
