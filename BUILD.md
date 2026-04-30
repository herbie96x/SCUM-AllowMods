# Building scum_allow_mods from source

This document is for developers who want to:
- Compile the mod themselves instead of using the pre-built release ZIP
- Update the AOB pattern after a SCUM patch breaks the current one
- Contribute fixes / improvements via PRs

End-users who just want to run the mod should follow the install instructions
in [README.md](README.md) and use the pre-built release ZIP from the
[Releases page](https://github.com/herbie96x/SCUM-AllowMods/releases).

---

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| **Visual Studio 2022** | 17.x with C++ workload | MSVC 14.40+ toolset |
| **CMake** | 3.22 or newer | Often not on PATH after install — use full path |
| **Git** | any recent | Submodules required |
| **Rust toolchain** | stable (1.70+) | Some UE4SS dependencies use it |
| **RE-UE4SS source** | commit `265115c0` (Experimental v3.0.1-946) | See below |

The mod links against the upstream UE4SS framework, so you need the source
of [RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) cloned locally **with
submodules**:

```bash
git clone --recurse-submodules https://github.com/UE4SS-RE/RE-UE4SS.git
```

> ⚠️ **Submodule note:** The `deps/first/Unreal` submodule is the UEPseudo
> repository, which links to Epic's protected GitHub fork. You must have your
> Epic account linked to your GitHub account, or the submodule clone will fail
> with a 404. Set up the link at https://www.unrealengine.com/en-US/ue-on-github
> first if you've never built UE4SS before.

---

## Recommended folder layout

The default `CMakeLists.txt` looks for RE-UE4SS at `../../UE4SS/RE-UE4SS-main/`
relative to the project root. The simplest layout is therefore:

```
<workspace>/
├── SCUM-AllowMods/         ← this repo
│   ├── CMakeLists.txt
│   ├── src/dllmain.cpp
│   └── ...
└── UE4SS/
    └── RE-UE4SS-main/      ← cloned RE-UE4SS source
        ├── CMakeLists.txt
        └── ...
```

If you prefer a different layout, override the path at configure time
(see "Custom paths" below).

---

## Build steps

Open a Developer PowerShell for VS 2022 (or any shell with cmake + cargo
on PATH) and run:

```powershell
# 1) Clone this repo
git clone https://github.com/herbie96x/SCUM-AllowMods.git
cd SCUM-AllowMods

# 2) Configure (first time only — pulls all UE4SS deps, slow)
cmake -B build -S .

# 3) Build (incremental after first time, ~15 s)
cmake --build build --config Game__Shipping__Win64 --target scum_allow_mods
```

**Important:** The build configuration is `Game__Shipping__Win64`, **not**
`Release`. UE4SS uses a custom multi-config layout. Using `--config Release`
will fail with cryptic linker errors.

### Outputs

After a successful build:

| Path | Contents |
|---|---|
| `build/Game__Shipping__Win64/scum_allow_mods.dll` | The compiled mod DLL |
| `<UE4SS_MODS_DIR>/scum_allow_mods/dlls/main.dll` | POST_BUILD auto-copy (renamed to `main.dll` per UE4SS convention) |
| `<UE4SS_MODS_DIR>/scum_allow_mods/enabled.txt` | Empty file, marks the mod as enabled |

By default `<UE4SS_MODS_DIR>` resolves to `../../UE4SS/SKRYPT-UE4SS-Mods/` —
override it as shown below if you want the auto-deploy to land somewhere else
(e.g. directly in your SCUM `Win64/ue4ss/Mods/` folder).

### Custom paths

```powershell
# Point at a non-default RE-UE4SS clone:
cmake -B build -S . -DUE4SS_REPO_DIR="C:/path/to/RE-UE4SS"

# Auto-deploy directly into your SCUM client install:
cmake -B build -S . -DUE4SS_MODS_DIR="C:/Steam/steamapps/common/SCUM/SCUM/Binaries/Win64/ue4ss/Mods"
```

You can pass both at once.

---

## First build is slow

The first `cmake -B build -S .` pulls and compiles all UE4SS dependencies:
fmt, imgui, glfw, zydis, polyhook2, lua, etc. Expect:

- **10–20 minutes** on a typical dev machine
- A working internet connection (FetchContent clones from various Git hosts)
- ~3–5 GB of build artifacts in `build/`

After that, incremental rebuilds of just our mod target take **5–15 seconds**.

If the first configure fails partway with a network error, just re-run the same
`cmake -B build -S .` command — FetchContent picks up where it left off.

---

## Updating the AOB pattern after a SCUM patch

This is the most likely reason you're reading this file. When SCUM ships a
hotfix that re-builds `SCUM.exe`, the linker may shuffle the validation
function and the existing AOB pattern stops matching. The mod will log
`[ScumAllowMods]: Delegate not found. Unable to patch.` in `UE4SS.log`, and
unsigned mod-PAKs stop mounting.

### Workflow to extract a fresh pattern

1. **Open the new `SCUM.exe`** in IDA Pro, Ghidra, or any disassembler that
   handles PE files.

2. **Find the validation function.** It has a distinctive shape:
   - Standard prologue (`mov [rsp+...], rbx; push rdi; sub rsp, 0x20`)
   - **TLS access via `mov rax, gs:[0x58]`** (this is the giveaway)
   - Followed by indexed reads from the TLS slot
   - Returns `bool` indicating signature validity

3. **In SCUM.exe the pattern matches twice** — engine-typical paired prologue
   (one is the actual signature-check, one is a near-identical sibling). The
   mod takes the **second match** and patches it with `0xC3` (RET).

4. **Extract the first 24 bytes of the function prologue** as your new pattern.
   Use `??` for any byte that contains a relocated address (typically inside
   `mov reg, [rip+disp32]` or `call rel32` — the offset bytes shift on every
   build).

5. **Replace the pattern string** in `src/dllmain.cpp` (search for
   `delegate_pattern` or `0x48 0x89 0x5C 0x24`).

6. **Verify before committing:**
   - Pattern must match exactly twice in the new `SCUM.exe`
   - Build the mod, drop it into your SCUM install, launch the game
   - Confirm `[ScumAllowMods]: Delegate found and patched.` in `UE4SS.log`
   - Drop a known-working unsigned PAK into `Content/Paks/~mods/`, confirm
     it mounts in `SCUM.log`

7. **Open a PR** with:
   - The new pattern
   - The SCUM build version it was tested against (Steam → SCUM →
     Properties → Updates → "Updated")
   - Ideally a screenshot of the IDA/Ghidra view that confirms the function
     signature shape

---

## Troubleshooting build issues

### `MSB8013 / Platform mismatch` or `Cannot find Game__Shipping__Win64`

You probably ran `cmake --build build --config Release`. UE4SS uses
`Game__Shipping__Win64` instead. Wipe `build/` and reconfigure:

```powershell
Remove-Item -Recurse -Force build
cmake -B build -S .
cmake --build build --config Game__Shipping__Win64 --target scum_allow_mods
```

### `RE-UE4SS Monorepo NICHT gefunden` at configure time

CMake can't find RE-UE4SS at the default path. Either move it to
`../../UE4SS/RE-UE4SS-main/` relative to this repo, or pass
`-DUE4SS_REPO_DIR=<path>` at configure time.

### `UE4SS-SDK-Header nicht gefunden`

The RE-UE4SS clone is missing submodules. Re-clone with:
```bash
git clone --recurse-submodules https://github.com/UE4SS-RE/RE-UE4SS.git
```

Or, in an existing clone:
```bash
cd RE-UE4SS
git submodule update --init --recursive
```

### Cargo / Rust errors during dependency build

Some UE4SS deps build via cargo. Make sure you have the Rust stable toolchain
installed and `cargo` on your PATH. Re-run the failing cmake build after
fixing.

### Build succeeds but the mod doesn't load in-game

- Verify `main.dll` exists in `<SCUM>/Binaries/Win64/ue4ss/Mods/scum_allow_mods/dlls/`
  (note the renamed filename — UE4SS expects `main.dll`, not `scum_allow_mods.dll`)
- Verify `enabled.txt` exists in the mod folder (empty file is fine)
- Verify UE4SS itself is loaded — `UE4SS.log` should exist and have init lines
- Verify BattlEye is **fully** disabled (Service stopped + `-nobattleye` flag +
  direct SCUM.exe launch, not Steam Play button)

---

## Code structure

The mod is intentionally tiny. The single source file `src/dllmain.cpp`
contains:

- `ScumAllowMods` class deriving from `RC::CppUserModBase` (UE4SS C++ mod base)
- An AOB pattern as a `constexpr const char*`
- `on_unreal_init()` runs the `SinglePassSigScanner` against the pattern,
  picks the second match, calls `VirtualProtect` to make the page writable,
  writes `0xC3` (x86 RET) at the function start, restores page protection.
- That's it — no hooks, no event subscriptions, no Lua exposure.

This is why the mod has zero overhead at runtime: the patch is one-shot at
init, after which the validation function is permanently a no-op for the
process lifetime.

---

## Code of conduct for PRs

- Patch should be the **minimum bytes needed** to disable validation. We do
  not modify any other engine logic.
- Pattern should match **exactly twice** in `SCUM.exe`. Patterns that match
  more often are too generic; patterns that match once may be hitting the
  wrong sibling function.
- Keep the C++ standard at C++20 (UE4SS default).
- English comments only in source files.
- New patterns must be tested on a real SCUM client + verified that an
  unsigned PAK mounts. CI cannot do this — it's manual.

---

## License

MIT — see [LICENSE](LICENSE). Adapted from
[Narknon's AllowModsMod](https://github.com/narknon/AllowModsMod) (PAYDAY 3),
which is the original proof-of-concept for this approach on UE 4.27.
