#pragma execution_character_set("utf-8")

#include "vault.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <filesystem>
#include <string>

// Pack controls
constexpr int IDC_PACK_TAB = 1;
constexpr int IDC_HEADER = 301;
constexpr int IDC_FOLDER_EDIT = 101;
constexpr int IDC_FOLDER_BROWSE = 102;
constexpr int IDC_ARCHIVE_EDIT = 103;
constexpr int IDC_ARCHIVE_BROWSE = 104;
constexpr int IDC_PACK_PASSWORD_EDIT = 105;
constexpr int IDC_PACK_BUTTON = 106;
constexpr int IDC_PACK_PROGRESS = 107;
constexpr int IDC_PACK_STATUS = 108;

// Extract controls
constexpr int IDC_EXTRACT_TAB = 2;
constexpr int IDC_UNPACK_ARCHIVE_EDIT = 201;
constexpr int IDC_UNPACK_ARCHIVE_BROWSE = 202;
constexpr int IDC_UNPACK_DEST_EDIT = 203;
constexpr int IDC_UNPACK_DEST_BROWSE = 204;
constexpr int IDC_UNPACK_PASSWORD_EDIT = 205;
constexpr int IDC_UNPACK_BUTTON = 206;
constexpr int IDC_UNPACK_PROGRESS = 207;
constexpr int IDC_UNPACK_STATUS = 208;

// Colors
constexpr COLORREF COLOR_HEADER = RGB(41, 128, 185);
constexpr COLORREF COLOR_TEXT = RGB(44, 62, 80);
constexpr COLORREF COLOR_LIGHT_BG = RGB(236, 240, 241);

static std::wstring GetTextFromControl(HWND parent, int controlId) {
    HWND control = GetDlgItem(parent, controlId);
    int length = GetWindowTextLengthW(control);
    std::wstring text(length, L'\0');
    GetWindowTextW(control, &text[0], length + 1);
    return text;
}

static std::string WideStringToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

static void SetTextToControl(HWND parent, int controlId, const std::wstring& text) {
    SetWindowTextW(GetDlgItem(parent, controlId), text.c_str());
}

static void ShowMessage(HWND owner, const std::wstring& title, const std::wstring& message, UINT type = MB_OK) {
    MessageBoxW(owner, message.c_str(), title.c_str(), type);
}

static std::wstring BrowseForFolder(HWND owner, const std::wstring& title) {
    BROWSEINFOW bi{};
    wchar_t buffer[MAX_PATH] = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    bi.pszDisplayName = buffer;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        return {};
    }

    std::wstring result;
    if (SHGetPathFromIDListW(pidl, buffer)) {
        result = buffer;
    }
    CoTaskMemFree(pidl);
    return result;
}

static bool PickSaveFile(HWND owner, std::wstring& file, const std::wstring& title, const wchar_t* filter, const wchar_t* defExt) {
    OPENFILENAMEW ofn{};
    wchar_t buffer[MAX_PATH] = {};
    if (!file.empty()) {
        wcsncpy_s(buffer, file.c_str(), _TRUNCATE);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }

    file = buffer;
    return true;
}

static bool PickOpenFile(HWND owner, std::wstring& file, const std::wstring& title, const wchar_t* filter) {
    OPENFILENAMEW ofn{};
    wchar_t buffer[MAX_PATH] = {};
    if (!file.empty()) {
        wcsncpy_s(buffer, file.c_str(), _TRUNCATE);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }

    file = buffer;
    return true;
}

