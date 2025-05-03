#include "framework.h"
#include "OpenKeys.h"
#include <string>
#include <random>
#include <json.hpp>
#include <fstream>
#include <map>
#include <filesystem>
#include <iostream>
#include <shellapi.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

#define MAX_LOADSTRING 100
#define WM_SENDKEYS (WM_USER + 1)
#define WM_TRAYICON (WM_USER + 2)
#define MAX_CHAR_LENGTH 10240

std::wstring VERSION_STRING = L"0.2.1";

NOTIFYICONDATA nid = {};
HMENU hTrayMenu = nullptr;

std::ofstream LOG; // Global log file stream

// Keyboard stuff
HHOOK hKeyboardHook;
std::wstring keyBuffer;
std::wstring displayedText = L"Epic Thing!!!!";
HFONT hFont;
// Declare an array of MAX_CHAR_LENGTH inputs
INPUT* inputs = new INPUT[MAX_CHAR_LENGTH]();

// Default JSON contents
std::string json_default_data = "{\n    \"version\": \"25.20.4\",\n    \"prefix\": \"`\",\n    \"goto_character\": \"^\",\n    \"start_minimized\": false,\n    \"enable_logging\": true,\n    \"shortcuts\": {\n        \"openkeys\": \"Welcome to OpenKeys, a free and open-source text replacement program! (type `about)\",\n        \"about\": \"OpenKeys is meant to be a free alternative to other pricey text replacement programs such as ShortKeys, and TextExpander. (type `features)\",\n        \"features\": \"This program is, for the most part, fully configurable within this json. Currently, you can change the prefix character, and set whether the program starts minimized. But my personal favorite, is what I call the Goto Character. (type `gotochar)\",\n        \"gotochar\": \"With the Goto Character feature, you can tell the program to put your text cursor in a specific spot after pasting. Instead of at the end, this shortcut will set the cursor ^ <- Here. Of course, the Goto Character is configurable. (type `newline)\",\n        \"newline\": \"This\\nProgram\\nSupports\\nNewlines!\\n(I don't know if that's impressive or not)\\n(type `github)\",\n		\"github\": \"Because this program is open-source, all of the source code is available on github (`link), feel free to make a bug report!\",\n		\"link\": \"https://github.com/feive7/OpenKeys\"\n	}\n}"; // Maybe find better way to write this

// Configurable stuff
bool START_MINIMIZED = false;
bool enableLogging = true;
bool easterEgg = false;
std::wstring prefix;
std::wstring gotochar;
std::wstring version;
std::map<std::wstring, std::wstring> shortcuts;

HWND g_hWnd = nullptr;
std::wstring pendingReplacement;

// Global variable for path to JSON file
std::wstring json_path;

// Mutex handle
HANDLE hHandle;

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

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
    return result;
}
void UpdateDisplayedTextFromShortcuts() {
    if (easterEgg) {
        displayedText = L"Ok, so literally, we've all been held back by the binary computer. Each bit can only be on or off. The only benefit of this is that when sending data wirelessly, there is a 0.0000001% chance of any bit to change. But when it comes to local computations or data storage, ternary is a superior option. I'm no computer scientist, but I'm pretty sure creating nano-tech to read whether a bit is on, off, or in-between seems not super complicated. If we did this, we could store 8192 binary bits in 1518 ternary bits. The only reason we haven't done this, is the standardization of binary processors and storage methods. We as a society only continue to improve the binary computer while completely ignoring the next best computing method.";
    }
    else {
        displayedText = L"Shortcuts Version: " + version + L"\n";
        displayedText += L"JSON File: " + json_path + L"\n";
        displayedText += L"Prefix Key: " + prefix + L"\n";

        if (gotochar.length() > 0) {
            displayedText += L"Goto Char: " + gotochar + L"\n";
        }

        displayedText += L"\n";
        displayedText += L"Shortcuts:\n";
        for (const auto& pair : shortcuts) {
            displayedText += pair.first + L"\n";
        }
    }
}
std::string DownloadJsonFromURL(const std::string& url) {
    HINTERNET hInternet = InternetOpen(L"OpenKeys", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet)
        return "";

    HINTERNET hFile = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hFile) {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string data;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        data.append(buffer, bytesRead);
    }

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return data;
}
void LoadJson(const std::string& jsonContent)  {
    nlohmann::json keyData = nlohmann::json::parse(jsonContent, nullptr, false);
    if (keyData.is_discarded())
        return;
    for (auto& el : keyData["shortcuts"].items()) {
        std::wstring key = Utf8ToWstring(el.key());
        std::wstring value = Utf8ToWstring(el.value().get<std::string>());
        shortcuts[key] = value;
    }
}

void LoadJsonKeysFromURL(const std::string& url) {
    std::string jsonContent = DownloadJsonFromURL(url);
    if (!jsonContent.empty()) {
        LoadJson(jsonContent);
    }
    UpdateDisplayedTextFromShortcuts();
}

