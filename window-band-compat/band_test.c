/* Local runtime checks for the experimental Wine desktop-band query.
 * Native Windows error-code conformance has not yet been independently run.
 * SPDX-License-Identifier: MIT
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef BOOL (WINAPI *band_fn)(HWND, DWORD *);
extern BOOL WINAPI raw_band_syscall(HWND, DWORD *, DWORD);
static unsigned checks, failures;
static FILE *report;
#define CHECK(cond, name) do { checks++; if (!(cond)) { failures++; \
    fprintf(report, "FAIL %s (line %d, last error %lu)\n", name, __LINE__, GetLastError()); } } while (0)

static HWND make_window(DWORD ex, DWORD style, HWND parent)
{
    return CreateWindowExW(ex, L"STATIC", L"Wine band compatibility test", style,
                          0, 0, 64, 64, parent, NULL, GetModuleHandleW(NULL), NULL);
}

static int child(unsigned long parent)
{
    char name[100]; HANDLE mapping, event; volatile uint64_t *shared;
    HWND hwnd;
    snprintf(name, sizeof(name), "Local\\DFBandMapping-%lu", parent);
    mapping = OpenFileMappingA(FILE_MAP_WRITE, FALSE, name);
    if (!mapping) return 2;
    shared = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(*shared));
    if (!shared) return 3;
    snprintf(name, sizeof(name), "Local\\DFBandReady-%lu", parent);
    event = OpenEventA(EVENT_MODIFY_STATE, FALSE, name);
    hwnd = make_window(0, WS_OVERLAPPED, NULL);
    *shared = (uintptr_t)hwnd;
    if (event) SetEvent(event);
    for (unsigned i = 0; i < 1000 && IsWindow(hwnd); ++i)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    if (event) CloseHandle(event);
    UnmapViewOfFile((const void *)shared); CloseHandle(mapping);
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE module; band_fn get_band; HWND normal, topmost, child_hwnd, message;
    DWORD value, old_protect; DWORD *page; BYTE *stub; DWORD number = 0;
    if (argc == 3 && !strcmp(argv[1], "child")) return child(strtoul(argv[2], NULL, 10));
    CreateDirectoryW(L"C:\\window-band-tests", NULL);
    report = fopen("C:\\window-band-tests\\result.txt", "w");
    if (!report) return 2;
    module = LoadLibraryW(L"win32u.dll");
    get_band = (band_fn)(uintptr_t)GetProcAddress(module, "NtUserGetWindowBand");
    CHECK(get_band != NULL, "API is exported");
    if (!get_band) goto done;
    stub = (BYTE *)(uintptr_t)get_band;
    if (!memcmp(stub, "\x4c\x8b\xd1\xb8", 4)) memcpy(&number, stub + 4, sizeof(number));
    CHECK(number == 0x145a, "recorded runner syscall number");
    normal = make_window(0, WS_OVERLAPPED, NULL);
    topmost = make_window(WS_EX_TOPMOST, WS_OVERLAPPED, NULL);
    child_hwnd = make_window(0, WS_CHILD, normal);
    message = make_window(0, 0, HWND_MESSAGE);
    CHECK(normal && topmost && child_hwnd && message, "real test windows created");
    HWND windows[] = {normal, topmost, child_hwnd, message, GetDesktopWindow()};
    for (unsigned i = 0; i < sizeof(windows) / sizeof(windows[0]); ++i)
    {
        struct { DWORD before, band, after; } guarded = {0x12345678, 0xdeadbeef, 0x87654321};
        SetLastError(0x1234abcd);
        CHECK(get_band(windows[i], &guarded.band), "valid window succeeds");
        CHECK(guarded.band == 1, "existing desktop model reported");
        CHECK(guarded.before == 0x12345678 && guarded.after == 0x87654321, "only output DWORD changed");
        CHECK(GetLastError() == 0x1234abcd, "success preserves last error");
        if (number)
        {
            value = 0xdeadbeef;
            CHECK(raw_band_syscall(windows[i], &value, number), "direct syscall succeeds");
            CHECK(value == 1, "direct syscall desktop model");
        }
    }
    HWND invalid[] = {NULL, (HWND)(uintptr_t)0xdeadbeef, (HWND)(intptr_t)-1};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        value = 0xdeadbeef; SetLastError(0);
        CHECK(!get_band(invalid[i], &value), "invalid window rejected");
        CHECK(value == 0xdeadbeef, "failure retains output");
        CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "invalid window error");
    }
    SetLastError(0);
    CHECK(!get_band(normal, NULL), "NULL output rejected");
    CHECK(GetLastError() == ERROR_NOACCESS, "NULL output error");
    page = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(page != NULL, "output test page allocated");
    if (page)
    {
        *page = 0xdeadbeef;
        CHECK(VirtualProtect(page, 4096, PAGE_READONLY, &old_protect), "read-only output setup");
        SetLastError(0);
        CHECK(!get_band(normal, page), "read-only output rejected without exception");
        CHECK(GetLastError() == ERROR_NOACCESS, "read-only output error");
        CHECK(*page == 0xdeadbeef, "read-only contents retained");
        CHECK(VirtualProtect(page, 4096, PAGE_NOACCESS, &old_protect), "no-access output setup");
        CHECK(!get_band(normal, page), "no-access output rejected without exception");
        CHECK(VirtualFree(page, 0, MEM_RELEASE), "output page released");
        CHECK(!get_band(normal, page), "unmapped output rejected without exception");
    }
    {
        char mapping_name[100], event_name[100], executable[MAX_PATH], command[MAX_PATH + 60];
        HANDLE mapping, event; volatile uint64_t *shared;
        STARTUPINFOA si = {0}; PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        snprintf(mapping_name, sizeof(mapping_name), "Local\\DFBandMapping-%lu", GetCurrentProcessId());
        snprintf(event_name, sizeof(event_name), "Local\\DFBandReady-%lu", GetCurrentProcessId());
        mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(*shared), mapping_name);
        event = CreateEventA(NULL, TRUE, FALSE, event_name);
        shared = mapping ? MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(*shared)) : NULL;
        CHECK(mapping && event && shared, "cross-process test IPC created");
        if (shared && event)
        {
            *shared = 0;
            GetModuleFileNameA(NULL, executable, sizeof(executable));
            snprintf(command, sizeof(command), "\"%s\" child %lu", executable, GetCurrentProcessId());
            BOOL created = CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            CHECK(created, "foreign-window process created");
            if (created)
            {
                CHECK(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "foreign window ready");
                HWND foreign = (HWND)(uintptr_t)*shared;
                value = 0xdeadbeef;
                CHECK(foreign && get_band(foreign, &value), "foreign window succeeds");
                CHECK(value == 1, "foreign desktop band");
                PostMessageW(foreign, WM_CLOSE, 0, 0);
                if (WaitForSingleObject(pi.hProcess, 3000) != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 3);
                CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            }
        }
        if (shared) UnmapViewOfFile((const void *)shared);
        if (event) CloseHandle(event);
        if (mapping) CloseHandle(mapping);
    }
    DestroyWindow(child_hwnd); DestroyWindow(message); DestroyWindow(topmost); DestroyWindow(normal);
    value = 0xdeadbeef;
    CHECK(!get_band(normal, &value), "destroyed window rejected");
    CHECK(value == 0xdeadbeef, "destroyed-window output retained");
done:
    fprintf(report, "%u checks, %u failures\n", checks, failures);
    fclose(report);
    return failures ? 1 : 0;
}
