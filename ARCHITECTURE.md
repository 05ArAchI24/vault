# Vault Archiver — Architecture & Design

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface Layer                     │
├──────────────────────────┬──────────────────────────────────┤
│   CLI (main.cpp)         │    GUI (vault_gui.cpp)           │
│  - Command parsing       │  - Win32 controls                │
│  - Usage messages        │  - File dialogs                  │
│  - Simple I/O            │  - Progress visualization        │
└──────────────────────────┴──────────────────────────────────┘
                                    │
                                    ▼
         ┌──────────────────────────────────────┐
         │    Core API Layer (vault.h)          │
         │  - createArchive()                   │
         │  - extractArchive()                  │
         │  - listArchive()                     │
         │  - getArchiveMetadata()              │
         └──────────────────────────────────────┘
                                    │
         ┌──────────────┬──────────────────┐
         ▼              ▼                  ▼
    ┌─────────┐   ┌──────────────┐  ┌──────────────┐
    │ Archiving│   │Encryption    │  │Compression   │
    │ Engine   │   │Engine        │  │Engine        │
    │(vault.cpp)   │(CryptoEngine)│  │(Compression) │
    └─────────┘   └──────────────┘  └──────────────┘
         │              │                  │
         └──────────────┴──────────────────┘
                        │
                        ▼
    ┌─────────────────────────────────────────────────┐
    │  Windows Native APIs                            │
    │  - CryptoAPI (AES-256, SHA-256)                │
    │  - File I/O (filesystem)                       │
    │  - UI (Win32 API)                              │
    └─────────────────────────────────────────────────┘
```

## Component Breakdown

### 1. **Core Archive Engine** (`vault.h` / `vault.cpp`)

**Responsibility:** VLT2 format implementation

**Public Functions:**
```cpp
bool createArchive(const std::filesystem::path& sourceDir,
                   const std::filesystem::path& archivePath,
                   const std::optional<std::string>& password,
                   uint64_t maxPartSize = 0);

bool extractArchive(const std::filesystem::path& archivePath,
                    const std::filesystem::path& destinationDir,
                    const std::optional<std::string>& password);

bool listArchive(const std::filesystem::path& archivePath,
                 std::vector<FileEntry>& entries,
                 const std::optional<std::string>& password);

bool getArchiveMetadata(const std::filesystem::path& archivePath,
                       ArchiveMetadata& metadata,
                       const std::optional<std::string>& password);
```

**Data Structures:**
- `ArchiveMetadata` — Version, compression type, file count, checksums, salt
- `FileEntry` — File path, sizes, offset, checksum
- `CompressionType` — NONE (0), ZSTD (1)

**Key Algorithms:**
1. **Pack Operation:**
   - Recursively collect all files
   - Generate salt for encryption
   - Compress each file individually (RLE)
   - Compute per-file SHA256 checksums
   - Concatenate all compressed data
   - Compute checksum of all data
   - Encrypt (if password provided)
   - Write: Magic → Version → Metadata → Entries → Data

2. **Unpack Operation:**
   - Validate magic and version
   - Read metadata
   - Require password if `metadata.encrypted`
   - Read all entries
   - Read and decrypt data (if needed)
   - Verify per-file checksums
   - Extract to destination

### 2. **Cryptography & Compression** (`vault_crypto.h` / `vault_crypto.cpp`)

**CryptoEngine:**
```cpp
class CryptoEngine {
    static std::vector<uint8_t> generateRandom(size_t size);
    static std::vector<uint8_t> deriveKey(const std::string& password, 
                                          const std::vector<uint8_t>& salt);
    static std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                        const std::vector<uint8_t>& key);
    static std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                        const std::vector<uint8_t>& key);
    static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> sha256File(const std::string& filePath);
};
```

**Implementation Details:**
- **Random Generation:** `CryptGenRandom` (Windows CryptoAPI)
- **Key Derivation:** Deterministic `SHA256(password || salt)`
  - Ensures same key from same password + salt
  - No variability issues from `CryptDeriveKey`
- **Encryption:** AES-256-CBC
  - Key: 256-bit (32 bytes)
  - IV: Random 128-bit per archive
  - Padding: PKCS#7 (automatic)
  - Output format: `IV || Ciphertext`
- **Hashing:** SHA-256 (32 bytes)

**CompressionEngine:**
```cpp
class CompressionEngine {
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, 
                                           uint64_t originalSize);
};
```

**Compression Algorithm: RLE (Run-Length Encoding)**
- **Marker:** 0xFF (255)
- **Rules:**
  - If 4+ consecutive bytes: `0xFF || count || byte`
  - If < 4: literal bytes
  - Escape: `0xFF` → `0xFF 0x00` (2 bytes)
- **Rationale:** Simple, fast, effective for structured data (logs, configs)

### 3. **CLI Interface** (`main.cpp`)

**Supported Commands:**
- `vault pack <source> <archive.vlt> [password]`
- `vault unpack <archive.vlt> <destination> [password]`
- `vault list <archive.vlt> [password]`
- `vault info <archive.vlt> [password]`

**Error Handling:**
- File not found
- Permission denied
- Archive format errors
- Decryption failures

### 4. **GUI Application** (`vault_gui.cpp`)

**Features:**
- Native Win32 API (no external GUI frameworks)
- Two-tab interface: Pack / Extract
- File/folder selection dialogs
- Password fields (masked input)
- Progress bars (placeholder)
- Status messages with icons (✓/✗)

**Key Functions:**
- `PackFolder()` — Validate inputs, call `vault::createArchive()`
- `UnpackArchive()` — Check encryption, verify password, call `vault::extractArchive()`
- `WindowProc()` — Win32 message handler
- `WideStringToUtf8()` — Convert GUI UTF-16 input to UTF-8 for crypto

**Color Scheme:**
- Header: Blue (#2980B9)
- Text: Dark gray (#2C3E50)
- Background: Light gray (#ECF0F1)

## VLT2 Archive Format Specification

### File Structure

```
Offset  Size    Field
------  ------  -------
0       4       Magic (0x3254564C "VLT2")
4       4       Version (2)