bool LoadDataFromJson(const std::wstring& filename) {
    shortcuts = {};
    std::ifstream file(filename);
    if (!file.is_open()) {
        displayedText = L"Failed to open " + filename;
        return false; // Couldn't find file
    }

    // Count how many entries we load from the JSON
    unsigned int count = 0;

    try {
        nlohmann::json jsonData;
        file >> jsonData;
        prefix = Utf8ToWstring(jsonData["prefix"]);
        version = Utf8ToWstring(jsonData["version"]);
        shortcuts.clear(); // Make sure you're clearing old shortcuts

        // If the JSON contains goto_character we pull it out
        if (jsonData.find("goto_character") != jsonData.end()) {
            gotochar = Utf8ToWstring(jsonData["goto_character"]);
        }

        if (jsonData.find("start_minimized") != jsonData.end()) {
            START_MINIMIZED = jsonData["start_minimized"];
        }
        for (auto& el : jsonData["shortcuts"].items()) {
            std::wstring key   = Utf8ToWstring(el.key());
            std::wstring value = Utf8ToWstring(el.value().get<std::string>());

            if (value.length() > MAX_CHAR_LENGTH / 2) {
                log_line(std::to_string(value.length()));
            }
            else {
                
            }
            shortcuts[key] = value;
            count++;
        }
        if (jsonData.find("enable_logging") != jsonData.end()) {
            enableLogging = jsonData["enable_logging"];
        }
        if (jsonData.find("ternary") != jsonData.end()) {
            easterEgg = true;
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

    // Log how many shortcuts we found
    if (enableLogging) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Loaded %u shortcuts from configuration", count);
        log_line(buffer);
    }

    return true; // Sucessfully found file, may or may not have been loaded. Maybe make this function return errors instead of just a bool
}
std::string wstringToString(const std::wstring& wstr) {
    // Create a string with enough space to hold the converted characters
    std::string str(wstr.begin(), wstr.end());
    return str;
}
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

// This is where the keypress is captured and compared against the list of shortcuts
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (p->flags & LLKHF_INJECTED) {
            return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
        }
        // Keydown event
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            BYTE keyboardState[256];
            GetKeyboardState(keyboardState);

            WCHAR unicodeChar[4] = {};
            UINT scanCode = MapVirtualKey(p->vkCode, MAPVK_VK_TO_VSC);
            ToUnicode(p->vkCode, scanCode, keyboardState, unicodeChar, 4, 0);

            if (unicodeChar[0] != 8) { // If character is not backspace
                // Append this new key/char to the buffer
                keyBuffer += towlower(unicodeChar[0]);
                // If we're larger than 20 characters we erase the first char to keep the buffer manageable
                if (keyBuffer.size() > 20) keyBuffer.erase(0, 1);

                // Loop through each known shortcut and see if we match
                for (const auto& pair : shortcuts) {
                    std::wstring trigger = prefix + pair.first;

                    // Make sure we have enough buffer to contain the trigger string
                    int has_enough_buff = keyBuffer.size() >= trigger.size();
                    if (!has_enough_buff) {
                        continue;
                    }

                    // Compare the end of the buffer with the trigger string to see if we match this shortcut
                    int buff_matches = keyBuffer.compare(keyBuffer.size() - trigger.size(), trigger.size(), trigger) == 0;
                    if (buff_matches) {
                        keyBuffer.clear();

                        if (enableLogging) {
                            char buffer[100];
                            snprintf(buffer, sizeof(buffer), "Heard '%ls'", pair.first.c_str());
                            log_line(buffer);
                        }

                        pendingReplacement = pair.second;

                        // Post a custom message to your main window after 1 tick
                        PostMessage(g_hWnd, WM_SENDKEYS, (WPARAM)trigger.size(), 0);
                        break;
                    }
                }
            }
            else { // If it IS backspace, don't add to buffer, and remove last entry
                if(keyBuffer.size() > 0) {
                    keyBuffer.pop_back();
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

void AddTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_OPENKEYS)); // your app icon
    wcscpy_s(nid.szTip, L"OpenKeys");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

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

    // Code to prevent multiple instances from opening
    LPCWSTR szUniqueNamedMutex = L"screw_shortkeys_I_aint_paying_40_bucks";
    hHandle = CreateMutex(NULL, TRUE, szUniqueNamedMutex);
    if (ERROR_ALREADY_EXISTS == GetLastError()) {
        return(1);
    }

    // Open the log file
    if (enableLogging) {
        std::wstring log_path = GetExecutableDirectory() + L"/openkeys.log";
        std::ifstream f(wstringToString(log_path.c_str()));
        LOG.open(log_path, std::ios::app);

        if (!LOG) {
            std::cerr << "Failed to open the log file for appending.\n";
            exit(9);
        }

        if (!f.good()) {
            log_line("OpenKeys started (You can disable logging by setting enable_logging to false in your json)");
        }
        log_line("OpenKeys started");

    }

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_OPENKEYS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // JSON file is found in the directory of the .exe
    json_path = GetExecutableDirectory() + L"/shortcuts.json";

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_OPENKEYS));

    MSG msg;

    // Include the version in the title of the window
    std::wstring window_title = L"OpenKeys v" + VERSION_STRING;
    SetWindowText(g_hWnd, window_title.c_str());

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

