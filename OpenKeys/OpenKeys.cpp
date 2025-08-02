#include "framework.h"
#include <windows.h>
#include <commctrl.h>    // For the status bar
#include <string>
#include <random>
#include <json.hpp>
#include <fstream>
#include <map>
#include <filesystem>
#include <iostream>
#include <shellapi.h>
#include <wininet.h>
#include "OpenKeys.h"
#include "clipboard.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")

#define MAX_LOADSTRING 100
#define WM_SENDKEYS (WM_USER + 1)
#define WM_TRAYICON (WM_USER + 2)
#define MAX_CHAR_LENGTH 10240
#define METHOD_SENDKEYS 0
#define METHOD_COPYPASTE 1

// When this version is changed, please also change installer/Package.wxs line #6
std::wstring VERSION_STRING   = L"0.3.3";
std::wstring WINDOW_TITLE_STR = L"OpenKeys v" + VERSION_STRING;
uint8_t DEBUG_LEVEL           = 0;

NOTIFYICONDATA nid = {};
HMENU hTrayMenu    = nullptr;

// Keyboard stuff
HHOOK hKeyboardHook;
std::wstring keyBuffer;
std::wstring displayedText = L"Epic Thing!!!!";
HFONT hFont;
// Declare an array of MAX_CHAR_LENGTH inputs
INPUT* inputs = new INPUT[MAX_CHAR_LENGTH]();

// Default JSON URL
std::string json_default_url = "https://raw.githubusercontent.com/feive7/OpenKeys/refs/heads/master/OpenKeys/shortcuts.json"; // Found a better way

// Configurable stuff
bool START_MINIMIZED = false;
bool enableLogging   = true;
int inputMethod      = METHOD_SENDKEYS;
bool easterEgg       = false;
std::wstring prefix;
std::wstring gotochar;
std::wstring version;
std::map<std::wstring, std::wstring> shortcuts;

HWND g_hWnd = nullptr;
HWND hStatus; // Status bar handle
HINSTANCE g_hInstance = NULL;
std::wstring pendingReplacement;

// Scroll bar variables
int scrollY    = 0;
int maxScrollY = 0;

// Global variable for path to JSON file
std::wstring json_path;

// Mutex handle
HANDLE hHandle;

// Flags
bool JSON_FILE_LOADED = false;
bool JSON_URL_LOADED  = false;

