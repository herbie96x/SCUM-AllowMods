# scum_allow_mods

A UE4SS C++ mod that re-enables loading of unsigned PAK mods in **SCUM**, after the developers blocked the `-fileopenlog`-based modding workflow.

> ⚠️ **USE AT YOUR OWN RISK** — there is no guarantee against future bans. Read the entire README before installing.

---

## What it does

In late April 2026, the SCUM developers neutralized the `-fileopenlog` launch parameter that the modding community had relied on for asset modding. This blocks the engine's PAK signature validation from being bypassed via the CLI flag.

This mod restores PAK modding by patching a UE 4.27.2 engine-internal validation function directly in the running game process:

1. On mod init, an AOB pattern scan locates the validation function in `SCUM.exe`.
2. The pattern matches twice (engine-typical paired prologue); the second match is the real target.
3. `VirtualProtect` opens the page for writing.
4. The first byte of the function is overwritten with `0xC3` (x86 `RET`).
5. The function returns immediately on every invocation — validation never runs — unsigned PAKs mount without rejection.

Works for **both client and server** — the same validation function exists in both binaries (verified against `SCUM.exe` and `SCUMServer.exe`).

---

## Credits

Adapted from [**Narknon's AllowModsMod**](https://github.com/narknon/AllowModsMod) for PAYDAY 3, which pioneered this approach. The architecture (`CppUserModBase` + `SinglePassSigScanner` + `0xC3` RET-patch) is theirs; we extracted the AOB pattern from the public release DLL and verified it against SCUM.

PAYDAY 3 and SCUM both run on Unreal Engine 4.27.2, which is why the same engine-internal pattern works for both games.

---

## Requirements

- **SCUM** (Steam version) installed and updated
- **UE4SS** (Experimental v3.x or compatible) installed in the SCUM client
- **BattlEye disabled** — the mod cannot run with BattlEye active (BE blocks DLL injection)
- Visual C++ Redistributable 2022 (usually already present)

---

## Install (Client)

### Step 1 — Disable BattlEye

The Steam launch parameter `-nobattleye` alone is **not enough**. You also need to stop the BE Windows service:

PowerShell as Administrator:
```powershell
Stop-Service -Name BEService -Force
Set-Service -Name BEService -StartupType Disabled
```

If the kernel driver `BEDaisy` keeps loading after a reboot, also disable it:
```powershell
sc stop BEDaisy
sc config BEDaisy start= disabled
```

### Step 2 — Install UE4SS

Drop a UE4SS Experimental v3.x build into:
```
<Steam>/steamapps/common/SCUM/SCUM/Binaries/Win64/
```
You should end up with:
```
Binaries/Win64/
├── UE4SS.dll
├── dxgi.dll
├── UE4SS-settings.ini
└── ue4ss/
    └── Mods/
        └── mods.txt
```

### Step 3 — Configure UE4SS

Edit `Binaries/Win64/ue4ss/UE4SS-settings.ini`:

```ini
[General]
bUseUObjectArrayCache = false      ; helps with crash-on-startup

[EngineVersionOverride]
MajorVersion = 4
MinorVersion = 27

[Hooks]
; All hooks OFF — this mod doesn't need any of them
HookProcessInternal                  = 0
HookProcessLocalScriptFunction       = 0
HookInitGameState                    = 0
HookLoadMap                          = 0
HookCallFunctionByNameWithArguments  = 0
HookBeginPlay                        = 0
HookEndPlay                          = 0
HookLocalPlayerExec                  = 0
HookAActorTick                       = 0
HookEngineTick                       = 0
HookGameViewportClientTick           = 0
HookUObjectProcessEvent              = 0
HookProcessConsoleExec               = 0
HookUStructLink                      = 0
```

Why all hooks off: this mod is a self-contained C++ mod that only uses UE4SS's `SinglePassSigScanner` (one-shot AOB scan during init). It needs **no** engine hooks. Many of the hooks above have known crash issues on SCUM.exe — if you turn them on for other Lua mods, do it gradually.

### Step 4 — Install the mod

Drop the mod folder into the UE4SS Mods directory:
```
Binaries/Win64/ue4ss/Mods/scum_allow_mods/
├── dlls/
│   └── main.dll
└── enabled.txt   (empty file)
```

Add to `Binaries/Win64/ue4ss/Mods/mods.txt`:
```
scum_allow_mods : 1
```

### Step 5 — Set Steam launch options

Steam → SCUM → Properties → Launch Options:
```
-nobattleye
```

Do **NOT** add `-fileopenlog` — this mod replaces it.

### Step 6 — Launch SCUM directly

The Steam launch button starts the BattlEye launcher even with `-nobattleye`. Bypass it by launching `SCUM.exe` directly:
```
<Steam>/steamapps/common/SCUM/SCUM/Binaries/Win64/SCUM.exe
```

(You can create a desktop shortcut to this exe, or use a Steam Custom Launcher tool.)

### Step 7 — Verify

Check `<Steam>/steamapps/common/SCUM/SCUM/Binaries/Win64/UE4SS.log`. You should see:
```
[ScumAllowMods]: Init.
[ScumAllowMods]: Delegate found and patched.
```

If you see `Delegate not found. Unable to patch.`, the AOB pattern needs an update — see [Troubleshooting](#troubleshooting).

### Step 8 — Drop in your mods

Place your unsigned `.pak` files into:
```
<Steam>/steamapps/common/SCUM/SCUM/Content/Paks/~mods/
```

Start SCUM. Check `<Steam>/steamapps/common/SCUM/SCUM/Saved/Logs/SCUM.log` for:
```
LogPakFile: Display: Mounting pak file ../../../SCUM/Content/Paks/~mods/YourMod_P.pak.
```

If it mounts, you're done. Connect to a server that has matching mods (same hash) and play.

---

## Install (Server-side)

If you run a SCUM dedicated server with custom unsigned mod-PAKs, you need this mod server-side too — otherwise your server's own engine will refuse to mount them on boot.

Same procedure, but install to:
```
<ServerInstall>/SCUM/Binaries/Win64/ue4ss/Mods/scum_allow_mods/
```

The `-fileopenlog` flag also works server-side currently (proven workflow), but this mod replaces it cleanly. Pick one — either flag, or this mod, not both.

---

## Important caveats

- **Hash-match required for multiplayer**: When you connect to a server, the server compares your mounted PAK list against its own. If the server doesn't have the same mod, you get kicked. Use this on **modded servers** that have matching PAKs, or on your own server.
- **No cheating**: This mod re-enables asset/cosmetic modding. It does not bypass server authority for gameplay (item spawning, position, stats — those are server-state). Don't expect this to enable cheats.
- **Future SCUM updates may break it**: If the developers refactor the validated function or add additional layers, the AOB pattern will need updating. Pull requests welcome.
- **No ban guarantee**: BattlEye is disabled while running this. Theoretically the developers could add server-side detection of modified clients in future updates. Use at your own risk.

---

## Troubleshooting

### `Delegate not found. Unable to patch.` in UE4SS.log

The AOB pattern didn't match in the current `SCUM.exe`. This means a SCUM update changed the validation function's prologue. To fix:
1. Open `SCUM.exe` in IDA / Ghidra or use a sigscanner tool.
2. Find the function with the prologue pattern (see `dllmain.cpp` for the original 24-byte AOB).
3. Update the AOB string in `dllmain.cpp` and rebuild.

### Game crashes with `EXCEPTION_ACCESS_VIOLATION`

Most often this is **not** caused by this mod, but by UE4SS itself. Try:
- Set all `Hook*` settings to `0` in `UE4SS-settings.ini` (this mod doesn't need them).
- Set `bUseUObjectArrayCache = false`.
- Set `[EngineVersionOverride]` explicitly to `4` / `27`.
- Delete `UE4SS_Cache_*.bin` files in the `Win64/` and `Win64/ue4ss/` folders.

If the crash persists, try a different UE4SS version. The mod is built against UE4SS Experimental v3.x main; older v0.4.x might not have a compatible ABI.

### `Failed to find resolution value strings in scalability ini` warnings

Harmless. Always present in SCUM logs regardless of mods.

### Mod loads but my PAK isn't mounted

Check `SCUM.log` (in `<SCUM>/Saved/Logs/`):
- `Found Pak file ... attempting to mount.` — engine sees your PAK.
- `Mounting pak file ...` — actual mount happened. ✅
- Anything mentioning `signature`, `rejected`, `failed`, or `invalid` — the patch isn't taking effect. Re-verify UE4SS.log shows the patch line.

If the PAK file isn't even seen by the engine, double-check the path: `<SCUM>/Content/Paks/~mods/YourMod_P.pak`. The `~mods` folder must exist; the underscore-suffix `_P` on the file name is required by UE4 for mod PAKs.

### BattlEye keeps reactivating

If the BE service starts back up after Windows reboot:
1. Confirm `Set-Service -Name BEService -StartupType Disabled` ran successfully.
2. Check `services.msc` — BE Service status should be "Disabled".
3. Some games re-install the BE service on Steam validation. If you ran "Verify Integrity of Game Files", BE may have been restored. Re-disable it.

---

## Build from source

### Prerequisites

- Visual Studio 2022 with the C++ workload (MSVC 14.40+)
- CMake 3.22+
- The [RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) repository cloned locally with submodules:
  ```
  git clone --recurse-submodules https://github.com/UE4SS-RE/RE-UE4SS.git
  ```

### Build steps

```bash
git clone https://github.com/<your-handle>/scum-allow-mods.git
cd scum-allow-mods

# CMake configure (point to your RE-UE4SS clone if not in default ../../UE4SS/RE-UE4SS-main)
cmake -B build -S . -DUE4SS_REPO_DIR=/path/to/RE-UE4SS

# Build (UE4SS uses custom config names)
cmake --build build --config Game__Shipping__Win64 --target scum_allow_mods
```

Output:
- DLL: `build/Game__Shipping__Win64/scum_allow_mods.dll`
- POST_BUILD copy: `<UE4SS_MODS_DIR>/scum_allow_mods/dlls/main.dll`

You can override the deploy target:
```bash
cmake -B build -S . -DUE4SS_MODS_DIR=/path/to/SCUM/Binaries/Win64/ue4ss/Mods
```

### First build is slow

The first cmake configure pulls all UE4SS dependencies (fmt, imgui, glfw, zydis, polyhook2, etc.). Expect 10-20 minutes and a working internet connection. Subsequent rebuilds are incremental and take seconds.

---

## How to contribute

PRs welcome. The interesting work is keeping the AOB pattern aligned with SCUM updates.

If you find a new pattern after a SCUM patch:
1. Open `SCUM.exe` in IDA/Ghidra.
2. Find the function corresponding to the original (search for the same disassembly shape — `gs:[0x58]` TLS access followed by indexed reads).
3. Extract the first 24 bytes of the prologue (skipping the first 4 if it's too generic to be unique).
4. Update the AOB string in `src/dllmain.cpp`.
5. Verify with two matches in `SCUM.exe` (it should always be exactly two).
6. PR with the new pattern + tested on SCUM build XYZ.

---

## License

MIT — see [LICENSE](LICENSE). Adapted from Narknon's AllowModsMod under similar permissive terms.