static void PackFolder(HWND hwnd) {
    std::wstring source = GetTextFromControl(hwnd, IDC_FOLDER_EDIT);
    if (source.empty()) {
        ShowMessage(hwnd, L"Vault", L"Please select a folder to archive", MB_ICONWARNING);
        return;
    }

    std::filesystem::path sourcePath(source);
    if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_directory(sourcePath)) {
        ShowMessage(hwnd, L"Vault", L"Folder not found or is not a directory", MB_ICONERROR);
        return;
    }

    std::wstring archivePathText = GetTextFromControl(hwnd, IDC_ARCHIVE_EDIT);
    std::filesystem::path archivePath(archivePathText);
    if (archivePathText.empty()) {
        std::filesystem::path defaultArchive = sourcePath.parent_path() / (sourcePath.filename().wstring() + L".vlt");
        archivePath = defaultArchive;
        SetTextToControl(hwnd, IDC_ARCHIVE_EDIT, defaultArchive.native());
    }

    std::wstring passwordText = GetTextFromControl(hwnd, IDC_PACK_PASSWORD_EDIT);
    std::optional<std::string> password;
    if (!passwordText.empty()) {
        password = WideStringToUtf8(passwordText);
    }

    SetWindowTextW(GetDlgItem(hwnd, IDC_PACK_STATUS), L"Archiving...");
    SendMessageW(GetDlgItem(hwnd, IDC_PACK_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(GetDlgItem(hwnd, IDC_PACK_PROGRESS), PBM_SETPOS, 50, 0);

    if (vault::createArchive(sourcePath, archivePath, password)) {
        SendMessageW(GetDlgItem(hwnd, IDC_PACK_PROGRESS), PBM_SETPOS, 100, 0);
        SetWindowTextW(GetDlgItem(hwnd, IDC_PACK_STATUS), L"✓ Completed successfully");
        ShowMessage(hwnd, L"Vault", L"Archive created successfully!", MB_ICONINFORMATION);
    } else {
        SetWindowTextW(GetDlgItem(hwnd, IDC_PACK_STATUS), L"✗ Failed");
        ShowMessage(hwnd, L"Vault", L"Failed to create archive", MB_ICONERROR);
    }
}

static void UnpackArchive(HWND hwnd) {
    std::wstring archiveText = GetTextFromControl(hwnd, IDC_UNPACK_ARCHIVE_EDIT);
    if (archiveText.empty()) {
        ShowMessage(hwnd, L"Vault", L"Please select an archive file to extract", MB_ICONWARNING);
        return;
    }

    std::filesystem::path archivePath(archiveText);
    if (!std::filesystem::exists(archivePath) || !std::filesystem::is_regular_file(archivePath)) {
        ShowMessage(hwnd, L"Vault", L"Archive not found", MB_ICONERROR);
        return;
    }

    std::wstring destinationText = GetTextFromControl(hwnd, IDC_UNPACK_DEST_EDIT);
    std::filesystem::path destinationPath(destinationText);
    if (destinationText.empty()) {
        std::filesystem::path defaultDestination = archivePath.parent_path() / archivePath.stem();
        destinationPath = defaultDestination;
        SetTextToControl(hwnd, IDC_UNPACK_DEST_EDIT, defaultDestination.native());
    }

    std::wstring passwordText = GetTextFromControl(hwnd, IDC_UNPACK_PASSWORD_EDIT);
    std::optional<std::string> password;
    if (!passwordText.empty()) {
        password = WideStringToUtf8(passwordText);
    }

    // Verify archive metadata first — require password if archive is encrypted
    vault::ArchiveMetadata metadata;
    if (!vault::getArchiveMetadata(archivePath, metadata)) {
        ShowMessage(hwnd, L"Vault", L"Failed to read archive metadata", MB_ICONERROR);
        return;
    }

    if (metadata.encrypted && !password.has_value()) {
        ShowMessage(hwnd, L"Vault", L"This archive is encrypted. Please enter the password.", MB_ICONWARNING);
        return;
    }

    SetWindowTextW(GetDlgItem(hwnd, IDC_UNPACK_STATUS), L"Extracting...");
    SendMessageW(GetDlgItem(hwnd, IDC_UNPACK_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(GetDlgItem(hwnd, IDC_UNPACK_PROGRESS), PBM_SETPOS, 50, 0);

    if (vault::extractArchive(archivePath, destinationPath, password)) {
        SendMessageW(GetDlgItem(hwnd, IDC_UNPACK_PROGRESS), PBM_SETPOS, 100, 0);
        SetWindowTextW(GetDlgItem(hwnd, IDC_UNPACK_STATUS), L"✓ Extracted successfully");
        ShowMessage(hwnd, L"Vault", L"Archive extracted successfully!", MB_ICONINFORMATION);
    } else {
        SetWindowTextW(GetDlgItem(hwnd, IDC_UNPACK_STATUS), L"✗ Failed");
        if (metadata.encrypted) {
            ShowMessage(hwnd, L"Vault", L"Failed to extract archive. Wrong password or corrupt archive.", MB_ICONERROR);
        } else {
            ShowMessage(hwnd, L"Vault", L"Failed to extract archive.", MB_ICONERROR);
        }
    }
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        // Header
        CreateWindowW(L"STATIC", L"Vault Archiver", 
            WS_VISIBLE | WS_CHILD, 0, 0, 420, 60, hwnd, (HMENU)(INT_PTR)IDC_HEADER, nullptr, nullptr);
        static HFONT titleFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, L"Segoe UI");
        SendMessageW(GetDlgItem(hwnd, IDC_HEADER), WM_SETFONT, (WPARAM)titleFont, TRUE);

        // Pack Section
        CreateWindowW(L"STATIC", L"Create Archive", WS_VISIBLE | WS_CHILD, 10, 70, 300, 20, hwnd, nullptr, nullptr, nullptr);
        
        CreateWindowW(L"STATIC", L"Folder to archive:", WS_VISIBLE | WS_CHILD, 10, 95, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 114, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_FOLDER_EDIT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 300, 114, 90, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_FOLDER_BROWSE, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Archive name (.vlt):", WS_VISIBLE | WS_CHILD, 10, 145, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 164, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_ARCHIVE_EDIT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save...", WS_CHILD | WS_VISIBLE, 300, 164, 90, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_ARCHIVE_BROWSE, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Password (optional):", WS_VISIBLE | WS_CHILD, 10, 195, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD, 10, 214, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_PACK_PASSWORD_EDIT, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Pack", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 10, 245, 120, 32, 
            hwnd, (HMENU)(INT_PTR)IDC_PACK_BUTTON, nullptr, nullptr);
        
        CreateWindowW(L"PROGRESS", L"", WS_CHILD | WS_VISIBLE, 140, 245, 250, 32, 
            hwnd, (HMENU)(INT_PTR)IDC_PACK_PROGRESS, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Ready", WS_VISIBLE | WS_CHILD, 10, 285, 380, 16, 
            hwnd, (HMENU)(INT_PTR)IDC_PACK_STATUS, nullptr, nullptr);

        // Extract Section
        CreateWindowW(L"STATIC", L"Extract Archive", WS_VISIBLE | WS_CHILD, 10, 315, 300, 20, hwnd, nullptr, nullptr, nullptr);
        
        CreateWindowW(L"STATIC", L"Archive file (.vlt):", WS_VISIBLE | WS_CHILD, 10, 340, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 359, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_ARCHIVE_EDIT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Select...", WS_CHILD | WS_VISIBLE, 300, 359, 90, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_ARCHIVE_BROWSE, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Destination folder:", WS_VISIBLE | WS_CHILD, 10, 390, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 409, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_DEST_EDIT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 300, 409, 90, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_DEST_BROWSE, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Password (if encrypted):", WS_VISIBLE | WS_CHILD, 10, 440, 200, 16, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD, 10, 459, 280, 24, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_PASSWORD_EDIT, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Extract", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 10, 490, 120, 32, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_BUTTON, nullptr, nullptr);
        
        CreateWindowW(L"PROGRESS", L"", WS_CHILD | WS_VISIBLE, 140, 490, 250, 32, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_PROGRESS, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Ready", WS_VISIBLE | WS_CHILD, 10, 530, 380, 16, 
            hwnd, (HMENU)(INT_PTR)IDC_UNPACK_STATUS, nullptr, nullptr);

        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_FOLDER_BROWSE:
        {
            std::wstring folder = BrowseForFolder(hwnd, L"Select folder to archive");
            if (!folder.empty()) {
                SetTextToControl(hwnd, IDC_FOLDER_EDIT, folder);
                std::filesystem::path sourcePath(folder);
                std::filesystem::path defaultArchive = sourcePath.parent_path() / (sourcePath.filename().wstring() + L".vlt");
                SetTextToControl(hwnd, IDC_ARCHIVE_EDIT, defaultArchive.native());
            }
            return 0;
        }
        case IDC_ARCHIVE_BROWSE:
        {
            std::wstring archivePath = GetTextFromControl(hwnd, IDC_ARCHIVE_EDIT);
            const wchar_t archiveFilter[] = L"Vault archive (*.vlt)\0*.vlt\0All files (*.*)\0*.*\0\0";
            if (PickSaveFile(hwnd, archivePath, L"Save as", archiveFilter, L"vlt")) {
                SetTextToControl(hwnd, IDC_ARCHIVE_EDIT, archivePath);
            }
            return 0;
        }
        case IDC_PACK_BUTTON:
            PackFolder(hwnd);
            return 0;
        case IDC_UNPACK_ARCHIVE_BROWSE:
        {
            std::wstring archivePath = GetTextFromControl(hwnd, IDC_UNPACK_ARCHIVE_EDIT);
            const wchar_t archiveFilter[] = L"Vault archive (*.vlt)\0*.vlt\0All files (*.*)\0*.*\0\0";
            if (PickOpenFile(hwnd, archivePath, L"Select archive", archiveFilter)) {
                SetTextToControl(hwnd, IDC_UNPACK_ARCHIVE_EDIT, archivePath);
                std::filesystem::path archiveFile(archivePath);
                std::filesystem::path defaultDestination = archiveFile.parent_path() / archiveFile.stem();
                SetTextToControl(hwnd, IDC_UNPACK_DEST_EDIT, defaultDestination.native());
            }
            return 0;
        }
        case IDC_UNPACK_DEST_BROWSE:
        {
            std::wstring folder = BrowseForFolder(hwnd, L"Select destination");
            if (!folder.empty()) {
                SetTextToControl(hwnd, IDC_UNPACK_DEST_EDIT, folder);
            }
            return 0;
        }
        case IDC_UNPACK_BUTTON:
            UnpackArchive(hwnd);
            return 0;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndCtl = (HWND)lParam;
        HWND hHeader = GetDlgItem(hwnd, IDC_HEADER);
        if (hwndCtl == hHeader) {
            SetTextColor(hdcStatic, RGB(255,255,255));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)CreateSolidBrush(COLOR_HEADER);
        }

        // Status labels
        if (hwndCtl == GetDlgItem(hwnd, IDC_PACK_STATUS) || hwndCtl == GetDlgItem(hwnd, IDC_UNPACK_STATUS)) {
            SetTextColor(hdcStatic, COLOR_TEXT);
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)CreateSolidBrush(RGB(240,240,240));
        }
        break;
    }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"VaultGuiWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));

    if (!RegisterClassW(&wc)) {
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Vault - File Archiver",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        570,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
