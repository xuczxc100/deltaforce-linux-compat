#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <stdio.h>
#include <string.h>

typedef NTSTATUS (NTAPI *query_fn)(HANDLE, const UNICODE_STRING *, ULONG, void *, ULONG, ULONG *);
typedef NTSTATUS (NTAPI *set_fn)(HANDLE, const UNICODE_STRING *, ULONG, ULONG, const void *, ULONG);

static unsigned int checks, failures;
static char report[8192];
static size_t report_length;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    ++failures; report_length += snprintf(report + report_length, sizeof(report) - report_length, \
    "FAIL line %u: %s\r\n", __LINE__, #condition); } } while (0)

static FARPROC resolve(HMODULE module, const char *name, BOOL raw)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)module + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY *exports = (IMAGE_EXPORT_DIRECTORY *)((BYTE *)module + dir.VirtualAddress);
    DWORD *names = (DWORD *)((BYTE *)module + exports->AddressOfNames);
    DWORD *functions = (DWORD *)((BYTE *)module + exports->AddressOfFunctions);
    WORD *ordinals = (WORD *)((BYTE *)module + exports->AddressOfNameOrdinals);
    DWORD i, rva;

    if (!raw) return GetProcAddress(module, name);
    for (i = 0; i < exports->NumberOfNames; ++i)
    {
        if (strcmp((char *)module + names[i], name)) continue;
        rva = functions[ordinals[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size)
            return NULL; /* A forwarder is not an executable entry point. */
        return (FARPROC)(void *)((BYTE *)module + rva);
    }
    return NULL;
}

static void check_value(query_fn query, HKEY key, WCHAR *buffer, USHORT length,
                        DWORD expected_value)
{
    UNICODE_STRING name = {length, length, buffer};
    UNICODE_STRING original = name;
    ULONG size = 0;
    struct {ULONG title, type, length; DWORD value;} data;
    NTSTATUS status;

    status = query(key, &name, 2, NULL, 0, &size);
    CHECK(status == STATUS_BUFFER_TOO_SMALL);
    CHECK(size == sizeof(data));
    memset(&data, 0xcc, sizeof(data));
    status = query(key, &name, 2, &data, sizeof(data), &size);
    CHECK(status == STATUS_SUCCESS);
    CHECK(data.type == REG_DWORD);
    CHECK(data.length == sizeof(DWORD));
    CHECK(data.value == expected_value);
    CHECK(name.Buffer == original.Buffer && name.Length == original.Length
          && name.MaximumLength == original.MaximumLength);
}

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll"), target;
    query_fn query;
    set_fn set_value = (set_fn)(void *)GetProcAddress(ntdll, "NtSetValueKey");
    HKEY key;
    DWORD value = 42, embedded_value = 73, default_value = 91, missing_size;
    LONG error;
    WCHAR padded[] = L"Value\0\0";
    WCHAR embedded[] = {L'A', 0, L'B', 0, 0};
    WCHAR all_null[] = {0, 0, 0};
    WCHAR missing[] = L"Missing\0";
    UNICODE_STRING name;
    NTSTATUS status;
    char key_path[160], result[160];
    HANDLE output;
    DWORD written;
    BOOL raw = argc == 3 && !strcmp(argv[2], "raw");

    if (argc != 2 && !raw) return 2;
    target = LoadLibraryA(argv[1]);
    if (!target) return 3;
    query = (query_fn)(void *)resolve(target,
              !strcmp(argv[1], "regcompat.dll") ? "Query" :
              !strcmp(argv[1], "ntoskrnl.exe") ? "ZwQueryValueKey" : "NtQueryValueKey", raw);
    set_value = (set_fn)(void *)resolve(target,
              !strcmp(argv[1], "regcompat.dll") ? "Set" :
              !strcmp(argv[1], "ntoskrnl.exe") ? "ZwSetValueKey" : "NtSetValueKey", raw);
    if (!query || !set_value) return 4;

    snprintf(key_path, sizeof(key_path), "Software\\DeltaForceCompatQueryTest-%lu", GetCurrentProcessId());
    error = RegCreateKeyExA(HKEY_CURRENT_USER, key_path, 0, NULL, 0,
                           KEY_ALL_ACCESS, NULL, &key, NULL);
    if (error) return 5;
    CHECK(RegSetValueExW(key, L"Value", 0, REG_DWORD, (BYTE *)&value, sizeof(value)) == ERROR_SUCCESS);
    CHECK(RegSetValueExW(key, NULL, 0, REG_DWORD, (BYTE *)&default_value, sizeof(default_value)) == ERROR_SUCCESS);

    name.Buffer = embedded; name.Length = 3 * sizeof(WCHAR); name.MaximumLength = sizeof(embedded);
    status = set_value(key, &name, 0, REG_DWORD, &embedded_value, sizeof(embedded_value));
    CHECK(status == STATUS_SUCCESS);

    check_value(query, key, padded, 5 * sizeof(WCHAR), value);
    check_value(query, key, padded, 6 * sizeof(WCHAR), value);
    check_value(query, key, padded, 7 * sizeof(WCHAR), value);
    check_value(query, key, embedded, 3 * sizeof(WCHAR), embedded_value);
    check_value(query, key, embedded, 5 * sizeof(WCHAR), embedded_value);
    check_value(query, key, all_null, 0, default_value);
    check_value(query, key, all_null, sizeof(all_null), default_value);

    name.Buffer = missing; name.Length = sizeof(missing); name.MaximumLength = sizeof(missing);
    missing_size = 0xfeed;
    status = query(key, &name, 2, NULL, 0, &missing_size);
    CHECK(status == STATUS_OBJECT_NAME_NOT_FOUND);
    CHECK(missing_size == 0xfeed);

    value = 137;
    name.Buffer = padded; name.Length = sizeof(padded); name.MaximumLength = sizeof(padded);
    CHECK(set_value(key, &name, 0, REG_DWORD, &value, sizeof(value)) == STATUS_SUCCESS);
    CHECK(name.Length == sizeof(padded));
    check_value(query, key, padded, 5 * sizeof(WCHAR), value);

    embedded_value = 158;
    name.Buffer = embedded; name.Length = sizeof(embedded); name.MaximumLength = sizeof(embedded);
    CHECK(set_value(key, &name, 0, REG_DWORD, &embedded_value, sizeof(embedded_value)) == STATUS_SUCCESS);
    check_value(query, key, embedded, 3 * sizeof(WCHAR), embedded_value);

    default_value = 175;
    name.Buffer = all_null; name.Length = sizeof(all_null); name.MaximumLength = sizeof(all_null);
    CHECK(set_value(key, &name, 0, REG_DWORD, &default_value, sizeof(default_value)) == STATUS_SUCCESS);
    check_value(query, key, all_null, 0, default_value);
    CHECK(RegCloseKey(key) == ERROR_SUCCESS);
    CHECK(RegDeleteKeyA(HKEY_CURRENT_USER, key_path) == ERROR_SUCCESS);

    report_length += snprintf(report + report_length, sizeof(report) - report_length,
                  "%s%s: %u checks, %u failures\r\n", argv[1], raw ? " raw EAT" : "", checks, failures);
    snprintf(result, sizeof(result), "C:\\fwpkclnt-probe\\query-test-%s%s.txt", argv[1], raw ? "-raw" : "");
    output = CreateFileA(result, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (output == INVALID_HANDLE_VALUE) return 6;
    if (!WriteFile(output, report, (DWORD)report_length, &written, NULL)
        || written != report_length) return 7;
    CloseHandle(output);
    fputs(report, stdout);
    return failures != 0;
}
