#pragma once
// interface layer: per-group command handlers. Each handler calls the real
// deeptrace public API for its commands and routes results to the printing layer.

#include "command/request.h"

#include "deeptrace.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace_cli {

// --- per-group handlers (return exit code) ---
int cmd_ps(const CommandRequest& req);
int cmd_mem(const CommandRequest& req);
int cmd_module(const CommandRequest& req);
int cmd_thread(const CommandRequest& req);
int cmd_debug(const CommandRequest& req);
int cmd_debug_run(const CommandRequest& req);  // scripted debug session
int cmd_disasm(const CommandRequest& req);
int cmd_resolve(const CommandRequest& req);
int cmd_watch(const CommandRequest& req);
int cmd_dll(const CommandRequest& req);
int cmd_asm(const CommandRequest& req);
int cmd_shellcode(const CommandRequest& req);
int cmd_convert(const CommandRequest& req);
int cmd_hex2bin(const CommandRequest& req);
int cmd_script(const CommandRequest& req);  // AA-style script engine (run/disable/status)

namespace internal {

// Print "Error: <result_message>(<ctx>)" and return exit code 1.
// Defined in executor.cpp.
int report_error(deeptrace::Result r, const std::string& ctx);

// Conversions for already-validated parameter strings.
uint64_t to_u64(const std::string& s);
uint32_t to_u32(const std::string& s);
int to_int(const std::string& s);
uintptr_t to_addr(const std::string& s);
std::vector<uint8_t> hex_bytes(const std::string& s);

// Convert an already-validated typed value (convert-value + convert-type) to
// bytes. type: byte|word|dword|qword|float|double|string|hex.
// Numeric types use little-endian bytes; string uses ASCII bytes; hex is
// passed through. Returns false only for an unexpected type (the parser has
// already validated the pair, so this is defensive).
bool typed_bytes(const std::string& value, const std::string& type,
                 std::vector<uint8_t>& out);

// Value type name -> deeptrace::ValueType (already validated by the parser).
int value_type_id(const std::string& s);

// ---- file I/O helpers (standard library only) ----
// Read an ASCII text file; returns false on open/read failure.
bool read_text_file(const std::string& path, std::string& out);
// Read a binary file; returns false on open/read failure.
bool read_binary_file(const std::string& path, std::vector<uint8_t>& out);
// Write raw bytes to a file; returns false on open/write failure.
bool write_binary_file(const std::string& path, const std::vector<uint8_t>& bytes);

// Resolve a shellcode source argument into bytes:
//  - valid hex string (0x prefix or even hex digits)  -> hex bytes
//  - existing file ending in .asm/.s (asm_ok=true)     -> read text + asm_assemble
//  - existing file (any other extension)               -> read binary
//  - otherwise                                          -> InvalidArg
// asm_ok controls whether .asm/.s files are accepted (exec yes, alloc no).
deeptrace::Result resolve_source(const std::string& source, bool asm_ok,
                                 std::vector<uint8_t>& out);

// ---- AA-style script parser (interface layer, testable) ----
namespace aa {

// One parsed line of an AA-style script ([ENABLE]/[DISABLE] blocks).
enum class StepKind {
    Alloc,             // alloc(name, size)
    LabelDecl,         // label(name) - explicit label declaration
    LabelDef,          // name: label definition
    RegisterSymbol,    // registersymbol(name)
    UnregisterSymbol,  // unregistersymbol(name)
    CreateThread,      // createThread(name)
    Dealloc,           // dealloc(name)
    Db,                // db <hex bytes>
    Asm,               // bare x64 assembly line
    NopFill,           // nop <count> (CE multi-byte nop)
    HookTarget         // "module"+offset: (module base + offset label)
};

struct Step {
    StepKind kind = StepKind::Asm;
    std::string name;    // symbol name (alloc/label/createthread/dealloc/...)
    size_t size = 0;     // alloc size / nop count
    std::string text;    // asm line / db hex / "module"+offset expression
    std::string module;  // hook target module name ("" otherwise)
    uint64_t offset = 0; // hook target offset (resolved in the executor)
    uintptr_t hook_addr = 0;  // resolved module base + offset (executor fills)
    size_t line = 0;     // 1-based source line
};

// Parse AA script text into [ENABLE] and [DISABLE] step lists (pure text;
// no session required). Returns false and fills err on syntax errors.
bool aa_parse_text(const std::string& text, std::vector<Step>& enable,
                   std::vector<Step>& disable, std::string& err);

}  // namespace aa

}  // namespace internal
}  // namespace deeptrace_cli
