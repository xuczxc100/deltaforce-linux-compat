/* Experimental Wine kernel-export adapter for NtQueryValueKey.
 * Preserve embedded NULs, trim trailing WCHAR NULs only.
 * No registry entries are invented and no errors are changed to success.
 * SPDX-License-Identifier: MIT
 */
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

__declspec(dllimport) LONG __cdecl DbgPrint(const char *format, ...);
__declspec(dllimport) NTSTATUS NTAPI NtQueryValueKey(HANDLE, const UNICODE_STRING *,
                                                   ULONG, void *, ULONG, ULONG *);
__declspec(dllimport) NTSTATUS NTAPI NtSetValueKey(HANDLE, const UNICODE_STRING *,
                                                 ULONG, ULONG, const void *, ULONG);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}

NTSTATUS NTAPI Query(HANDLE key, const UNICODE_STRING *name, ULONG information_class,
                     void *information, ULONG length, ULONG *result_length)
{
    UNICODE_STRING trimmed;
    NTSTATUS status;

    /* Leave malformed odd byte lengths to the underlying implementation. */
    if (!name || (name->Length & (sizeof(WCHAR) - 1)))
        return NtQueryValueKey(key, name, information_class, information, length, result_length);

    trimmed = *name;
    while (trimmed.Length >= sizeof(WCHAR)
           && !trimmed.Buffer[trimmed.Length / sizeof(WCHAR) - 1])
        trimmed.Length -= sizeof(WCHAR);

    status = NtQueryValueKey(key, &trimmed, information_class, information, length, result_length);
    if (trimmed.Length != name->Length)
        DbgPrint("regcompat: trimmed %u trailing WCHAR NUL(s), status 0x%08lx\n",
                 (name->Length - trimmed.Length) / (unsigned int)sizeof(WCHAR), (ULONG)status);
    return status;
}

NTSTATUS NTAPI Set(HANDLE key, const UNICODE_STRING *name, ULONG title_index,
                   ULONG type, const void *data, ULONG length)
{
    UNICODE_STRING trimmed;
    NTSTATUS status;

    if (!name || (name->Length & (sizeof(WCHAR) - 1)))
        return NtSetValueKey(key, name, title_index, type, data, length);
    trimmed = *name;
    while (trimmed.Length >= sizeof(WCHAR)
           && !trimmed.Buffer[trimmed.Length / sizeof(WCHAR) - 1])
        trimmed.Length -= sizeof(WCHAR);
    status = NtSetValueKey(key, &trimmed, title_index, type, data, length);
    if (trimmed.Length != name->Length)
        DbgPrint("regcompat: Set trimmed %u trailing WCHAR NUL(s), status 0x%08lx\n",
                 (name->Length - trimmed.Length) / (unsigned int)sizeof(WCHAR), (ULONG)status);
    return status;
}
