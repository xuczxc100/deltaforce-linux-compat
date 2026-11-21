#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <stdio.h>

/* Link directly against the diagnostic provider to test the Windows ABI. */
LONG WINAPI FwpmEngineOpen0(const WCHAR *, UINT32, void *, const void *, HANDLE *);
LONG WINAPI FwpmEngineClose0(HANDLE);
LONG WINAPI FwpmSubLayerAdd0(HANDLE, const void *, void *);
LONG WINAPI FwpmSubLayerDeleteByKey0(HANDLE, const GUID *);
LONG WINAPI FwpmCalloutAdd0(HANDLE, const void *, void *, UINT32 *);
LONG WINAPI FwpmCalloutDeleteById0(HANDLE, UINT32);
LONG WINAPI FwpmFilterAdd0(HANDLE, const void *, void *, UINT64 *);
LONG WINAPI FwpmFilterDeleteById0(HANDLE, UINT64);
LONG WINAPI FwpsCalloutRegister1(void *, const void *, UINT32 *);
LONG WINAPI FwpsCalloutUnregisterById0(UINT32);

static unsigned failures, checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    printf("FAIL at line %u: %s\n", __LINE__, #condition); ++failures; } } while (0)

int main(void)
{
    HANDLE engine = (HANDLE)(ULONG_PTR)0xfeed;
    UINT32 identifier = 0xfeed;
    UINT64 filter_id = 0xfeed;
    GUID key = {0};
    HANDLE output;
    DWORD written;
    char summary[128];
    int length;

    CHECK(FwpmEngineOpen0(NULL, 10, NULL, NULL, &engine) == STATUS_NOT_SUPPORTED);
    CHECK(engine == NULL);
    CHECK(FwpmEngineOpen0(NULL, 10, NULL, NULL, NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmEngineClose0(NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmSubLayerAdd0(NULL, NULL, NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmSubLayerDeleteByKey0(NULL, &key) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmCalloutAdd0(NULL, NULL, NULL, &identifier) == STATUS_NOT_SUPPORTED);
    CHECK(identifier == 0);
    CHECK(FwpmCalloutAdd0(NULL, NULL, NULL, NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmCalloutDeleteById0(NULL, 0xfeed) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmFilterAdd0(NULL, NULL, NULL, &filter_id) == STATUS_NOT_SUPPORTED);
    CHECK(filter_id == 0);
    CHECK(FwpmFilterAdd0(NULL, NULL, NULL, NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpmFilterDeleteById0(NULL, 0xfeed) == STATUS_NOT_SUPPORTED);
    identifier = 0xfeed;
    CHECK(FwpsCalloutRegister1(NULL, NULL, &identifier) == STATUS_NOT_SUPPORTED);
    CHECK(identifier == 0);
    CHECK(FwpsCalloutRegister1(NULL, NULL, NULL) == STATUS_NOT_SUPPORTED);
    CHECK(FwpsCalloutUnregisterById0(0xfeed) == STATUS_NOT_SUPPORTED);
    length = snprintf(summary, sizeof(summary), "fwpkclnt-probe: %u checks, %u failures\r\n", checks, failures);
    fputs(summary, stdout);
    output = CreateFileA("C:\\fwpkclnt-probe\\test-result.txt", GENERIC_WRITE,
                         FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (output == INVALID_HANDLE_VALUE) return 2;
    if (!WriteFile(output, summary, length, &written, NULL) || written != (DWORD)length)
    {
        CloseHandle(output);
        return 2;
    }
    CloseHandle(output);
    return failures != 0;
}
