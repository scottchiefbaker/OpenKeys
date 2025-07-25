#pragma once

#include "resource.h"
#include <shellapi.h>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

// Global log file stream
std::ofstream LOG;

// Prototypes for helper functions
std::string wstringToString(const std::wstring& wstr);
size_t log_line(std::string line);

///////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////

// Open a folder in Windows Explorer
void OpenFolderInExplorer(const wchar_t* folderPath)
{
    ShellExecuteW(NULL, L"open", folderPath, NULL, NULL, SW_SHOWNORMAL);
}

// Informational message box
void InfoMessage(LPCWSTR title, LPCWSTR contents) {
    MessageBox(NULL, contents, title, MB_OK | MB_ICONINFORMATION);
}

// Informational message box with default title
void InfoMessage(LPCWSTR contents) {
    InfoMessage(L"Info", contents);
}

// Warning message box
void WarningMessage(LPCWSTR title, LPCWSTR contents) {
    MessageBox(NULL, contents, title, MB_OK | MB_ICONWARNING);
}

// Error message popup message
void ErrorMessage(LPCWSTR title, LPCWSTR contents) {
    MessageBox(NULL, contents, title, MB_OK | MB_ICONERROR);
}

// Error message popup message
void ErrorMessage(int error_id, LPCWSTR contents) {
    std::wstring title = L"Error #" + std::to_wstring(error_id);
    ErrorMessage(title.c_str(), contents);

    log_line("Error #" + std::to_string(error_id) + ": " + wstringToString(contents));
}

// Get a simple datestring suitable for putting in a log file
std::string get_datetime_string() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now); // Windows

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

// Add a line to the open log file
size_t log_line(std::string line) {
    if (!LOG) {
        std::cerr << "Log file not ready #52195.\n";
        return 0;
    }

    std::string date_str = get_datetime_string() + ": ";

    LOG << date_str << line << '\n';

    LOG.flush();

    return 1;
}

// Add a wstr line to the open log file
size_t log_line(std::wstring line) {
    if (!LOG) {
        ErrorMessage(65473, L"Log file not ready #95874.");
        return 0;
    }

    std::string tmp_str = get_datetime_string() + ": " + wstringToString(line);

    LOG << tmp_str << '\n';

    LOG.flush();

    return 1;
}

LPCSTR WcharToLpcstr(const WCHAR* wideString) {
    if (!wideString) {
        return nullptr;
    }

    // Determine the required buffer size for the multibyte string
    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideString, -1, NULL, 0, NULL, NULL);
    if (bufferSize == 0) {
        // Handle error
        return nullptr;
    }

    // Allocate buffer for the multibyte string
    std::vector<char> buffer(bufferSize);

    // Perform the conversion
    WideCharToMultiByte(CP_ACP, 0, wideString, -1, buffer.data(), bufferSize, NULL, NULL);

    // Return a pointer to the converted string (ensure its lifetime is managed)
    // Note: Returning a raw pointer to a local vector's data is unsafe if the vector goes out of scope.
    // For practical use, consider returning std::string or dynamically allocating and managing memory.
    return buffer.data(); // This is for demonstration; use with caution.
}


// Convert a UTF-8 string to a wide string
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);

    return result;
}

// Convert a wide string to a UTF-8 string
std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);

    return str;
}

// Get the REAL path for %APPDATA%
std::wstring GetAppDataDir() {
    // Read the %APPDATA% environment variable
    wchar_t buffer[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);  // Note the 'W' suffix

    return buffer;
}

// Get the directory of the currently running executable
std::wstring GetExecutableDirectory() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        // Handle error
        return L"";
    }

    std::wstring fullPath(path);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return fullPath.substr(0, lastSlash);
    }

    return fullPath; // fallback to full path if no slash found
}

// Add the OpenKeys app to the startup programs
void AddToStartup() {
    std::wstring progPath = GetExecutableDirectory() + L"\\OpenKeys.exe";
    HKEY hkey = NULL;
    LONG createStatus = RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey); //Creates a key
    LONG status = RegSetValueEx(hkey, L"MyApp", 0, REG_SZ, (BYTE*)progPath.c_str(), static_cast<DWORD>((progPath.size() + 1) * sizeof(wchar_t)));
}

// Check if a directory exists on the filesystem
bool DirectoryExists(const std::wstring& dirPath) {
    DWORD attribs = GetFileAttributesW(dirPath.c_str());
    return (attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY);
}