# Garena Delta Force: experimental Wine compatibility work

Source-only Wine compatibility experiments developed while investigating the Garena PC client on Linux.

Development disclosure: this work is substantially AI-assisted. The recorded builds and component checks were executed locally; independent review and native Windows conformance testing are still needed.

**Observed result:** the game reached its first-run language-selection screen after the official launcher started it, downloaded approximately 93 MB of game updates, and completed shader warm-up. The game log reported `ACE Init Success`.

**Online matchmaking and sustained gameplay have not been tested. This is not official Linux support, a complete anti-cheat implementation, or a finished compatibility release.**

## What is included

| Component | Purpose |
|---|---|
| Registry compatibility | Trim trailing UTF-16 NULs in `NtQueryValueKey` / `NtSetValueKey`, preserving embedded NULs, actual registry contents, and underlying error codes. |
| File-system device selection | Implement the object-selection logic for `IoGetBaseFileSystemDeviceObject`. |
| Wine runtime overlay builder | Add executable x64 wrappers to a **separate Wine `ntoskrnl.exe` copy**, retaining its original code and import slots and adding unwind metadata. |
| `fwpkclnt` diagnostic provider | Resolve ten WFP imports so that runtime use can be traced. **Every WFP operation returns `STATUS_NOT_SUPPORTED`; no packet filtering is implemented.** |
| ICU dependency installer | Install the matching ICU DLLs already bundled with GE-Proton into an experimental prefix. |
| Test programs and source diffs | Reproduction cases, compiler sources, and candidate upstream Wine patches. |

No game executable, proprietary driver, decompiled game source, account information, token, raw runtime log, or patched Wine binary is distributed here.

## Tested configuration

- Garena Delta Force PC client, locally tested on 2026-11-21.
- Wine binaries from **GE-Proton11-12 x86_64**, invoked through Bottles.
- A separate `win64` experimental prefix and a private copy of the runner.
- `WINEARCH=wow64` for the 32-bit Garena launcher.
- Esync/fsync disabled for the recorded test; this is not a performance recommendation.
- DXVK 3.1.1 and VKD3D-Proton 3.1.

Directly using the Wine binaries from a Proton package is a custom setup. The results here should not be read as a supported GE-Proton configuration or a general regression report against GE-Proton.

## Build

On the Debian/Ubuntu environment used for this work:

```sh
sudo apt install gcc-mingw-w64-x86-64-posix make python3
make -C registry-compat
make -C fwpkclnt-probe
```

The device-object tests use the DDK headers shipped with MinGW-w64. See the Makefile if your distribution installs those headers elsewhere.

See [reproduction and installation notes](docs/REPRODUCTION.md) before applying an overlay. Build outputs stay local and are ignored by Git.

## Validation

| Check | Local result |
|---|---|
| Original Wine registry functions, extended reproduction | 81 checks, 27 failures against the expected behavior |
| Executable registry wrappers, including direct export-table lookup | 81 checks, 0 failures |
| File-system device-object selection | 9 checks, 0 failures |
| ICU function resolution and real Unicode conversion | 7 checks, 0 failures |
| Diagnostic WFP provider's failure contract | 18 checks, 0 failures |
| Game startup | First-run language selection reached |
| Online match / long-running session | Not tested |

The registry expectations are based on the documented/researched Native API behavior, including [NtQueryValueKey](https://ntdoc.m417z.com/ntqueryvaluekey) and [NtSetValueKey](https://ntdoc.m417z.com/ntsetvaluekey). These test cases have **not** been cross-run on a native Windows installation in this project.

The candidate upstream diffs were prepared against Wine commit `97027f879ba9a36dbe594720d4710818536e5433`. A full Wine build incorporating those diffs has not been tested; executed validation used the separate PE overlay and test programs described here.

## Important limitations

- The `fwpkclnt` provider is diagnostic only. Its tests demonstrate predictable failure returns, not WFP functionality.
- Wine's remaining kernel callbacks and driver behavior are incomplete. A successful game SDK initialization message does not establish full anti-cheat compatibility.
- The tested startup traces did not call the diagnostic WFP exports before initialization completed. This does not establish behavior during online play.
- Wine updates can change export locations, import layout, object behavior, and compatibility. The overlay tool validates structural assumptions and records input/output hashes; only the configuration above has been tested.

## Upstream references

- [Wine source](https://github.com/wine-mirror/wine)
- [GE-Proton ACE loader discussion](https://github.com/GloriousEggroll/proton-ge-custom/issues/847) — related work on a different ACE title, not proof of Delta Force compatibility.
- [Kernel-mode FwpmEngineOpen0](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fwpmk/nf-fwpmk-fwpmengineopen0)
- [FILE_OBJECT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_file_object)

## License

Original tools, diagnostic providers, and test code are MIT licensed. The Wine source diffs contain LGPL-2.1-or-later Wine context and are provided under that license; see [NOTICE](NOTICE) and [the included Wine license](LICENSES/Wine-LGPL-2.1.txt). This project does not grant rights to redistribute the game or its anti-cheat binaries.

## 繁體中文

這是 Garena《三角洲行動》的 Linux／Wine 相容性實驗原始碼。目前本機已進入首次語言設定畫面；**尚未驗證線上對戰與長時間穩定性**。

內容包含登錄查詢／寫入的尾端 NUL 相容性修正、檔案系統裝置物件選擇、Wine 執行環境修補工具、ICU 相依程式庫安裝工具，以及回傳「不支援」的 WFP 診斷模組。WFP 診斷模組沒有實作封包過濾。

請使用獨立的測試瓶子與 runner 副本；建置和重現方式見上方文件。本專案不包含遊戲檔案、帳號資料或原始日誌。
