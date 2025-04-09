#include <windows.h>
#include <string>
#include <iostream>
#include <thread>
#include <unordered_map>

HHOOK hHook;
std::string buffer;
std::unordered_map<std::string, std::string> shortcuts = {
    {"brb", "be right back"},
    {"idk", "I don't know"},
    {"omw", "on my way"},
    {"gtg", "got to go"},
    {"np", "no problem"},
    {"ema", "Hello, [Name],\r\n\r\n[body]\r\n\r\nThanks,\r\n- Roland"}
};
void copy(std::string text) {
    const char* output = text.c_str();
    const size_t len = strlen(output) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), output, len);
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}
void SendText(const std::string& text) {
    copy(text);
    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
    ip.ki.wVk = VK_CONTROL;
    ip.ki.dwFlags = 0;
    SendInput(1, &ip, sizeof(INPUT));
    ip.ki.wVk = 'V';
    ip.ki.dwFlags = 0;
    SendInput(1, &ip, sizeof(INPUT));
    ip.ki.wVk = 'V';
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
    ip.ki.wVk = VK_CONTROL;
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}
bool fix_stupid_bug = false;
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = p->vkCode;

        // Only consider standard printable keys
        if ((vkCode >= 0x30 && vkCode <= 0x5A) || (vkCode >= 0x60 && vkCode <= 0x6F)) {
            char ch = MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR);
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
                ch = toupper(ch);
            else
                ch = tolower(ch);

            buffer += ch;
            if (buffer.size() > 3)
                buffer.erase(0, 1);  // Keep only last 3 chars

            for (const auto& pair : shortcuts) {
                const std::string& key = pair.first;
                const std::string& replacement = pair.second;

                size_t keyLen = key.length();
                if (buffer.length() >= keyLen && buffer.substr(buffer.length() - keyLen) == key) {
                    std::thread([keyLen, replacement]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(30));

                        // Erase the shortcut using backspaces
                        for (size_t i = 0; i < keyLen; ++i) {
                            keybd_event(VK_BACK, 0, 0, 0);
                            keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
                        }

                        SendText(replacement);
                        }).detach();

                    buffer.clear();
                    break;
                }
            }

        }
        else if (vkCode == VK_BACK && !buffer.empty()) {
            buffer.pop_back();  // Backspace support
        }
    }

    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);

    if (!hHook) {
        std::cerr << "Failed to install hook!\n";
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) ) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);        
    }

    UnhookWindowsHookEx(hHook);
    return 0;
}
