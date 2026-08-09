#pragma once
// printing layer: pure formatting of results into ASCII text.
// Depends only on pmem public types (for structure) and the standard library.

#include <cstdint>
#include <string>
#include <vector>

namespace pmem {

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

}  // namespace pmem

namespace pmem_cli {
namespace printer {

void print_error(const std::string& msg);             // "Error: <msg>" -> stderr
void print_message(const std::string& msg);           // "<msg>" -> stdout
void print_help(const std::string& text);
void print_version();

// Wide string -> ASCII printable subset ('?' for the rest).
std::string to_ascii(const std::wstring& s);

void print_processes(const std::vector<pmem::ProcessInfo>& list);
void print_process_info(const pmem::ProcessInfo& p);
void print_regions(const std::vector<pmem::MemoryRegion>& list);
void print_modules(const std::vector<pmem::ModuleInfo>& list);
void print_module(const pmem::ModuleInfo& m);
void print_exports(const std::vector<pmem::ExportInfo>& list);
void print_threads(const std::vector<pmem::ThreadInfo>& list);
void print_registers(const std::vector<pmem::RegisterInfo>& list);
void print_instructions(const std::vector<pmem::Instruction>& list);
void print_watches(const std::vector<pmem::WatchEntry>& list);
void print_hex_dump(uintptr_t base, const std::vector<uint8_t>& bytes);
void print_bytes(const std::vector<uint8_t>& bytes);  // single-line hex, e.g. "48 8B C3"
void print_bytes_formatted(const std::vector<uint8_t>& bytes, const std::string& format);
void print_status(const pmem::DebugStatus& st);
void print_breakpoint(const pmem::BreakpointInfo& bp);
void print_injects(const std::vector<pmem::InjectInfo>& list);
void print_inject(const pmem::InjectInfo& info);

std::string format_address(uintptr_t a);  // "0x%016llX"

}  // namespace printer
}  // namespace pmem_cli
