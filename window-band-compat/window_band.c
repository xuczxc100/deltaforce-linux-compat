/* Experimental Wine Unix-side desktop-window band query.
 * SPDX-License-Identifier: MIT
 *
 * Wine currently implements a single desktop band. This does not implement
 * privileged Windows immersive/UIAccess bands or change any window's order.
 * Keep the real Wine window validation and guarded NtWriteVirtualMemory path.
 */
#include <stdint.h>
#include <stddef.h>

extern void *band_query_window(void *hwnd, unsigned int info_class);
extern void band_set_error(uint32_t error);
extern void *band_lookup(void *handle, const char *name);

typedef int32_t (*write_memory_fn)(void *, void *, const void *, size_t, size_t *);

__attribute__((visibility("hidden")))
int wine_desktop_window_band(void *hwnd, uint32_t *band)
{
    write_memory_fn write_memory;
    uint32_t desktop_band = 1;
    size_t written = 0;

    if (!band_query_window(hwnd, 0))
    {
        band_set_error(1400); /* ERROR_INVALID_WINDOW_HANDLE */
        return 0;
    }
    if (!band)
    {
        band_set_error(998); /* ERROR_NOACCESS */
        return 0;
    }
    write_memory = (write_memory_fn)band_lookup(NULL, "NtWriteVirtualMemory");
    if (!write_memory)
    {
        band_set_error(120); /* ERROR_CALL_NOT_IMPLEMENTED */
        return 0;
    }
    if (write_memory((void *)(intptr_t)-1, band, &desktop_band,
                     sizeof(desktop_band), &written) || written != sizeof(desktop_band))
    {
        band_set_error(998);
        return 0;
    }
    return 1;
}
