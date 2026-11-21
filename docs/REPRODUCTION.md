# Reproduction notes

This is a record of an experimental setup, not a one-command compatibility guarantee. Keep the original prefix and runner available. Stop the experimental prefix before replacing its runtime modules.

## 1. Prepare a private environment

Create a separate 64-bit Bottles prefix using the Wine files from GE-Proton11-12. Copy its `files` directory to a private runner directory. The game itself must be obtained from its publisher; this repository does not provide it.

Use these environment values for the recorded configuration:

```text
WINEARCH=wow64
WINEESYNC=0
WINEFSYNC=0
```

`WINEARCH=wow64` selects Wine's newer WoW64 path for the 32-bit launcher. The prefix architecture remains 64-bit. The synchronization settings above reproduce the test; other settings have not been validated here.

## 2. Build the experimental Wine overlay

```sh
make -C registry-compat
GE_FILES=/path/to/GE-Proton11-12-x86_64/files
python3 registry-compat/redirect_export.py \
  "$GE_FILES/lib/wine/x86_64-windows/ntoskrnl.exe" \
  registry-compat/build/ntoskrnl.exe
```

The tool writes a separate output and a JSON manifest. The original source image is not modified. The tested original SHA-256 was:

```text
f09883413c67bcc881c656c6f2df4b47db38b0bdd79b1d27f92bf67cb80c2fe1
```

After stopping the experimental prefix, place the generated file at `lib/wine/x86_64-windows/ntoskrnl.exe` in the **private runner copy**. Do not replace the original package's file.

The overlay keeps the old `.text` bytes intact and redirects three export-table entries to newly added executable code:

- `ZwQueryValueKey`
- `ZwSetValueKey`
- `IoGetBaseFileSystemDeviceObject`

The registry wrappers call the existing ntdll import slots. They preserve the six-argument Windows x64 ABI and have unwind metadata. The file-system selector is a leaf function with no stack allocation.

Executable wrappers are necessary here: an intermediate experiment with PE export forwarders passed ordinary API tests but failed when a driver looked up an export address directly. The final overlay does not use forwarder strings for these exports.

## 3. Install the diagnostic WFP import provider

```sh
make -C fwpkclnt-probe
```

Place `fwpkclnt-probe/build/fwpkclnt.sys` in `drive_c/windows/system32/drivers` in the experimental prefix and select native loading for `fwpkclnt.sys` in its Wine DLL overrides.

This is not a working WFP driver. It exports the ten imported names, logs calls when Wine's `debugstr` channel is enabled, zeroes output handles/IDs, and returns `STATUS_NOT_SUPPORTED` for every operation. It must not be presented as functioning packet filtering or complete anti-cheat support.

## 4. Install the bundled ICU dependencies

Wine's `icu.dll` in this package forwards functions to ICU 68 DLLs. When invoked outside Proton's launcher environment, those separate DLL directories were not on the loader's search path in the recorded setup.

```sh
python3 tools/install_icu.py /path/to/private-runner /path/to/experimental-prefix
```

The script copies the existing package's matching 32-bit and 64-bit ICU DLLs to the prefix's `syswow64` and `system32` directories. It does not download an unrelated ICU version, change the system Wine installation, or overwrite conflicting existing files.

## 5. Run the component checks

Create `C:\fwpkclnt-probe` in the experimental prefix. Place the built test executables there, together with `fwpkclnt.sys` for `probe_test.exe`.

Run under the experimental prefix:

```text
probe_test.exe
query_test.exe ntdll.dll
query_test.exe ntoskrnl.exe raw
basefs_test.exe
icu_test.exe
```

The `ntdll.dll` case reproduces the original registry discrepancy; its failing checks are expected on the recorded original runtime. The `ntoskrnl.exe raw` case deliberately resolves callable addresses directly from the PE export table.

The programs write their results to `C:\fwpkclnt-probe`. Registry tests create a temporary per-process key under the current user's Software hive and remove it on normal completion.

## Launcher font recovery

A later blank-launcher incident recovered in an isolated prefix after restoring the pre-repair registry/runtime state and adding the same runner's bundled Tahoma fonts. With the experimental prefix stopped, copy only missing matching fonts:

```sh
python3 tools/install_launcher_fonts.py /path/to/private-runner /path/to/experimental-prefix
```

The helper refuses conflicting existing files and distributes no font binaries. The main menu was subsequently observed and the user confirmed normal operation. Other state was restored during diagnosis, so the font change has not been isolated as the sole cause.

## 6. Start through the official launcher

Use the official Garena launcher in the same prefix. Starting the game executable directly reached a publisher SDK initialization failure in this test, while the normal launcher supplied its expected startup context.

The observed sequence after the fixes was:

1. Launcher UI rendered in the new WoW64 mode.
2. Game startup passed the previously failing driver initialization stages.
3. The game log recorded `ACE Init Success`.
4. Missing ICU dependency resolution was corrected.
5. The game downloaded approximately 93 MB of updates and completed shader warm-up.
6. First-run language selection appeared.

No online matchmaking test was performed. There is no claim that future updates, every anti-cheat component, or a long gameplay session will work.

## Source diffs

The two `.patch` files are candidate upstream source changes. They were prepared against Wine commit `97027f879ba9a36dbe594720d4710818536e5433`; the full source tree was not rebuilt for the recorded test. The native Windows API expectations still need independent Windows conformance testing.
