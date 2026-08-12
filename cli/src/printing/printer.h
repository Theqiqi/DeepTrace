#pragma once
// printing layer: pure formatting of results into ASCII text.
// Depends only on deeptrace public types (for structure) and the standard library.

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {

struct ProcessInfo;
struct MemoryRegion;
struct ModuleInfo;
struct ExportInfo;
struct ThreadInfo;
struct RegisterInfo;
struct Instruction;
struct WatchEntry;
struct DebugStatus;
struct BreakpointInfo;
struct InjectInfo;
struct ScriptInfo;

}  // namespace deeptrace

namespace deeptrace_cli {

// One `mem batch read` result row (v2.9.0).
struct BatchRow {
    std::string name;
    uintptr_t address = 0;
    std::string value;
};

namespace printer {

void print_error(const std::string& msg);             // "Error: <msg>" -> stderr
void print_message(const std::string& msg);           // "<msg>" -> stdout
void print_help(const std::string& text);
void print_version();

// Wide string -> ASCII printable subset ('?' for the rest).
std::string to_ascii(const std::wstring& s);

void print_processes(const std::vector<deeptrace::ProcessInfo>& list);
void print_process_info(const deeptrace::ProcessInfo& p);
void print_regions(const std::vector<deeptrace::MemoryRegion>& list);
void print_modules(const std::vector<deeptrace::ModuleInfo>& list);
void print_module(const deeptrace::ModuleInfo& m);
void print_exports(const std::vector<deeptrace::ExportInfo>& list);
void print_threads(const std::vector<deeptrace::ThreadInfo>& list);
void print_registers(const std::vector<deeptrace::RegisterInfo>& list);
void print_instructions(const std::vector<deeptrace::Instruction>& list);
void print_watches(const std::vector<deeptrace::WatchEntry>& list);
void print_hex_dump(uintptr_t base, const std::vector<uint8_t>& bytes);
void print_bytes(const std::vector<uint8_t>& bytes);  // single-line hex, e.g. "48 8B C3"
void print_bytes_formatted(const std::vector<uint8_t>& bytes, const std::string& format);
void print_status(const deeptrace::DebugStatus& st);
void print_breakpoint(const deeptrace::BreakpointInfo& bp);
void print_injects(const std::vector<deeptrace::InjectInfo>& list);
void print_inject(const deeptrace::InjectInfo& info);
void print_script_status(const std::vector<deeptrace::ScriptInfo>& list);
void print_batch_read(const std::vector<BatchRow>& rows);  // NAME ADDRESS VALUE table

std::string format_address(uintptr_t a);  // "0x%016llX"

}  // namespace printer
}  // namespace deeptrace_cli
