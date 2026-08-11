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
int cmd_disasm(const CommandRequest& req);
int cmd_resolve(const CommandRequest& req);
int cmd_watch(const CommandRequest& req);
int cmd_dll(const CommandRequest& req);
int cmd_asm(const CommandRequest& req);
int cmd_shellcode(const CommandRequest& req);
int cmd_convert(const CommandRequest& req);

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

}  // namespace internal
}  // namespace deeptrace_cli
