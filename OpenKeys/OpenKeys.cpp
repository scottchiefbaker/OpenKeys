#include "framework.h"
#include "OpenKeys.h"
#include <string>
#include <random>
#include <json.hpp>
#include <fstream>
#include <map>
#include <filesystem>
#include <iostream>

#define MAX_LOADSTRING 100
#define IDT_REFRESH_TIMER 1

HHOOK hKeyboardHook;
std::wstring keyBuffer;
std::wstring displayedText = L"Epic Thing!!!!";
HFONT hFont;
std::map<std::wstring, std::wstring> shortcuts;
HWND g_hWnd = nullptr;
std::wstring pendingReplacement;
std::wstring prefix;

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
    return result;
}
void UpdateDisplayedTextFromShortcuts() {
    displayedText = L"Shortcuts:\n";
    for (const auto& pair : shortcuts) {
        displayedText += pair.first + L" → " + pair.second + L"\n";
    }
}
void LoadShortcutsFromJSON(const std::wstring& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        displayedText = L"Failed to open " + filename;
        return;
    }

    try {
        nlohmann::json jsonData;
        file >> jsonData;
        prefix = Utf8ToWstring(jsonData["prefix"]);
        shortcuts.clear(); // Make sure you're clearing old shortcuts
        for (auto& el : jsonData["shortcuts"].items()) {
            std::wstring key = Utf8ToWstring(el.key());
            std::wstring value = Utf8ToWstring(el.value().get<std::string>());
            shortcuts[key] = value;
        }

        UpdateDisplayedTextFromShortcuts();
    }
    catch (const std::exception& ex) {
        std::string err = "JSON Parse Error: ";
        err += ex.what();
        displayedText = std::wstring(err.begin(), err.end());
    }

    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, TRUE);
        UpdateWindow(g_hWnd);
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            BYTE keyboardState[256];
            GetKeyboardState(keyboardState);

            WCHAR unicodeChar[4] = {};
            UINT scanCode = MapVirtualKey(p->vkCode, MAPVK_VK_TO_VSC);
            ToUnicode(p->vkCode, scanCode, keyboardState, unicodeChar, 4, 0);

            if (unicodeChar[0]) {
                keyBuffer += towlower(unicodeChar[0]);
                if (keyBuffer.size() > 20) keyBuffer.erase(0, 1);

                for (const auto& pair : shortcuts) {
                    std::wstring trigger = prefix + pair.first;
                    if (keyBuffer.size() >= trigger.size() &&
                        keyBuffer.compare(keyBuffer.size() - trigger.size(), trigger.size(), trigger) == 0) {

                        keyBuffer.clear();
                        pendingReplacement = pair.second;

                        // Post a custom message to your main window after 1 tick
                        PostMessage(g_hWnd, WM_USER + 1, (WPARAM)trigger.size(), 0);
                        break;
                    }
                }
            }
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_OPENKEYS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_OPENKEYS));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_OPENKEYS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_OPENKEYS);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
   hInst = hInstance; // Store instance handle in our global variable
   // Create Window
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   g_hWnd = hWnd;

   if (!hWnd) {
      return FALSE;
   }
   // Initialize Keyboard Hook
   hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
   if (!hKeyboardHook) {
       MessageBox(NULL, L"Keyboard hook failed!", L"Error", MB_ICONERROR);
       return FALSE;
   }

   // Create Font for Window (Ridiculous)
   LOGFONT lf = { 0 };
   lf.lfHeight = 24;  // Font height
   lf.lfWeight = FW_NORMAL;  // Regular weight
   lf.lfItalic = FALSE;  // No italics
   lf.lfUnderline = FALSE;  // No underline
   lf.lfStrikeOut = FALSE;  // No strikeout
   // Set the font name (e.g., Arial)
   wcscpy_s(lf.lfFaceName, L"Arial");
   hFont = CreateFontIndirect(&lf);

   //Load Shortcuts
   std::filesystem::path fullpath = __FILE__;
   std::wstring dirname = fullpath.parent_path();
   LoadShortcutsFromJSON(dirname + L"/shortcuts.json");
   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   SetTimer(g_hWnd, IDT_REFRESH_TIMER, 1000, NULL);  // 1000 ms = 1 second

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect; // Window Rect
        GetClientRect(hWnd, &rect);

        COLORREF bgColor = RGB(173, 216, 230);
        SetBkColor(hdc, bgColor);
        GetClientRect(hWnd, &rect);
        FillRect(hdc, &rect, CreateSolidBrush(bgColor));

        // Select the custom font into the device context
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // Save the current graphics state to restore it later
        int savedDC = SaveDC(hdc);

        // Set the text color (for example, red)
        SetTextColor(hdc, RGB(0, 98, 191)); // RGB for red color

        // Set the background mode to transparent (so background won't overwrite the text)
        SetBkMode(hdc, TRANSPARENT);

        // Draw the text
        DrawTextW(hdc, displayedText.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);

        // Restore the graphics state to prevent affecting other parts of the window
        RestoreDC(hdc, savedDC);
        
        // Restore the old font
        SelectObject(hdc, oldFont);

        EndPaint(hWnd, &ps);

        }
        break;
    case WM_DESTROY:
        DeleteObject(hFont);
        KillTimer(hWnd, IDT_REFRESH_TIMER);
        UnhookWindowsHookEx(hKeyboardHook);
        PostQuitMessage(0);
        break;
    case WM_USER + 1: {
        size_t triggerLen = (size_t)wParam;

        INPUT inputs[2048] = {};
        int inputIndex = 0;

        // Step 1: Backspace trigger
        for (size_t i = 0; i < triggerLen; ++i) {
            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = VK_BACK;
            inputIndex++;

            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = VK_BACK;
            inputs[inputIndex].ki.dwFlags = KEYEVENTF_KEYUP;
            inputIndex++;
        }

        // Step 2: Type the replacement
        for (wchar_t ch : pendingReplacement) {
            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = 0;
            inputs[inputIndex].ki.wScan = ch;
            inputs[inputIndex].ki.dwFlags = KEYEVENTF_UNICODE;
            inputIndex++;

            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = 0;
            inputs[inputIndex].ki.wScan = ch;
            inputs[inputIndex].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputIndex++;
        }

        SendInput(inputIndex, inputs, sizeof(INPUT));
        break;
    }
    case WM_TIMER:
        if (wParam == IDT_REFRESH_TIMER) {
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    default: 
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
