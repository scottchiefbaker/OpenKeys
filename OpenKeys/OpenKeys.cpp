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
    {"np", "no problem"}
};
void SendText(const std::string& text) {
    for (char c : text) {
        SHORT vk = VkKeyScanA(c);
        if (vk == -1) continue;

        // Press key
        KEYBDINPUT kb = {};
        INPUT input = {};
        kb.wVk = LOBYTE(vk);
        kb.dwFlags = 0;
        input.type = INPUT_KEYBOARD;
        input.ki = kb;
        SendInput(1, &input, sizeof(INPUT));

        // Release key
        kb.dwFlags = KEYEVENTF_KEYUP;
        input.ki = kb;
        SendInput(1, &input, sizeof(INPUT));
    }
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
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);        
    }

    UnhookWindowsHookEx(hHook);
    return 0;
}
