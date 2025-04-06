#include <iostream>
#include <Windows.h>
#include <winuser.h>
#include <string>
#include <conio.h>
HKL keyboard;
struct keystate {
    WORD vk;
    bool shift;
};
void keyStateDown(WORD key) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = key;
    input.ki.dwFlags = KEYEVENTF_SCANCODE;

    SendInput(1, &input, sizeof(INPUT));
}
void keyStateUp(WORD key) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = key;
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
}
keystate getKS(char key) { 
    switch (key) {
    case '1': return { 2 };
    case '!': return { 2, true };
    case '2': return { 3 };
    case '@': return { 3, true };
    case '3': return { 4 };
    case '#': return { 4, true };
    case '4': return { 5 };
    case '$': return { 5, true };
    case '5': return { 6 };
    case '%': return { 6, true };
    case '6': return { 7 };
    case '^': return { 7, true };
    case '7': return { 8 };
    case '&': return { 8, true };
    case '8': return { 9 };
    case '*': return { 9, true };
    case '9': return { 10 };
    case '(': return { 10, true };
    case '0': return { 11 };
    case ')': return { 11, true };
    case '-': return { 12 };
    case '_': return { 12, true };
    case '=': return { 13 };
    case '+': return { 13, true };
    case 'q': return { 16 };
    case 'Q': return { 16, true };
    case 'w': return { 17 };
    case 'W': return { 17, true };
    case 'e': return { 18 };
    case 'E': return { 18, true };
    case 'r': return { 19 };
    case 'R': return { 19, true };
    case 't': return { 20 };
    case 'T': return { 20, true };
    case 'y': return { 21 };
    case 'Y': return { 21, true };
    case 'u': return { 22 };
    case 'U': return { 22, true };
    case 'i': return { 23 };
    case 'I': return { 23, true };
    case 'o': return { 24 };
    case 'O': return { 24, true };
    case 'p': return { 25 };
    case 'P': return { 25, true };
    case '[': return { 26 };
    case '{': return { 26, true };
    case ']': return { 27 };
    case '}': return { 27, true };
    case '\n': return { 28 };
    case 'a': return { 30 };
    case 'A': return { 30, true };
    case 's': return { 31 };
    case 'S': return { 31, true };
    case 'd': return { 32 };
    case 'D': return { 32, true };
    case 'f': return { 33 };
    case 'F': return { 33, true };
    case 'g': return { 34 };
    case 'G': return { 34, true };
    case 'h': return { 35 };
    case 'H': return { 35, true };
    case 'j': return { 36 };
    case 'J': return { 36, true };
    case 'k': return { 37 };
    case 'K': return { 37, true };
    case 'l': return { 38 };
    case 'L': return { 38, true };
    case ';': return { 39 };
    case ':': return { 39, true };
    case 39: return { 40 };
    case '"': return { 40, true };
    case '~': return { 41, true };
    case '\\': return { 43 };
    case '|': return { 43, true };
    case 'z': return { 44 };
    case 'Z': return { 44, true };
    case 'x': return { 45 };
    case 'X': return { 45, true };
    case 'c': return { 46 };
    case 'C': return { 46, true };
    case 'v': return { 47 };
    case 'V': return { 47, true };
    case 'b': return { 48 };
    case 'B': return { 48, true };
    case 'n': return { 49 };
    case 'N': return { 49, true };
    case 'm': return { 50 };
    case 'M': return { 50, true };
    case ',': return { 51 };
    case '<': return { 51, true };
    case '.': return { 52 };
    case '>': return { 52, true };
    case '/': return { 53 };
    case '?': return { 53, true };
    case ' ': return { 57, true };
    }
    return { 0 };
}
void keyPress(keystate ks) {
    if (ks.shift) {
        keyStateDown({ 42 });
    }
    keyStateDown(ks.vk);
    keyStateUp(ks.vk);
    if (ks.shift) {
        keyStateUp({ 42 });
    }
}
void sendText(std::string text) {
    const char* c = text.c_str();
    for (int i = 0; i < text.length(); i++) {
        keystate ks = getKS(c[i]);
        keyPress(ks);
    }
}
void copyToClipboard(std::string text) {
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
void paste(std::string text) {
    copyToClipboard(text);

    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
    ip.ki.wVk = VK_CONTROL;
    ip.ki.dwFlags = 0; // 0 for key press
    SendInput(1, &ip, sizeof(INPUT));

    // Press the "V" key
    ip.ki.wVk = 'V';
    ip.ki.dwFlags = 0; // 0 for key press
    SendInput(1, &ip, sizeof(INPUT));

    // Release the "V" key
    ip.ki.wVk = 'V';
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));

    // Release the "Ctrl" key
    ip.ki.wVk = VK_CONTROL;
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}
HHOOK _k_hook;
LRESULT __stdcall k_Callback1(int nCode, WPARAM wParam, LPARAM lParam)
{
    PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)lParam;
    //a key was pressed
    if (wParam == WM_KEYDOWN && nCode == HC_ACTION)
    {
        switch (key->vkCode)
        {
        case 'A':
            puts("A");
            break;
        case 'R':
            puts("R");
            break;

        case 'S':
            puts("S");
            break;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    RegisterHotKey(NULL,1,NULL, VK_OEM_3);
    keyboard = GetKeyboardLayout(0);
    _k_hook = SetWindowsHookExA(WH_KEYBOARD_LL, k_Callback1, NULL, 0);
    MSG msg = { 0 };
    std::string bind_s = "Dear [Name], \nSorry for the unpleasant experience you had in our store and I can understand your frustration. I have forwarded your complaint to our management team, and we’ll\ndo our best to make sure this never happens again.\n\nI refunded your purchase, and your funds should be with you shortly.We also want to offer you a 10 % discount for your next purchase in our store.Please use this promo code to get a discount : [link] .\n\nPlease accept our apologies for the inconvenience you had.\n\nBest regards, \n[Your name]\n[Job title]\n[Contact details]";
    std::string bind_a = "Dear [Company Name],\n\nI am writing to express my interest in the [Job Title] position. With a [Degree or Professional Qualification] in [Your Field of Study] and [Number of Years] years of experience in [Your Current or Previous Relevant Job Role], I am confident in my ability to contribute effectively to your team at [Company Name].\n\nIn my previous role at [Previous Company], I [Briefly Describe a Relevant Achievement or Project]. This experience honed my skills in [Specific Skills Relevant to the New Job], which I believe align well with the requirements for the [Job Title] role. For instance, [Provide a Specific Example of How You Used a Skill or Addressed a Challenge Relevant to the New Job].\n\nEnclosed is my resume, which highlights my qualifications. I look forward to the possibility of discussing this exciting opportunity with you. I am available for an interview at your earliest convenience.\n\nSincerely,\n[Your Name]";
    std::string bind_r = "Dear team,\n\nI am pleased to introduce you to [Name] who is starting today as a Customer Support Representative. She will be providing technical support and assistance to our users, making sure they enjoy the best experience with our products.\n\nFeel free to greet [Name] in person and congratulate her with the new role!\n\nBest regards,\n[Your name]\n[Job title]";
    bool waitingforkey = false;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        if (msg.message == WM_HOTKEY && !waitingforkey) {
            int key = 0;
            std::cout << "Waiting for next key...\n";
            waitingforkey = true;
        }
        std::cout << msg.message << "\n";
    }
    if (_k_hook)
        UnhookWindowsHookEx(_k_hook);
    return 0;
}