void UpdateDisplayedTextFromShortcuts() {
    displayedText = L"";
    if (easterEgg) {
        displayedText += L"Ok, so literally, we've all been held back by the binary computer. Each bit can only be on or off. The only benefit of this is that when sending data wirelessly, there is a 0.0000001% chance of any bit to change. But when it comes to local computations or data storage, ternary is a superior option. I'm no computer scientist, but I'm pretty sure creating nano-tech to read whether a bit is on, off, or in-between seems not super complicated. If we did this, we could store 8192 binary bits in 1518 ternary bits. The only reason we haven't done this, is the standardization of binary processors and storage methods. We as a society only continue to improve the binary computer while completely ignoring the next best computing method.";
    }
    else {
        if (JSON_FILE_LOADED) {
            displayedText += L"Usage: " + prefix + L" + [Shortcut]\n";

            if (gotochar.length() > 0) {
                displayedText += L"Goto Char: " + gotochar + L"\n";
            }

            displayedText += L"\n";
            displayedText += L"Shortcuts:\n";
            for (const auto& pair : shortcuts) {
                displayedText += pair.first + L"\n";
            }

            displayedText += L"\n";

            char buf[100];
            snprintf(buf, sizeof(buf), "Shortcuts: %ls", version.c_str());

            UpdateStatusBar(hStatus, 2, buf);
        }
        else {
            displayedText += L"No JSON file was loaded";
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
bool JsonHasKey(nlohmann::json jsonData, std::string key) {
    if (jsonData.find(key) != jsonData.end()) {
        return true;
	}
	else {
        return false;
	}
}
nlohmann::json LoadJsonFromFile(const std::wstring& filename) {
    JSON_FILE_LOADED = false; // Reset the flag
    std::ifstream file(filename);

    // For some reason the file won't open
    if (!file.is_open()) {
        displayedText = L"Failed to open JSON from file";
        log_line("Failed to open file " + wstringToString(filename));
        JSON_FILE_LOADED = false; // Couldn't find file
        return {};
	}

    JSON_FILE_LOADED = true; // Could find file
    
    nlohmann::json jsonData = nlohmann::json::parse(file, nullptr, false);
    if (jsonData.is_discarded()) {
        ErrorMessage(12, L"JSON is invalid. Please check formatting");
        exit(12);
    }
    file.close();

    std::string version = "";
    version = jsonData.value("version", "");
    log_line("Loaded local JSON file " + wstringToString(filename) + " (version: " + version + ")");

    return jsonData;
}
nlohmann::json LoadJsonFromUrl(const std::string& url) {
    JSON_URL_LOADED = false; // Reset the flag
	std::string jsonContent = DownloadJsonFromURL(url);
	if (jsonContent.empty()) {
		displayedText = L"Failed to download JSON from URL";
        log_line("Failed to open url " + url);
        JSON_URL_LOADED = false; // Couldn't open url
        return {};
	}

    JSON_URL_LOADED = true; // Could open url
	nlohmann::json jsonData = nlohmann::json::parse(jsonContent, nullptr, false);

    // If the JSON is still invalid, throw fatal error
    if (jsonData.is_discarded()) {
		displayedText = L"Failed to parse JSON from URL";
		log_line("Failed to parse JSON from URL: " + url);
		JSON_URL_LOADED = false; // Couldn't parse json
		return {};
	}

    std::string version = "";
    version = jsonData.value("version", "");
    log_line("Loaded URL JSON file " + url + " (version: " + version + ")");

    return jsonData;
}
void LoadDataFromJson(nlohmann::json jsonData) {
    prefix = Utf8ToWstring(jsonData["prefix"]);
    version = Utf8ToWstring(jsonData["version"]);
    if (JsonHasKey(jsonData, "goto_character")) {
        gotochar = Utf8ToWstring(jsonData["goto_character"]);
    }
    if (JsonHasKey(jsonData, "start_minimized")) {
        START_MINIMIZED = jsonData["start_minimized"];
    }
    if (JsonHasKey(jsonData, "enable_logging")) {
        enableLogging = jsonData["enable_logging"];
    }
    if (JsonHasKey(jsonData, "ternary")) {
        easterEgg = true;
    }
    if (JsonHasKey(jsonData, "input_method")) {
        std::wstring inputMethodWish = Utf8ToWstring(jsonData["input_method"]);
        if (inputMethodWish == L"sendkeys") {
            inputMethod = METHOD_SENDKEYS;
        }
        else if (inputMethodWish == L"copypaste") {
            inputMethod = METHOD_COPYPASTE;
        }
    }
    for (auto& el : jsonData["shortcuts"].items()) {
        std::wstring key = Utf8ToWstring(el.key());
        std::wstring value = Utf8ToWstring(el.value().get<std::string>());

        if (value.length() > MAX_CHAR_LENGTH / 2) {
            log_line(std::to_string(value.length()));
        }
        else {

        }
        shortcuts[key] = value;
    }

    // Debug is optional... if it's not in the JSON then it stays at 0
    if (JsonHasKey(jsonData, "debug")) {
        DEBUG_LEVEL = jsonData["debug"];
    }
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
            bool ok = GetKeyboardState(keyboardState);

            // If GetKeyboardState fails, we bail out
            if (!ok) {
                log_line("GetKeyboardState failed???");
                return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
            }

            // This makes sure that when you press shift + 3 for the # character
            // it registers correctly
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                keyboardState[VK_SHIFT] |= 0x80;
            }
            if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
                keyboardState[VK_LSHIFT] |= 0x80;
            }
            if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) {
                keyboardState[VK_RSHIFT] |= 0x80;
            }
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                keyboardState[VK_CONTROL] |= 0x80;
            }
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {    // Alt Key
                keyboardState[VK_MENU] |= 0x80;
            }
            if (GetAsyncKeyState(VK_CAPITAL) & 0x0001) { // Capslock
                keyboardState[VK_CAPITAL] |= 0x01;
            }

            // Map the virtual key code to a scan code
            UINT scanCode = MapVirtualKey(p->vkCode, MAPVK_VK_TO_VSC_EX);

            // Convert the virtual key code to a Unicode character
            WCHAR unicodeChar[4] = {};
            int result           = ToUnicode(p->vkCode, scanCode, keyboardState, unicodeChar, 4, 0);

            if (unicodeChar[0] != 8 && unicodeChar[0] != 0) { // If character is not backspace
                keyBuffer += towlower(unicodeChar[0]); // Otherwise proceed as normal

                // TO-DO: make a switch-case statement for every special key or fix it in a better way

                // If debug is enabled we log the last keypress
                if (DEBUG_LEVEL > 0) {
                    char buffer[100];
                    snprintf(buffer, sizeof(buffer), "Last: %ls", unicodeChar);

                    // Update the RIGHT status bar with the last key pressed
                    UpdateStatusBar(hStatus, 1, buffer);
                }

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

                    if (DEBUG_LEVEL >= 2) {
                        char buffer[100];
                        snprintf(buffer, sizeof(buffer), "Buf: %ls", keyBuffer.c_str());

                        // Update the RIGHT status bar with the last key pressed
                        UpdateStatusBar(hStatus, 1, buffer);
                    }

                    if (buff_matches) {
                        keyBuffer.clear();

                        if (enableLogging) {
                            // Log the shortcut trigger
                            char buffer[100];
                            snprintf(buffer, sizeof(buffer), "Shortcut '%ls' triggered", pair.first.c_str());
                            log_line(buffer);

                            // Update the status bar with the shortcut trigger
                            get_datetime_string();
                            snprintf(buffer, sizeof(buffer), "%ls: Shortcut '%ls' triggered", GetDateTimeHuman().c_str(), pair.first.c_str());

                            UpdateStatusBar(hStatus, 0, buffer);
                        }

                        pendingReplacement = pair.second;

                        // Post a custom message to your main window after 1 tick
                        PostMessage(g_hWnd, WM_SENDKEYS, (WPARAM)trigger.size(), 0);
                        break;
                    }
                }
            }
            else if(unicodeChar[0] == 8) { // If it IS backspace, don't add to buffer, and remove last entry
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
void MinimizeToTray(HWND hWnd) {
    ShowWindow(hWnd, SW_HIDE);
    AddTrayIcon(hWnd);
}
void RestoreFromTray(HWND hWnd) {
    ShowWindow(hWnd, SW_SHOW);
    ShowWindow(hWnd, SW_RESTORE);
    Shell_NotifyIcon(NIM_DELETE, &nid);
}
bool CloseConfirmation() {
    int response = MessageBox(g_hWnd, L"Are you sure you want to close OpenKeys?", L"Confirm Close", MB_ICONQUESTION | MB_YESNO);
    if (response == IDYES) {
        return true;
    }
    return false;
}
void CloseWindowAndExit() {
    log_line("Closing OpenKeys...\n");
    ReleaseMutex(hHandle);
    CloseHandle(hHandle);
    DeleteObject(hFont);
    Shell_NotifyIcon(NIM_DELETE, &nid);
    UnhookWindowsHookEx(hKeyboardHook);

    // Free the memory from the inputs
    delete[] inputs;
    PostQuitMessage(0);
}

int8_t LoadShortcuts() {
    shortcuts.clear();
    nlohmann::json jsonFILE = LoadJsonFromFile(json_path);
    uint8_t ret             = 0; // Default return value

	// If there is no local JSON file, we download the default one from github
    if (jsonFILE.empty()) {
        log_line("Downloading default JSON from github: " + json_default_url);
        std::string json_default_data = DownloadJsonFromURL(json_default_url);

        if (json_default_data.empty()) {
			// If the download fails, we create a default JSON from local data
			log_line("Failed to download default JSON from github. Creating JSON from local data...");
            json_default_data = R"({"prefix": "`", "version": ")" + wstringToString(VERSION_STRING) + R"(", "shortcuts": {"example": "This is an example shortcut"}})";
		}

        std::ofstream file;
        file.open(json_path);
        file << json_default_data;
        file.close();

        if (!file.good() || !std::filesystem::exists(json_path)) {
            log_line("Failed to create shortcuts.json file.");
            ErrorMessage(12, L"Failed to create shortcuts.json file. Please check permissions.");
            return -8;
        }

        jsonFILE = LoadJsonFromFile(json_path);
    }

    // If the JSON has an external URL, we check the versions and ask to overwrite if they don't match
    if (JsonHasKey(jsonFILE, "external_url")) {

        nlohmann::json jsonURL = LoadJsonFromUrl(jsonFILE["external_url"]);
        if (jsonURL.empty()) {
            log_line("Failed to load JSON from URL: " + jsonFILE["external_url"].get<std::string>());
            WarningMessage(L"Warning", L"The URL you provided does not contain valid JSON. URL ignored.");
            return -9;
        }

        // The local version and remote versions are different
        if (jsonURL["version"] != jsonFILE["version"]) {
            int overwrite = MessageBox(NULL, L"New version of shortcuts found. Would you like to use the new version?", L"New shortcuts found", MB_ICONINFORMATION | MB_YESNO);

            // If the user chooses to overwrite, we overwrite the local JSON with the one from the URL
            if (overwrite == IDYES) {
                // If the remote JSON does not have an external URL, we preserve the local one
                if (!JsonHasKey(jsonURL, "external_url")) {
                    jsonURL["external_url"] = jsonFILE["external_url"]; // Preserve the external URL in the new JSON
                }

                // Write the new JSON to the local file
                std::ofstream file(json_path);
                if (file.is_open()) {
                    file << jsonURL.dump(4); // Pretty print with 4 spaces
                } else {
                    log_line("Failed to open shortcuts.json for writing.");
                }
                file.close();

                log_line("Overwrote local JSON with the one from the URL");

                jsonFILE = LoadJsonFromFile(json_path); // Reload the JSON from the file

                ret = 1; // We got a new JSON file from the URL
            }
            // If the user chooses not to overwrite, we use the local JSON
            else {
                log_line("Did not overwrite local JSON, using local data");

				ret = -1; // No need to overwrite, user chose not to
            }
        }
        else if (jsonURL["version"] == jsonFILE["version"]) {
            log_line("Local and remote versions are the same");

			ret = -2; // No need to overwrite, versions match
        }

    }
    else {
		ret = -3; // No external URL, so we just use the local data
    }

    LoadDataFromJson(jsonFILE);
    UpdateDisplayedTextFromShortcuts();

    return ret;
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

    // Initialize global hInstance
    g_hInstance = hInstance;

    // Code to prevent multiple instances from opening
    LPCWSTR szUniqueNamedMutex = L"screw_shortkeys_I_aint_paying_40_bucks";
    hHandle = CreateMutex(NULL, TRUE, szUniqueNamedMutex);
    if (ERROR_ALREADY_EXISTS == GetLastError()) {
        InfoMessage(L"Instance already running", L"Openkeys is already running. Check your system tray.");
        return(1);
    }

    // Open the log file
    if (enableLogging) {
        // Open the log file in the AppData directory
        std::wstring log_path = GetAppDataDir() + L"\\OpenKeys\\openkeys.log";
        std::ifstream f(wstringToString(log_path.c_str()));
        LOG.open(log_path, std::ios::app); // Open the log file for APPENDING
        if (!LOG) { // If doesn't exist, create and try again
            if (!std::filesystem::create_directory(GetAppDataDir() + L"\\OpenKeys")) {
                ErrorMessage(L"Fatal Error", L"Error 10: Could not create OpenKeys directory in Appdata");
                std::cerr << "Failed to create Appdata directory";
                exit(10);
            }
            else {
                log_line("Appdata directory created");
            }
        }

        if (!f.good()) {
            log_line("Logfile created (You can disable logging by setting enable_logging to false in your json)");
        }

        char buffer[100];
        snprintf(buffer, sizeof(buffer), "OpenKeys v%ls started", VERSION_STRING.c_str());
        log_line(buffer);

    }

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_OPENKEYS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // JSON file is found in %APPDATA%\OpenKeys\shortcuts.json
    json_path = GetAppDataDir() + L"\\OpenKeys\\shortcuts.json";

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_OPENKEYS));

    MSG msg;

    // Include the version in the title of the window
    std::wstring window_title = L"OpenKeys v" + VERSION_STRING;
    SetWindowText(g_hWnd, window_title.c_str());

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES; // Needed for status bar
    InitCommonControlsEx(&icex);

    //SendMessage(hStatus, SB_SETTEXT, part_index, (LPARAM)L"New text");

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
       ErrorMessage(62, L"Keyboard hook failed!");
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
   LoadShortcuts();

   if (!START_MINIMIZED) {
       ShowWindow(hWnd, nCmdShow);
   }

   UpdateWindow(hWnd);

   if (START_MINIMIZED) {
       MinimizeToTray(hWnd);
   }

   return TRUE;
}

void SendKeys(WPARAM wParam) {
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
            ErrorMessage(L"Fatal Error", L"Error 26: Maximum Key Buffer exceeded");
            log_line(std::to_string(inputIndex) + " err #26");
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

// Paste the pending replacement text over the trigger word
void PasteKeys() {
    std::string clipboardBuffer = getClipboardContents(); // Preserve original clipboard contents
    copyToClipboard(wstringToString(pendingReplacement)); // Copy the pending replacement

    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0); // Select the trigger word (Ctrl+Shift+Left)
    keybd_event(VK_SHIFT, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0);
    keybd_event(VK_LEFT, 0x2F, KEYEVENTF_EXTENDEDKEY | 0, 0);
    keybd_event(VK_LEFT, 0x2F, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0); // Paste replacement over the trigger
    keybd_event('V', 0x2F, KEYEVENTF_EXTENDEDKEY | 0, 0);
    keybd_event('V', 0x2F, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    Sleep(100);
    copyToClipboard(clipboardBuffer); // Copy original clipboard contents
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
        int wmId                = LOWORD(wParam);
        std::wstring appDataDir = GetAppDataDir() + L"\\OpenKeys\\";
        std::wstring tmp_str    = L"";

        // Parse the menu selections:
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_REFRESH_BUTTON:
            {
                log_line("Reloaded JSON file");
                int8_t status = LoadShortcuts();

                if (status == 1) {
                    InfoMessage(L"Loaded new shortcuts from server");
                }
                else if (status == -2) {
                    InfoMessage(L"Remote server does not have new shortcuts file");
                }

                UpdateDisplayedTextFromShortcuts();
                break;
          }
        case IDM_FILE_OPENDATA:
            // Open the OpenKeys %APPDATA% directory in Windows Explorer
            if (!DirectoryExists(appDataDir)) {
                ErrorMessage(65413, L"OpenKeys directory not found in %APPDATA%");
                break;
            }

            OpenFolderInExplorer(appDataDir.c_str());
            tmp_str = L"Opened %APPDATA% folder: " + appDataDir;
            log_line(tmp_str);
            break;
        case IDM_EXIT:
            if (CloseConfirmation()) {
                // If user confirms, close the window
                CloseWindowAndExit();
            }
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

        // Set the text color to very dark gray
        SetTextColor(hdc, RGB(20, 20, 20));

        // Fill the background with white
        COLORREF bgColor = RGB(239, 239, 227);
        SetBkColor(hdc, bgColor);
        GetClientRect(hWnd, &rect);
        FillRect(hdc, &rect, CreateSolidBrush(bgColor));

        // Select the custom font into the device context
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // Save the current graphics state to restore it later
        int savedDC = SaveDC(hdc);

        // Set the background mode to transparent (so background won't overwrite the text)
        SetBkMode(hdc, TRANSPARENT);

        // Calculate the full text height to determine drawing area
        RECT textRect = { 0, 0, rect.right - rect.left, 0 };
        DrawTextW(hdc, displayedText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);

        // Set the viewport origin to handle scrolling
        SetViewportOrgEx(hdc, 0, -scrollY, NULL);

        // Draw the text in the full content area (use calculated height)
        int padding   = 4;
        RECT drawRect = { padding, padding, rect.right - rect.left, textRect.bottom + padding };
        DrawTextW(hdc, displayedText.c_str(), -1, &drawRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

        // Restore the graphics state to prevent affecting other parts of the window
        RestoreDC(hdc, savedDC);

        // Restore the old font
        SelectObject(hdc, oldFont);

        EndPaint(hWnd, &ps);

    }
                 break;
    case WM_CLOSE: {
        // Ask user if they want to close the window
        if (CloseConfirmation()) {
			// If user confirms, close the window
            CloseWindowAndExit();
		}
    }
                 break;
    case WM_DESTROY: {
        CloseWindowAndExit();
    }
                   break;
    case WM_SENDKEYS: {
        if (inputMethod == METHOD_SENDKEYS) {
            SendKeys(wParam);
        }
        else if (inputMethod == METHOD_COPYPASTE) {
            PasteKeys();
        }
    }
                    break;
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            MinimizeToTray(hWnd);
        }
        else {
            // Resize the status bar
            SendMessage(hStatus, WM_SIZE, 0, 0);

            // This keeps the dynamic sizing of the status bar
            {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int width = rcClient.right;

                const int fixedWidth = 150;

                int parts[3];
                parts[0] = max(0, width - 2 * fixedWidth);
                parts[1] = parts[0] + fixedWidth;
                parts[2] = -1;

                SendMessage(hStatus, SB_SETPARTS, 3, (LPARAM)parts);
            }

            // Update scroll bar when window is resized
            RECT rect;
            GetClientRect(hWnd, &rect);

            // Calculate text height with proper width constraint
            HDC hdc = GetDC(hWnd);
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            RECT textRect = { 0, 0, rect.right - rect.left, 0 };
            DrawTextW(hdc, displayedText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
            SelectObject(hdc, oldFont);
            ReleaseDC(hWnd, hdc);

            int textHeight = textRect.bottom - textRect.top;
            int clientHeight = rect.bottom - rect.top;

            // Show/hide scroll bar based on content size
            if (textHeight > clientHeight) {
                ShowScrollBar(hWnd, SB_VERT, TRUE);
                maxScrollY = textHeight - clientHeight;
                SetScrollRange(hWnd, SB_VERT, 0, maxScrollY, TRUE);
                SetScrollPos(hWnd, SB_VERT, scrollY, TRUE);
            }
            else {
                ShowScrollBar(hWnd, SB_VERT, FALSE);
                scrollY = 0;
                maxScrollY = 0;
            }
        }
    }
                break;
    case WM_VSCROLL: {
        int scrollCode = LOWORD(wParam);
        int pos = HIWORD(wParam);

        switch (scrollCode) {
        case SB_LINEUP:
            scrollY = max(0, scrollY - 20);
            break;
        case SB_LINEDOWN:
            scrollY = min(maxScrollY, scrollY + 20);
            break;
        case SB_PAGEUP:
            scrollY = max(0, scrollY - 100);
            break;
        case SB_PAGEDOWN:
            scrollY = min(maxScrollY, scrollY + 100);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            scrollY = pos;
            break;
        case SB_TOP:
            scrollY = 0;
            break;
        case SB_BOTTOM:
            scrollY = maxScrollY;
            break;
        }

        SetScrollPos(hWnd, SB_VERT, scrollY, TRUE);
        InvalidateRect(hWnd, NULL, TRUE);
    }
                   break;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollAmount = delta / WHEEL_DELTA * 30; // 30 pixels per wheel notch

        scrollY = max(0, min(maxScrollY, scrollY - scrollAmount));
        SetScrollPos(hWnd, SB_VERT, scrollY, TRUE);
        InvalidateRect(hWnd, NULL, TRUE);
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
    case WM_CREATE:
        {
            hStatus = CreateWindowEx(
                0,                       // no extended styles
                STATUSCLASSNAME,         // status bar class
                NULL,                    // no text initially
                WS_CHILD | WS_VISIBLE,   // styles
                0, 0, 0, 0,              // x, y, cx, cy
                hWnd,                    // parent window
                (HMENU)1,                // ID for status bar
                g_hInstance,             // application instance
                NULL);                   // no additional data

            // Optionally, set number of parts
            RECT rect;
            GetClientRect(hWnd, &rect);
            int width = rect.right;

            // Fixed sizes for parts 2 and 3
            const int fixedWidth = 150;

            // Calculate the widths (positions) for SB_SETPARTS
            int parts[3];
            parts[0] = width - 2 * fixedWidth; // flexible part
            parts[1] = parts[0] + fixedWidth;  // 150 pixels
            parts[2] = -1;                     // 150 pixels (extends to the right edge)

            SendMessage(hStatus, SB_SETPARTS, 3, (LPARAM)parts);

            // Initalize the status bar with empty text
            UpdateStatusBar(hStatus, 0, L"");
            UpdateStatusBar(hStatus, 1, L"");
            UpdateStatusBar(hStatus, 2, L"");

            break;
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG: {
        SetDlgItemText(hDlg, IDC_VERSION, (L"Version " + VERSION_STRING).c_str());

        // Load and display the bitmap image
        HBITMAP hBitmap = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(IDB_BITMAP1),
            IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
        if (hBitmap) {
            SendDlgItemMessage(hDlg, IDC_STATIC, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
        }

        return (INT_PTR)TRUE;
    }

    case WM_CTLCOLORDLG: {
        HDC hdcStatic = (HDC)wParam;
        SetBkColor(hdcStatic, RGB(255, 255, 255)); // White background
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;

        // Get the control ID to identify which static control this is
        int ctrlId = GetDlgCtrlID(hwndStatic);

        // Make copyright and version labels transparent
        if (ctrlId == IDC_STATIC || ctrlId == IDC_VERSION) {
            SetBkMode(hdcStatic, TRANSPARENT);
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // Black text for visibility
            return (INT_PTR)GetStockObject(NULL_BRUSH); // Transparent background
        }

        return (INT_PTR)DefWindowProc(hDlg, WM_CTLCOLORSTATIC, wParam, lParam);
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