void MinimizeToTray(HWND hWnd) {
    ShowWindow(hWnd, SW_HIDE);
    AddTrayIcon(hWnd);
}
void RestoreFromTray(HWND hWnd) {
    ShowWindow(hWnd, SW_SHOW);
    ShowWindow(hWnd, SW_RESTORE);
    Shell_NotifyIcon(NIM_DELETE, &nid);
}
void CloseWindowAndExit() {
    ReleaseMutex(hHandle);
    CloseHandle(hHandle);
    DeleteObject(hFont);
    Shell_NotifyIcon(NIM_DELETE, &nid);
    UnhookWindowsHookEx(hKeyboardHook);

    // Free the memory from the inputs
    delete[] inputs;
    PostQuitMessage(0);
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
   HWND hWnd = CreateWindowW(
       szWindowClass,
       szTitle,
       WS_OVERLAPPEDWINDOW,
       20,  // X start
       20,  // Y start
       800, // Window width
       600, // Window height
       nullptr,
       nullptr,
       hInstance,
       nullptr
   );

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
   //LoadJsonKeysFromURL("https://www.perturb.org/tmp/shortcuts.json");
   bool good = LoadDataFromJson(json_path);
   if (!good) {
       std::ofstream outfile(json_path);
       outfile << json_default_data << std::endl;
       outfile.close();
       LoadDataFromJson(json_path);
   }

   if (!START_MINIMIZED) {
       ShowWindow(hWnd, nCmdShow);
   }

   UpdateWindow(hWnd);

   if (START_MINIMIZED) {
       MinimizeToTray(hWnd);
   }

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
            case IDM_REFRESH_BUTTON:
                log_line("Reloaded JSON file");
                LoadDataFromJson(json_path);
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
    case WM_DESTROY: {
        CloseWindowAndExit();
    }
        break;
    case WM_SENDKEYS: {
        size_t triggerLen = (size_t)wParam;

        int inputIndex = 0;

        // Step 1: Erase the trigger text
        for (size_t i = 0; i < triggerLen; ++i) {
            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = VK_BACK;
            inputIndex++;

            inputs[inputIndex].type = INPUT_KEYBOARD;
            inputs[inputIndex].ki.wVk = VK_BACK;
            inputs[inputIndex].ki.dwFlags = KEYEVENTF_KEYUP;
            inputIndex++;
        }

        // Step 2: Type the replacement one character at a time
        for (wchar_t ch : pendingReplacement) {
            if (ch == L'\n') {
                // Press Enter key
                inputs[inputIndex].type = INPUT_KEYBOARD;
                inputs[inputIndex].ki.wVk = VK_RETURN;
                inputs[inputIndex].ki.dwFlags = 0;
                inputIndex++;

                inputs[inputIndex].type = INPUT_KEYBOARD;
                inputs[inputIndex].ki.wVk = VK_RETURN;
                inputs[inputIndex].ki.dwFlags = KEYEVENTF_KEYUP;
                inputIndex++;
            }
            else {
                // Unicode input
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

            if (inputIndex > MAX_CHAR_LENGTH) {
                log_line(std::to_string(inputIndex) + " err #26694");
                exit(26);
            }
        }
        
        // Step 3: if shortcut contains gotochar move text cursor to it
        if (gotochar.length() > 0) {
            if (pendingReplacement.find(gotochar) != std::wstring::npos) {

                size_t pos = pendingReplacement.length() - pendingReplacement.find(gotochar) - 1;

                for (int i = 0; i < pos; i++) {
                    inputs[inputIndex].type = INPUT_KEYBOARD;
                    inputs[inputIndex].ki.wVk = VK_LEFT;
                    inputs[inputIndex].ki.dwFlags = KEYEVENTF_UNICODE;
                    inputIndex++;
                    inputs[inputIndex].type = INPUT_KEYBOARD;
                    inputs[inputIndex].ki.wVk = VK_LEFT;
                    inputs[inputIndex].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                    inputIndex++;
                }
                // Remove the goto character
                inputs[inputIndex].type = INPUT_KEYBOARD;
                inputs[inputIndex].ki.wVk = VK_BACK;
                inputIndex++;

                inputs[inputIndex].type = INPUT_KEYBOARD;
                inputs[inputIndex].ki.wVk = VK_BACK;
                inputs[inputIndex].ki.dwFlags = KEYEVENTF_KEYUP;
                inputIndex++;
            }
        }
        SendInput(inputIndex, inputs, sizeof(INPUT));
    }
        break;
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            MinimizeToTray(hWnd);
        }
    }
        break;
    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP) {
            RestoreFromTray(hWnd);
        }
        else if (lParam == WM_RBUTTONUP) {
            CloseWindowAndExit();
        }
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
        SetDlgItemText(hDlg, IDC_VERSION, (L"Version " + VERSION_STRING).c_str());
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