#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace::internal {

// Simple line-based state file store (ASCII, '|' separated).
bool read_lines(const std::string& path, std::vector<std::string>& out);
bool write_lines(const std::string& path, const std::vector<std::string>& lines);
bool file_exists(const std::string& path);
bool remove_file(const std::string& path);

// Software breakpoint record.
struct SwBreakRecord {
    uintptr_t address = 0;
    uint8_t original = 0;
};
// Hardware breakpoint record.
struct HwBreakRecord {
    uintptr_t address = 0;
    int index = 0;  // DR0-DR3 slot
};

std::vector<SwBreakRecord> load_sw_breaks(uint32_t pid);
bool save_sw_breaks(uint32_t pid, const std::vector<SwBreakRecord>& recs);
std::vector<HwBreakRecord> load_hw_breaks(uint32_t pid);
bool save_hw_breaks(uint32_t pid, const std::vector<HwBreakRecord>& recs);

// Inject records: kind=dll|shellcode.
struct InjectRecord {
    std::string kind;       // "dll" | "shellcode"
    std::string path;       // dll path (ASCII-safe) or hex bytes for shellcode
    uintptr_t address = 0;
    uint32_t thread_id = 0;
};
std::vector<InjectRecord> load_injects(uint32_t pid);
bool save_injects(uint32_t pid, const std::vector<InjectRecord>& recs);

// Script symbol record: name -> address (script alloc symbols).
struct ScriptSymbolRecord {
    std::string name;
    uintptr_t address = 0;
};
std::vector<ScriptSymbolRecord> load_script_symbols(uint32_t pid);
bool save_script_symbols(uint32_t pid, const std::vector<ScriptSymbolRecord>& recs);

// Hook record: target patched to jump to newmem, original bytes saved.
struct HookRecord {
    uintptr_t target = 0;
    uintptr_t newmem = 0;
    std::vector<uint8_t> orig_bytes;
    size_t size = 0;
};
std::vector<HookRecord> load_hooks(uint32_t pid);
bool save_hooks(uint32_t pid, const std::vector<HookRecord>& recs);

// Script enable records: path -> state ("enabled"). Idempotent.
std::vector<std::string> load_enabled_scripts(uint32_t pid);
bool save_enabled_scripts(uint32_t pid, const std::vector<std::string>& paths);

}  // namespace deeptrace::internal
