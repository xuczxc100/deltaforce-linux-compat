# Desktop window-band compatibility

This experiment implements the missing **native x64** `NtUserGetWindowBand` syscall for the recorded GE-Proton11-12 Wine Unix library. It queries real Wine window handles and reports the desktop band used by Wine's current window model. It does not create privileged/UIAccess/immersive bands or change a window's order.

The Windows desktop-band value is `ZBID_DESKTOP = 1`; see the original researcher's [window-band description](https://blog.adeltax.com/window-z-order-in-windows-10/). Native Windows error-code conformance has not been independently run for this project.

## Why this exists

Three local game dumps recorded `0x80000100` at `win32u.dll + 0x14554`, inside the recorded runner's `NtUserGetWindowBand` syscall entry. The same runs failed approximately 300 seconds after initialization. The Windows-side syscall entry and syscall number are preserved by this overlay; both normal export calls and direct syscalls reach the new Unix-side implementation.

This is a Wine API implementation, not a modification to game or anti-cheat files. The fact that the diagnostic files were collected by ACE does not establish which component originally requested the API.

## Implementation

- Validate the supplied handle using Wine's existing `NtUserQueryWindow`.
- Use the existing `NtWriteVirtualMemory` implementation to copy the output safely, returning a failure for inaccessible output memory.
- Preserve the caller's last-error value on success. Invalid window handles and inaccessible output pointers fail rather than reporting unconditional success.
- Replace the original Unix-side function's initial five bytes with a relative jump and update its syscall argument metadata from zero to 16 bytes.
- Append an RX-only ELF segment containing the implementation and compiler-generated unwind data. Retain all original unwind-table entries and add the new function's entry.
- Reuse the build-id `PT_NOTE` program-header slot for that segment; the original note bytes remain. The original build-id therefore **does not identify the complete modified artifact**. Use the manifest's output SHA-256.
- Keep the existing non-executable stack and RELRO program headers.

The builder accepts only source SHA-256 `2585a00d63361196b02eeff2ab153dde983fab51b7916d9cb2e9fc7025a9fe6e`. Do not apply it to another Wine build or an already overlaid library.

## Build and local checks

Required: native x86-64 GCC/binutils, x86-64 MinGW-w64, Make, and Python 3.

```sh
make -C window-band-compat
make -C window-band-compat overlay \
    WIN32U_SOURCE=/path/to/pristine-GE-Proton11-12-files/lib/wine/x86_64-unix/win32u.so
```

The recorded local runtime test completed **60 checks, zero failures**. It covers ordinary, topmost, child, message-only, desktop, cross-process, destroyed and invalid windows; output canaries; last-error preservation; NULL, read-only, inaccessible and unmapped output memory; and native x64 direct syscalls.

Place `build/band_test.exe` in the experimental prefix and execute it through that prefix's runner. It creates and removes its own temporary windows and one child process. Results are written to `C:\window-band-tests\result.txt`.

The checks exercise the existing single-band Wine model; they are not independent proof of Windows behavior for every edge case.

## Install or roll back

Use a **private runner copy**. Stop every bottle using that private runner before replacing its Unix library. Back up `lib/wine/x86_64-unix/win32u.so`, then install the built file at that same relative path using an atomic rename. Restore the backup the same way to roll back. Do not replace the stock/shared runner or update a library in place while it is mapped by running processes.

The source diff in `0003-win32u-desktop-window-band.patch` includes a WoW64 bridge as a candidate upstream change. A complete Wine source build and the 32-bit/WoW64 path have **not** been validated here; the executed overlay and direct-syscall checks cover the x64 game path.

Original tools and test code are MIT licensed. Wine source-diff context remains LGPL-2.1-or-later under the repository's existing notice.
