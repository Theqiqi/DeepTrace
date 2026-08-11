#include "interface/cmd.h"

#include "interface/script.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

constexpr uint32_t kExceptionBreakpoint = 0x80000003;  // EXCEPTION_BREAKPOINT

// Session state, local to one invocation (one invocation = one debug session).
struct SessionState {
    std::vector<uintptr_t> sw_addrs;  // software breakpoints set this session
    std::vector<uintptr_t> hw_addrs;  // hardware breakpoints set this session
    struct GuardRange {
        uintptr_t addr;
        size_t size;
    };
    std::vector<GuardRange> guards;  // page guards set this session
};

const std::string& field(const script::Step& s, const char* name) {
    static const std::string kEmpty;
    auto it = s.fields.find(name);
    return it == s.fields.end() ? kEmpty : it->second;
}

std::string step_header(size_t index, const script::Step& s) {
    std::string head = "[" + std::to_string(index) + "] " + s.op;
    for (const auto& kv : s.fields) {
        head += " ";
        head += kv.first;
        head += "=";
        head += kv.second;
    }
    return head;
}

// Clear everything this session armed, then leave debug mode.
// Warnings do not change the exit code.
void cleanup_session(SessionState& st) {
    for (auto it = st.guards.rbegin(); it != st.guards.rend(); ++it) {
        deeptrace::Result r = deeptrace::guard_clear(it->addr, it->size);
        if (r != deeptrace::Result::Ok) {
            printer::print_error("warning: unguard failed at " +
                                 printer::format_address(it->addr));
        }
    }
    for (auto it = st.hw_addrs.rbegin(); it != st.hw_addrs.rend(); ++it) {
        deeptrace::Result r = deeptrace::hw_breakpoint_clear(*it);
        if (r != deeptrace::Result::Ok) {
            printer::print_error("warning: hclear failed at " +
                                 printer::format_address(*it));
        }
    }
    for (auto it = st.sw_addrs.rbegin(); it != st.sw_addrs.rend(); ++it) {
        deeptrace::Result r = deeptrace::breakpoint_clear(*it);
        if (r != deeptrace::Result::Ok) {
            printer::print_error("warning: clear failed at " +
                                 printer::format_address(*it));
        }
    }
    deeptrace::Result r = deeptrace::debug_detach();
    if (r != deeptrace::Result::Ok) {
        printer::print_error("warning: debug detach failed");
    }
}

