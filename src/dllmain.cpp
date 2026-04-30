// ScumAllowMods — UE4SS C++ mod that re-enables loading of unsigned PAK mods
// in SCUM (both client and server).
//
// Adapted from Narknon's AllowModsMod for PAYDAY 3:
//   https://github.com/narknon/AllowModsMod
// The AOB pattern was extracted via reverse-engineering the pre-built
// AllowModsMod.dll and verified to produce exactly 2 matches in both
// SCUM.exe (client) and SCUMServer.exe (server). Both games run on
// UE 4.27.2, so the same engine-internal function exists in each.
//
// Mechanics:
//   1. On mod init: SinglePassScanner searches for a UE-engine-internal
//      function that blocks the mounting of unsigned PAKs.
//   2. The pattern matches twice (engine-typical paired prologue);
//      the second match is the real target.
//   3. VirtualProtect → overwrite the first byte with 0xC3 (RET) → the
//      function returns immediately on every call, validation never runs.
//
// ⚠️  END-USER REQUIREMENTS:
//   - BattlEye MUST be disabled (`-nobattleye` in Steam launch options
//     plus stop+disable the BEService Windows service).
//   - Server and clients must mount the same mod-PAKs (hash match) for
//     multiplayer; otherwise the server kicks you on connect.
//   - Connecting to a vanilla server with extra PAKs will get you kicked.
//
// ⚠️  USE AT YOUR OWN RISK — no guarantee against future bans.

#include <windows.h>
#ifdef TEXT
#undef TEXT
#endif
#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <memoryapi.h>
#include <SigScanner/SinglePassSigScanner.hpp>
#include <Unreal/UnrealInitializer.hpp>

namespace RC
{
    /**
     * ScumAllowMods: UE4SS C++ mod class definition.
     */
    class ScumAllowMods : public RC::CppUserModBase {
    public:

        void patch_delegate();

        // constructor
        ScumAllowMods() {
            ModVersion     = STR("0.1.0");
            ModName        = STR("ScumAllowMods");
            ModAuthors     = STR("SKRYPT (adapted from Narknon's AllowModsMod)");
            ModDescription = STR("Allows asset mods to load in SCUM (client + server, UE 4.27.2)");
            Output::send<LogLevel::Warning>(STR("[ScumAllowMods]: Init.\n"));
            patch_delegate();
        }

        // destructor
        ~ScumAllowMods() override {
            // intentionally empty — patch is one-shot, no cleanup required
        }

    }; // class

    void ScumAllowMods::patch_delegate()
    {
        uint8_t* function_address = nullptr;
        int8_t   matches_found    = 0;
        uint8_t* found_address    = nullptr;

        SignatureContainer sig_address = [&]() -> SignatureContainer {
            return {
                // ── AOB pattern (UE 4.27.2 engine-internal function) ──────
                // 39 bytes with 7 wildcards. Extracted from Narknon's
                // AllowModsMod DLL (PAYDAY 3) and verified to produce
                // exactly 2 matches in both SCUM.exe and SCUMServer.exe.
                //
                // Disassembly sketch:
                //   mov [rsp+8], rbx      48 89 5C 24 08
                //   push rdi              57
                //   sub rsp, 0x20         48 83 EC 20
                //   mov rax, gs:[0x58]    65 48 8B 04 25 58 00 00 00  ← TLS access
                //   mov rdi, rcx          48 8B F9
                //   mov reg, [rax+disp]   8B ?? ?? ?? ?? ??           ← TLS slot read
                //   mov ecx, imm16        B9 ?? ?? 00 00              ← const load
                //   mov r8, [rax+rdx*8]   4C 8B 04 D0                 ← TLS array index
                { { "48 89 5C 24 08 57 48 83 EC 20 65 48 8B 04 25 58 00 00 00 48 8B F9 8B ?? ?? ?? ?? ?? B9 ?? ?? 00 00 4C 8B 04 D0" } },

                // ── on-match lambda: 2-match pattern, take the second hit ──
                [&](SignatureContainer& self) {
                    if (static_cast<uint8_t*>(self.get_match_address()) > function_address)
                    {
                        function_address = self.get_match_address();
                    }
                    ++matches_found;
                    if (matches_found == 2)
                    {
                        found_address = function_address;
                        self.get_did_succeed() = true;
                        return true;
                    }
                    return false;
                },

                // ── on-finished lambda: patch first byte to 0xC3 (RET) ─────
                [&](const SignatureContainer& self) {
                    if (self.get_did_succeed())
                    {
                        DWORD old;
                        VirtualProtect(found_address, 0x1, PAGE_EXECUTE_READWRITE, &old);
                        found_address[0] = 0xC3;
                        VirtualProtect(found_address, 0x1, old, &old);
                        Output::send<LogLevel::Warning>(
                            STR("[ScumAllowMods]: Delegate found and patched.\n"));
                    }
                    if (!self.get_did_succeed())
                    {
                        Output::send<LogLevel::Warning>(
                            STR("[ScumAllowMods]: Delegate not found. Unable to patch.\n"));
                    }
                }
            };
        }();

        std::vector<SignatureContainer> signature_containers_core{};
        signature_containers_core.emplace_back(sig_address);

        SinglePassScanner::SignatureContainerMap container_map{};
        container_map.emplace(ScanTarget::Core, signature_containers_core);

        // Single-threaded scan — same as upstream AllowModsMod.
        // Multi-threaded sig-scan can produce false negatives on small
        // patterns when the match straddles a thread boundary.
        uint32_t old_threads = SinglePassScanner::m_num_threads;
        SinglePassScanner::m_num_threads = 1;
        SinglePassScanner::start_scan(container_map);
        SinglePassScanner::m_num_threads = old_threads;
    }
}

/**
 * Export start_mod() / uninstall_mod() — UE4SS standard symbol convention.
 */
#define MOD_EXPORT __declspec(dllexport)
extern "C" {
    MOD_EXPORT RC::CppUserModBase* start_mod()                       { return new RC::ScumAllowMods(); }
    MOD_EXPORT void                uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