Metadata Header:
8       1       Compression Type
9       1       Encrypted (0/1)
10      8       File Count
18      8       Total Uncompressed Size
26      8       Checksum Length
34      *       Checksum (SHA256, 32 bytes typical)
34+*    8       Salt Length
42+*    *       Salt (16 bytes if encrypted, else empty)

File Entries (repeated):
        4       Path String Length
        *       Path String (UTF-8)
        8       Original File Size
        8       Compressed File Size
        8       Checksum Length
        *       Checksum (SHA256, 32 bytes)

Data Block:
        *       Concatenated compressed data
                If encrypted: IV (16 bytes) + AES-256-CBC ciphertext
```

## Security Considerations

1. **Password Derivation:**
   - Salt: 16 random bytes generated per archive
   - KDF: `SHA256(password || salt)`
   - Simplicity: Deterministic, no variability
   - *Note:* Consider PBKDF2 for production use

2. **Encryption:**
   - Algorithm: AES-256-CBC (NIST-approved)
   - IV: Random per archive (prepended to ciphertext)
   - Padding: PKCS#7 (automatic)
   - Windows CryptoAPI: Leverages OS-level security

3. **Integrity:**
   - Per-file SHA-256 checksums
   - Archive-level checksum
   - Detects corruption or tampering

4. **Limitations:**
   - No authentication tag (HMAC-SHA256 recommended for production)
   - RLE compression: Not optimal for highly random data

## Error Handling & Recovery

**Handled Errors:**
- Missing source folder
- Unreadable files
- Invalid archive format
- Version mismatch
- Checksum mismatch
- Decryption failure (wrong password)
- Missing password for encrypted archive

**Unhandled Scenarios:**
- Partial writes (disk full during extraction)
- Corrupted encryption metadata
- Concurrency (not thread-safe)

## Performance Notes

- **Compression:** RLE is fast but modest compression ratios
- **Encryption:** AES-256-CBC via CryptoAPI (hardware-accelerated on modern CPUs)
- **Hashing:** SHA-256 is incremental in `sha256File()`
- **Memory:** Loads entire compressed data into memory (consider streaming for huge archives)

## Build Requirements

- **C++23** — For modern features (std::optional, structured bindings)
- **Windows SDK 10.0+** — For CryptoAPI
- **CMake 3.15+** — For cross-platform configuration

## Testing Strategy

1. **Unit Tests** (manual):
   - Pack/unpack without password
   - Pack/unpack with password
   - List archive contents
   - Get metadata

2. **Integration Tests:**
   - GUI pack/unpack workflow
   - CLI all commands
   - Large file handling
   - Special characters in filenames

3. **Security Tests:**
   - Wrong password rejection
   - Checksum mismatch detection
   - Archive tampering detection

## Future Enhancements

1. **Archive Splitting** — Multi-part archives for size limits
2. **Progress Callbacks** — Real-time progress reporting
3. **Async Operations** — Non-blocking GUI with worker threads
4. **Enhanced GUI** — Dark theme, drag-and-drop, shell integration
5. **Better KDF** — PBKDF2 or Argon2 for password derivation
6. **Authentication** — HMAC-SHA256 for integrity (not just compression)
7. **Incremental Archive** — Update existing archives
8. **Virtual Mount** — Extract as read-only virtual drive

---

**Maintained by:** Vault Contributors  
**Last Updated:** June 2026
