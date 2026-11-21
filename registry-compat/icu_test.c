#include <windows.h>
#include <stdio.h>

int main(void)
{
    HMODULE module = LoadLibraryA("icu.dll");
    void (__cdecl *convert)(const char *, WCHAR *, int) = NULL;
    void (__cdecl *version)(unsigned char *) = NULL;
    int (__cdecl *length)(const WCHAR *) = NULL;
    WCHAR text[4] = {0xffff, 0xffff, 0xffff, 0xffff};
    unsigned char v[4] = {0};
    unsigned int checks = 0, failures = 0;
    HANDLE output;
    DWORD written;
    char summary[160];
    int size;
#define CHECK(condition) do { ++checks; if (!(condition)) ++failures; } while (0)

    CHECK(module != NULL);
    if (module)
    {
        convert = (void *)GetProcAddress(module, "u_charsToUChars");
        version = (void *)GetProcAddress(module, "u_getVersion");
        length = (void *)GetProcAddress(module, "u_strlen");
        CHECK(convert != NULL); CHECK(version != NULL); CHECK(length != NULL);
        if (convert) { convert("ABC", text, 4); CHECK(text[0] == 'A' && text[1] == 'B' && text[2] == 'C' && text[3] == 0); }
        if (version) { version(v); CHECK(v[0] == 68); }
        if (length && convert) CHECK(length(text) == 3);
    }
    size = snprintf(summary, sizeof(summary), "ICU %u.%u.%u.%u: %u checks, %u failures\r\n", v[0], v[1], v[2], v[3], checks, failures);
    output = CreateFileA("C:\\fwpkclnt-probe\\icu-test.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (output == INVALID_HANDLE_VALUE) return 2;
    if (!WriteFile(output, summary, size, &written, NULL) || written != (DWORD)size) return 3;
    CloseHandle(output);
    return failures != 0;
}
