fwpkclnt diagnostic import provider

This is a diagnostic module. It implements no Windows Filtering Platform
filter engine, callout delivery, or packet inspection. Every exported WFP
operation logs its name and returns STATUS_NOT_SUPPORTED (0xc00000bb).
Output handles and IDs are zeroed; no operation reports success.

Purpose: separate PE import resolution failures from runtime use of WFP.
A driver still must handle the unsupported operations itself.

Build: make
Compiler: gcc-mingw-w64-x86-64-posix (Ubuntu package).
Run build/probe_test.exe with the module on its DLL search path under Wine.
Enable WINEDEBUG=+debugstr to see the diagnostic messages.
Only install in an isolated experimental prefix, never a host Windows OS.

Kernel-mode ABI references:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fwpmk/nf-fwpmk-fwpmengineopen0
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fwpsk/nf-fwpsk-fwpscalloutregister1

The source is MIT licensed. The module is not a game compatibility fix and
must not be presented as working anti-cheat or working packet filtering.
