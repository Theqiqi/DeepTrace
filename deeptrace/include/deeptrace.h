#pragma once
// deeptrace - process memory operation static library.
// Public API header. This is the ONLY header a consumer (CLI) may include.
// All public types use standard C++ types only (no windows.h types).
// Detailed per-function docs (params/returns/error codes) live in
// docs/api/v2.1.0/ (see docs/developers/v2.1.0/ for architecture).

#include "domain/types.h"

#include <string>
#include <vector>

namespace deeptrace {

// ---- utility -------------------------------------------------------------
const char* result_message(Result r);

// ---- process -------------------------------------------------------------
Result enumerate_processes(std::vector<ProcessInfo>& out);
Result attach(uint32_t pid);
Result detach();
Result process_info(uint32_t pid, ProcessInfo& out);
Result suspend_process(uint32_t pid);
Result resume_process(uint32_t pid);
Result terminate_process(uint32_t pid, uint32_t exit_code);
Result session_pid(uint32_t* out_pid);

// ---- memory (requires attached session) ----------------------------------
Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read);
Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written);
Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out);
Result memory_regions(std::vector<MemoryRegion>& out);
Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text);

// ---- module ---------------------------------------------------------------
Result module_list(std::vector<ModuleInfo>& out);
Result module_find(const std::string& name, ModuleInfo& out);
Result module_base(const std::string& name, uintptr_t* out_base);
Result module_exports(const std::string& name, std::vector<ExportInfo>& out);
Result module_dump(const std::string& name, const std::string& output_file, std::string* out_hex);

// ---- thread ----------------------------------------------------------------
Result thread_list(std::vector<ThreadInfo>& out);
Result thread_suspend(uint32_t tid);
Result thread_resume(uint32_t tid);
Result thread_terminate(uint32_t tid, uint32_t exit_code);

// ---- debug -----------------------------------------------------------------
Result debug_attach();
Result debug_detach();
Result debug_pause();
Result debug_resume();
Result debug_step(uint32_t tid, uintptr_t* out_rip);         // tid=0 -> first thread
Result debug_step_over(uint32_t tid, uintptr_t* out_rip);    // tid=0 -> first thread
Result debug_continue(uint32_t timeout_ms, ContinueInfo& out);  // run to breakpoint/exit/timeout
Result breakpoint_set(uintptr_t addr, BreakpointInfo& out);
Result breakpoint_clear(uintptr_t addr);
Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length);
Result hw_breakpoint_clear(uintptr_t addr);
Result guard_set(uintptr_t addr, size_t size);
Result guard_clear(uintptr_t addr, size_t size);
Result debug_status(DebugStatus& out);
Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid);   // tid=0 -> first thread
Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid);

// ---- disassembly ------------------------------------------------------------
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);

// ---- resolve -----------------------------------------------------------------
Result resolve_base(const std::string& name, uintptr_t* out_base);
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);

// ---- watch -------------------------------------------------------------------
Result watch_list(std::vector<WatchEntry>& out);
Result watch_add(const std::string& desc, uintptr_t addr, ValueType type);
Result watch_remove(uint32_t index);
Result watch_refresh(std::vector<WatchEntry>& out);
Result watch_clear();

// ---- dll injection --------------------------------------------------------------
Result dll_inject(const std::string& path, InjectInfo& out);
Result dll_eject(const std::string& path_or_addr);
Result dll_list(std::vector<InjectInfo>& out);
Result dll_status(std::vector<InjectInfo>& out);

// ---- assembly -------------------------------------------------------------------
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out, std::string* out_text);

// ---- shellcode -------------------------------------------------------------------
Result shellcode_inject(const std::vector<uint8_t>& bytes, InjectInfo& out);
Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes, InjectInfo& out);
Result shellcode_alloc(const std::vector<uint8_t>& bytes, InjectInfo& out);  // alloc+write, no execute
Result shellcode_run(uintptr_t addr, InjectInfo& out);                     // trigger at recorded addr (repeatable)
Result shellcode_free(uintptr_t addr);                                     // free recorded addr
Result shellcode_status(std::vector<InjectInfo>& out);

}  // namespace deeptrace