// Run one step. Returns 0 on success, 1 on step failure (already printed),
// 2 on process exit (script must stop; already printed).
int run_step(size_t index, const script::Step& s, SessionState& st) {
    using deeptrace::Result;
    const std::string& op = s.op;
    printer::print_message(step_header(index, s));

    auto fail = [&](Result r) -> int {
        printer::print_error("step " + std::to_string(index) + " (" + op + "): " +
                             deeptrace::result_message(r));
        return 1;
    };

    if (op == "break") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        deeptrace::BreakpointInfo bp;
        Result r = deeptrace::breakpoint_set(addr, bp);
        if (r != Result::Ok) return fail(r);
        st.sw_addrs.push_back(addr);
        printer::print_breakpoint(bp);
        return 0;
    }
    if (op == "clear") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        Result r = deeptrace::breakpoint_clear(addr);
        if (r != Result::Ok) return fail(r);
        st.sw_addrs.erase(std::remove(st.sw_addrs.begin(), st.sw_addrs.end(), addr),
                          st.sw_addrs.end());
        printer::print_message("OK");
        return 0;
    }
    if (op == "hbreak") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        uint32_t type = internal::to_u32(field(s, "type"));
        uint32_t len = internal::to_u32(field(s, "length"));
        Result r = deeptrace::hw_breakpoint_set(addr, type, len);
        if (r != Result::Ok) return fail(r);
        st.hw_addrs.push_back(addr);
        printer::print_message("OK");
        return 0;
    }
    if (op == "hclear") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        Result r = deeptrace::hw_breakpoint_clear(addr);
        if (r != Result::Ok) return fail(r);
        st.hw_addrs.erase(std::remove(st.hw_addrs.begin(), st.hw_addrs.end(), addr),
                          st.hw_addrs.end());
        printer::print_message("OK");
        return 0;
    }
    if (op == "guard") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        size_t size = static_cast<size_t>(internal::to_u64(field(s, "size")));
        Result r = deeptrace::guard_set(addr, size);
        if (r != Result::Ok) return fail(r);
        st.guards.push_back({addr, size});
        printer::print_message("OK");
        return 0;
    }
    if (op == "unguard") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        size_t size = static_cast<size_t>(internal::to_u64(field(s, "size")));
        Result r = deeptrace::guard_clear(addr, size);
        if (r != Result::Ok) return fail(r);
        st.guards.erase(
            std::remove_if(st.guards.begin(), st.guards.end(),
                           [&](const SessionState::GuardRange& g) {
                               return g.addr == addr && g.size == size;
                           }),
            st.guards.end());
        printer::print_message("OK");
        return 0;
    }
    if (op == "pause") {
        Result r = deeptrace::debug_pause();
        if (r != Result::Ok) return fail(r);
        printer::print_message("OK");
        return 0;
    }
    if (op == "resume") {
        Result r = deeptrace::debug_resume();
        if (r != Result::Ok) return fail(r);
        printer::print_message("OK");
        return 0;
    }
    if (op == "step" || op == "next") {
        uint32_t tid = internal::to_u32(field(s, "tid"));
        uintptr_t rip = 0;
        Result r = (op == "step") ? deeptrace::debug_step(tid, &rip)
                                  : deeptrace::debug_step_over(tid, &rip);
        if (r != Result::Ok) return fail(r);
        printer::print_message("rip = " + printer::format_address(rip));
        return 0;
    }
    if (op == "registers") {
        uint32_t tid = internal::to_u32(field(s, "tid"));
        std::vector<deeptrace::RegisterInfo> regs;
        Result r = deeptrace::registers_get(regs, tid);
        if (r != Result::Ok) return fail(r);
        printer::print_registers(regs);
        return 0;
    }
    if (op == "register") {
        std::string name = field(s, "name");
        uint32_t tid = internal::to_u32(field(s, "tid"));
        uint64_t value = 0;
        Result r = deeptrace::register_get(name, &value, tid);
        if (r != Result::Ok) return fail(r);
        printer::print_message(name + " = " + printer::format_address(value));
        return 0;
    }
    if (op == "status") {
        deeptrace::DebugStatus stt;
        Result r = deeptrace::debug_status(stt);
        if (r != Result::Ok) return fail(r);
        printer::print_status(stt);
        return 0;
    }
    if (op == "continue") {
        uint32_t timeout = internal::to_u32(field(s, "timeout_ms"));
        deeptrace::ContinueInfo info;
        Result r = deeptrace::debug_continue(timeout, info);
        if (r == Result::Timeout) {
            printer::print_message("continue timeout (" + std::to_string(timeout) +
                                   " ms)");
            return 0;
        }
        if (r != Result::Ok) return fail(r);
        if (info.exited) {
            printer::print_message("process exited (code " +
                                   std::to_string(info.exit_code) + ")");
            return 2;  // stop the script
        }
        if (info.hit) {
            if (info.exception == kExceptionBreakpoint) {
                printer::print_message("breakpoint hit at " +
                                       printer::format_address(info.address) +
                                       " (rip = " + printer::format_address(info.rip) +
                                       ")");
            } else {
                char buf[32];
                std::snprintf(buf, sizeof buf, "0x%08X", info.exception);
                printer::print_message(std::string("exception ") + buf + " at " +
                                       printer::format_address(info.address));
            }
            return 0;
        }
        printer::print_message("continue ok");
        return 0;
    }
    if (op == "read") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        size_t size = static_cast<size_t>(internal::to_u64(field(s, "size")));
        const std::string& format = field(s, "format");
        std::vector<uint8_t> buf(size);
        size_t got = 0;
        Result r = deeptrace::memory_read(addr, buf.data(), size, &got);
        if (r != Result::Ok) return fail(r);
        buf.resize(got);
        if (format == "text") {
            std::string text;
            for (uint8_t c : buf) {
                text.push_back((c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.');
            }
            printer::print_message(text);
        } else {
            printer::print_hex_dump(addr, buf);
        }
        return 0;
    }
    if (op == "write") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        std::vector<uint8_t> bytes = internal::hex_bytes(field(s, "bytes"));
        size_t written = 0;
        Result r = deeptrace::memory_write(addr, bytes.data(), bytes.size(), &written);
        if (r != Result::Ok) return fail(r);
        printer::print_message("wrote " + std::to_string(written) + " bytes");
        return 0;
    }
    if (op == "disasm") {
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        uint32_t count = internal::to_u32(field(s, "count"));
        std::vector<deeptrace::Instruction> insns;
        Result r = deeptrace::disasm_at(addr, count, insns);
        if (r != Result::Ok) return fail(r);
        printer::print_instructions(insns);
        return 0;
    }
    if (op == "watch_add") {
        std::string desc = field(s, "desc");
        uintptr_t addr = internal::to_addr(field(s, "addr"));
        deeptrace::ValueType type =
            static_cast<deeptrace::ValueType>(internal::value_type_id(field(s, "type")));
        Result r = deeptrace::watch_add(desc, addr, type);
        if (r != Result::Ok) return fail(r);
        printer::print_message("OK");
        return 0;
    }
    if (op == "watch_remove") {
        uint32_t index = internal::to_u32(field(s, "index"));
        Result r = deeptrace::watch_remove(index);
        if (r != Result::Ok) return fail(r);
        printer::print_message("OK");
        return 0;
    }
    if (op == "watch_clear") {
        Result r = deeptrace::watch_clear();
        if (r != Result::Ok) return fail(r);
        printer::print_message("OK");
        return 0;
    }
    if (op == "watch_refresh") {
        std::vector<deeptrace::WatchEntry> entries;
        Result r = deeptrace::watch_refresh(entries);
        if (r != Result::Ok) return fail(r);
        printer::print_watches(entries);
        return 0;
    }
    if (op == "watch_list") {
        std::vector<deeptrace::WatchEntry> entries;
        Result r = deeptrace::watch_list(entries);
        if (r != Result::Ok) return fail(r);
        printer::print_watches(entries);
        return 0;
    }
    return fail(Result::InvalidArg);  // defensive: script validation blocks this
}

}  // namespace

int cmd_debug_run(const CommandRequest& req) {
    if (req.args.empty()) {
        printer::print_error("missing script path");
        return 2;
    }
    const std::string& path = req.args[0];

    std::vector<script::Step> steps;
    std::string err;
    if (!script::parse_file(path, steps, err)) {
        printer::print_error(err);
        return 2;
    }

    deeptrace::Result r = deeptrace::debug_attach();
    if (r != deeptrace::Result::Ok) {
        return internal::report_error(r, "debug attach");
    }

    SessionState st;
    int rc = 0;
    for (size_t i = 0; i < steps.size(); ++i) {
        int step_rc = run_step(i + 1, steps[i], st);
        if (step_rc == 2) {
            rc = 1;  // process exited: stop the script
            break;
        }
        if (step_rc != 0) {
            rc = 1;
            break;
        }
    }

    cleanup_session(st);  // success and failure paths both clean up
    return rc;
}

}  // namespace deeptrace_cli
