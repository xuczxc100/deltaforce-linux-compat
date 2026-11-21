#include <windows.h>
#include <stdio.h>

int run_device_cases(void *, unsigned int *);

int main(void)
{
    HMODULE module = LoadLibraryA("ntoskrnl.exe");
    FARPROC function;
    unsigned int checks = 0, failures;
    HANDLE output;
    DWORD written;
    char summary[128];
    int length;

    if (!module) return 2;
    function = GetProcAddress(module, "IoGetBaseFileSystemDeviceObject");
    if (!function) return 3;
    failures = run_device_cases((void *)function, &checks);
    length = snprintf(summary, sizeof(summary), "basefs: %u checks, %u failures\r\n", checks, failures);
    output = CreateFileA("C:\\fwpkclnt-probe\\basefs-result.txt", GENERIC_WRITE,
                         FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (output == INVALID_HANDLE_VALUE) return 4;
    if (!WriteFile(output, summary, length, &written, NULL) || written != (DWORD)length) return 5;
    CloseHandle(output);
    fputs(summary, stdout);
    return failures != 0;
}
