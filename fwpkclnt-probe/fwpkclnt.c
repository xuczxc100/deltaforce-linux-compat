/* Diagnostic PE import provider for Wine, NOT a WFP implementation.
 * Every WFP operation returns STATUS_NOT_SUPPORTED. No filters or callouts
 * are registered. This lets a trace distinguish missing imports from an
 * actual attempt to use unsupported functionality.
 *
 * Kernel-mode signatures: Microsoft Learn, fwpmk.h and fwpsk.h.
 * SPDX-License-Identifier: MIT
 */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

__declspec(dllimport) LONG __cdecl DbgPrint(const char *format, ...);

static LONG unsupported(const char *name)
{
    DbgPrint("fwpkclnt-probe: %s -> STATUS_NOT_SUPPORTED (0xc00000bb)\n", name);
    return STATUS_NOT_SUPPORTED;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DbgPrint("fwpkclnt-probe: diagnostic module loaded; WFP is unavailable\n");
    return TRUE;
}

LONG WINAPI FwpmEngineOpen0(const WCHAR *server, UINT32 authentication,
                          void *identity, const void *session, HANDLE *engine)
{
    (void)server; (void)authentication; (void)identity; (void)session;
    if (engine) *engine = NULL;
    return unsupported("FwpmEngineOpen0");
}

LONG WINAPI FwpmEngineClose0(HANDLE engine)
{
    (void)engine;
    return unsupported("FwpmEngineClose0");
}

LONG WINAPI FwpmSubLayerAdd0(HANDLE engine, const void *sublayer, void *security)
{
    (void)engine; (void)sublayer; (void)security;
    return unsupported("FwpmSubLayerAdd0");
}

LONG WINAPI FwpmSubLayerDeleteByKey0(HANDLE engine, const GUID *key)
{
    (void)engine; (void)key;
    return unsupported("FwpmSubLayerDeleteByKey0");
}

LONG WINAPI FwpmCalloutAdd0(HANDLE engine, const void *callout, void *security,
                          UINT32 *identifier)
{
    (void)engine; (void)callout; (void)security;
    if (identifier) *identifier = 0;
    return unsupported("FwpmCalloutAdd0");
}

LONG WINAPI FwpmCalloutDeleteById0(HANDLE engine, UINT32 identifier)
{
    (void)engine; (void)identifier;
    return unsupported("FwpmCalloutDeleteById0");
}

LONG WINAPI FwpmFilterAdd0(HANDLE engine, const void *filter, void *security,
                         UINT64 *identifier)
{
    (void)engine; (void)filter; (void)security;
    if (identifier) *identifier = 0;
    return unsupported("FwpmFilterAdd0");
}

LONG WINAPI FwpmFilterDeleteById0(HANDLE engine, UINT64 identifier)
{
    (void)engine; (void)identifier;
    return unsupported("FwpmFilterDeleteById0");
}

LONG WINAPI FwpsCalloutRegister1(void *device, const void *callout,
                               UINT32 *identifier)
{
    (void)device; (void)callout;
    if (identifier) *identifier = 0;
    return unsupported("FwpsCalloutRegister1");
}

LONG WINAPI FwpsCalloutUnregisterById0(UINT32 identifier)
{
    (void)identifier;
    return unsupported("FwpsCalloutUnregisterById0");
}
