#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace deeptrace::internal {

// Get the register set of a thread (tid=0 -> first thread).
Result GetThreadRegisters(uint32_t pid, uint32_t tid, std::vector<RegisterInfo>& out);

// Get a single register by name ("rax", "rip", "eflags", ...).
Result GetRegisterValue(uint32_t pid, uint32_t tid, const std::string& name, uint64_t* out);

// Read one byte at a remote address (used for software breakpoints).
Result ReadByte(void* hprocess, uintptr_t addr, uint8_t* out);
Result WriteByte(void* hprocess, uintptr_t addr, uint8_t value);

// Debug attach/detach via DebugActiveProcess/Stop.
Result DebugAttachProcess(uint32_t pid);
Result DebugDetachProcess(uint32_t pid);

// Single-step one thread of an attached debuggee: sets the trap flag before
// continuing the pending debug event, then waits for EXCEPTION_SINGLE_STEP.
// tid must be a real thread id (0 -> first thread).
Result DebugSingleStep(uint32_t pid, uint32_t tid, uintptr_t* out_rip);

// Step over: if the current instruction is a near call, place a temporary
// software breakpoint at the return address, run until it hits, and restore.
// Otherwise behaves like DebugSingleStep. Returns the post-step RIP.
Result DebugStepOver(uint32_t pid, uint32_t tid, uintptr_t* out_rip);

// Resume the debuggee and wait for the next debug event within timeout_ms.
// Fills out when an exception occurs or the process exits; the exception event
// is NOT continued (the debuggee stays frozen on it) so callers can decide how
// to handle it. Returns Timeout when no interesting event arrives in time.
// sw_addrs lists the software breakpoints currently armed: EXCEPTION_BREAKPOINT
// events at any other address (e.g. the attach-time breakin break inside ntdll)
// are skipped/continued so the run keeps going until a real breakpoint.
Result DebugWaitEvent(uint32_t pid, uint32_t timeout_ms,
                      const std::vector<uintptr_t>& sw_addrs, ContinueInfo& out);

// Consume a pending software-breakpoint exception: restore the original byte
// at addr, single-step the hitting thread over the instruction (RIP is forced
// back to addr first), then re-arm the INT3. The breakpoint exception must
// still be pending for tid. Fills *out_rip with the post-instruction RIP.
Result DebugConsumeBreakpoint(uint32_t pid, uint32_t tid, uintptr_t addr,
                              uint8_t orig, uintptr_t* out_rip);

}  // namespace deeptrace::internal
