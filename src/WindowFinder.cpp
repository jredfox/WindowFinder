#include <windows.h>
#include <psapi.h>
#include <process.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <atomic>

using namespace std;
std::string text = "";
std::string title = "";
std::mutex textMutex;
std::atomic<bool> exitFlag(false);
HWND cachedHWND = NULL;
std::string old_title = "";

/**
* kills the process without being handled by System#exit or SIGNALING. this may cause file corruption
*/
void killProcess(unsigned long pid)
{
	const auto explorer = OpenProcess(PROCESS_TERMINATE, false, pid);
	TerminateProcess(explorer, 1);
	CloseHandle(explorer);
}

std::string GetWindowTitle(HWND hwnd) {
    const int bufferSize = 256; // You can adjust the buffer size according to your needs
    char buffer[bufferSize];
    GetWindowText(hwnd, buffer, bufferSize);
    return std::string(buffer);
}

/**
 * returns the full executable path of the running process
 */
string getProcessName(unsigned long pid)
{
	string name = "";
	HANDLE phandle = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	TCHAR filename[MAX_PATH];
	GetModuleFileNameEx(phandle, NULL, filename, MAX_PATH);
	CloseHandle(phandle);
	return string(filename);
}

extern "C" __declspec(dllexport) DWORD getPID(HWND h)
{
	DWORD pid = 0;
	GetWindowThreadProcessId(h, &pid);
	return pid;
}

#include <windows.h>
#include <string>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/**
 * actual code
 */
void BackgroundTask(HWND hwnd) {
	while (!exitFlag) {
        Sleep(16);
        std::lock_guard<std::mutex> lock(textMutex);
        HWND activeHWND = GetForegroundWindow();
        DWORD pid = getPID(activeHWND);
        text = "EXE: " + getProcessName(pid);
        title = "Title: '" + GetWindowTitle(activeHWND) + "' PID:" + to_string(pid);
        if(activeHWND != cachedHWND || old_title != title)
        {
        	InvalidateRect(hwnd, NULL, TRUE);
        	cachedHWND = activeHWND;
        	old_title = title;
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	ShowWindow (GetConsoleWindow(), SW_HIDE);
    // ... (same as previous code)
    // Register the window class.
    const char CLASS_NAME[]  = "MyWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.
    HWND hwnd = CreateWindowEx(
        0,                               // Optional window styles.
        CLASS_NAME,                      // Window class
        "Window Finder",                     // Window text
        WS_OVERLAPPEDWINDOW,             // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 150,

        NULL,       // Parent window
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) {
        return 0;
    }

    // Make the window always on top.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(hwnd, nCmdShow);

    // Start the background thread
    std::thread backgroundThread(BackgroundTask, hwnd);

    // Run the message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        InvalidateRect(hwnd, NULL, TRUE);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        Sleep(1);
    }

    killProcess(getpid());

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
            PostQuitMessage(0);
            exitFlag = true;
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);

            // Fill the window with a background color (white in this case)
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);

            // Convert string to const char* before drawing
            const char* charText = text.c_str();
            const char* charTitle = title.c_str();

            // Draw the text on the window
            DrawText(hdc, charText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawText(hdc, charTitle, -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

