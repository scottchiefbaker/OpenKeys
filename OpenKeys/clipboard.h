#pragma once

#include <windows.h>
#include <string>

// Get the contents of the Windows clipboard (UTF-8 encoded)
std::string getClipboardContents() {
    // Try to open the clipboard
    if (!OpenClipboard(NULL)) {
        return ""; // Failed to open clipboard
    }

    // Try Unicode format first (preferred)
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        // Lock the handle and get pointer to Unicode data
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText != NULL) {
            // Convert UTF-16 to UTF-8
            int utf8Length = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
            if (utf8Length > 0) {
                std::string utf8Text(utf8Length - 1, 0); // -1 to exclude null terminator
                WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &utf8Text[0], utf8Length, NULL, NULL);

                GlobalUnlock(hData);
                CloseClipboard();
                return utf8Text;
            }
        }
        GlobalUnlock(hData);
    }

    // Fall back to ANSI format if Unicode not available
    hData = GetClipboardData(CF_TEXT);
    if (hData != NULL) {
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText != NULL) {
            std::string text(pszText);
            GlobalUnlock(hData);
            CloseClipboard();
            return text;
        }
        GlobalUnlock(hData);
    }

    CloseClipboard();
    return ""; // No text data available
}

// Copy text to the clipboard (UTF-8 encoded)
void copyToClipboard(const std::string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, text.size() + 1);
        if (hg) {
            char* data = static_cast<char*>(GlobalLock(hg));
            if (data) {
                strcpy_s(data, text.size() + 1, text.c_str());
                GlobalUnlock(hg);
                SetClipboardData(CF_TEXT, hg);
            }
        }
        CloseClipboard();
    }
}