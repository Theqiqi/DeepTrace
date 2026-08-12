#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

std::string trim_str(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::string lower_str(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Strip a // line comment, respecting double-quoted strings.
std::string strip_comment(const std::string& line) {
    bool in_quote = false;
    for (size_t i = 0; i + 1 < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            in_quote = !in_quote;
            continue;
        }
        if (!in_quote && c == '/' && line[i + 1] == '/') {
            return line.substr(0, i);
        }
    }
    return line;
}

bool valid_ident(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

// Unsigned integer: decimal or 0x-hex (used for alloc sizes).
bool parse_u64(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    size_t i = 0;
    int base = 10;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i = 2;
        if (i >= s.size()) return false;
    }
    uint64_t v = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (v > (UINT64_MAX - static_cast<uint64_t>(d)) / static_cast<uint64_t>(base))
            return false;
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    out = v;
    return true;
}

// Hex-only offset (CE convention: "module"+offset uses hex, optional 0x).
bool parse_hex_offset(const std::string& s, uint64_t& out) {
    size_t i = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
    if (i >= s.size()) return false;
    uint64_t v = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (v > (UINT64_MAX - static_cast<uint64_t>(d)) / 16) return false;
        v = v * 16 + static_cast<uint64_t>(d);
    }
    out = v;
    return true;
}

bool is_hex_text(const std::string& s) {
    std::string compact;
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        compact.push_back(c);
    }
    size_t start = 0;
    if (compact.size() >= 2 && compact[0] == '0' && (compact[1] == 'x' || compact[1] == 'X'))
        start = 2;
    size_t n = compact.size() - start;
    if (n == 0 || (n % 2) != 0) return false;
    for (size_t i = start; i < compact.size(); ++i) {
        char c = compact[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

// hex bytes with optional spaces/tabs (db source).
std::vector<uint8_t> spaced_hex(const std::string& s) {
    std::string compact;
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        compact.push_back(c);
    }
    return internal::hex_bytes(compact);
}

}  // namespace

namespace internal {
namespace aa {

namespace {

// ---- line classification ------------------------------------------------

struct ClassifyResult {
    bool ok = true;
    Step step;
    std::string err;  // empty when ok
};

// "module"+offset label: find the quoted module and the offset expression.
bool parse_module_target(const std::string& body, std::string& module,
                         uint64_t& offset, std::string& err) {
    if (body.empty() || body[0] != '"') {
        err = "expected '\"module\"+offset' label";
        return false;
    }
    size_t end = body.find('"', 1);
    if (end == std::string::npos) {
        err = "unterminated module name";
        return false;
    }
    module = body.substr(1, end - 1);
    if (module.empty()) {
        err = "empty module name";
        return false;
    }
    std::string rest = trim_str(body.substr(end + 1));
    if (rest.empty() || rest[0] != '+') {
        err = "expected '+' after module name";
        return false;
    }
    std::string off = trim_str(rest.substr(1));
    if (!parse_hex_offset(off, offset)) {
        err = "invalid module offset: '" + off + "'";
        return false;
    }
    return true;
}

ClassifyResult classify(const std::string& line_in, size_t line_no) {
    ClassifyResult res;
    const std::string line = trim_str(line_in);
    res.step.line = line_no;

    // Keyword call: name(...). Only recognized for known keywords; anything
    // else falls through to label/asm classification.
    size_t paren = line.find('(');
    if (paren != std::string::npos) {
        std::string kw = lower_str(trim_str(line.substr(0, paren)));
        bool known = kw == "alloc" || kw == "label" || kw == "registersymbol" ||
                     kw == "unregistersymbol" || kw == "createthread" ||
                     kw == "dealloc";
        if (known) {
            if (line.back() != ')') {
                res.ok = false;
                res.err = "expected ')'";
                return res;
            }
            std::string argstr = line.substr(paren + 1, line.size() - paren - 2);
            std::vector<std::string> args;
            {
                std::string cur;
                for (char c : argstr) {
                    if (c == ',') {
                        args.push_back(trim_str(cur));
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                args.push_back(trim_str(cur));
            }
            if (kw == "alloc") {
                if (args.size() < 2 || args[0].empty() || args[1].empty()) {
                    res.ok = false;
                    res.err = "alloc requires (name, size)";
                    return res;
                }
                if (!valid_ident(args[0])) {
                    res.ok = false;
                    res.err = "invalid symbol name: '" + args[0] + "'";
                    return res;
                }
                uint64_t sz = 0;
                if (!parse_u64(args[1], sz) || sz == 0) {
                    res.ok = false;
                    res.err = "invalid alloc size: '" + args[1] + "'";
                    return res;
                }
                res.step.kind = StepKind::Alloc;
                res.step.name = args[0];
                res.step.size = static_cast<size_t>(sz);
                // Optional third argument: CE near expression (placement hint).
                // Accepted as "module"+offset or an absolute address; the
                // executor validates it (module must be loaded) but allocation
                // placement remains OS-driven (VirtualAllocEx).
                if (args.size() >= 3 && !args[2].empty()) {
                    const std::string& near_arg = args[2];
                    if (near_arg[0] == '"') {
                        std::string module;
                        uint64_t offset = 0;
                        std::string merr;
                        if (!parse_module_target(near_arg, module, offset, merr)) {
                            res.ok = false;
                            res.err = "invalid alloc near expression: " + merr;
                            return res;
                        }
                        res.step.text = near_arg;
                    } else {
                        uint64_t addr = 0;
                        if (!parse_u64(near_arg, addr) || addr == 0) {
                            res.ok = false;
                            res.err = "invalid alloc near address: '" + near_arg + "'";
                            return res;
                        }
                        res.step.text = near_arg;
                    }
                }
                return res;
            }
            if (args.size() != 1 || args[0].empty() || !valid_ident(args[0])) {
                res.ok = false;
                res.err = "invalid symbol name: '" +
                          (args.empty() ? std::string("") : args[0]) + "'";
                return res;
            }
            res.step.name = args[0];
            if (kw == "label") res.step.kind = StepKind::LabelDecl;
            else if (kw == "registersymbol") res.step.kind = StepKind::RegisterSymbol;
            else if (kw == "unregistersymbol") res.step.kind = StepKind::UnregisterSymbol;
            else if (kw == "createthread") res.step.kind = StepKind::CreateThread;
            else res.step.kind = StepKind::Dealloc;
            return res;
        }
    }

    // db <hex bytes>
    if (line.size() >= 2 && lower_str(line.substr(0, 2)) == "db" &&
        (line.size() == 2 || line[2] == ' ' || line[2] == '\t')) {
        std::string hex = trim_str(line.substr(2));
        if (!is_hex_text(hex)) {
            res.ok = false;
            res.err = "invalid db hex bytes: '" + hex + "'";
            return res;
        }
        res.step.kind = StepKind::Db;
        res.step.text = hex;
        return res;
    }

    // nop <count> (CE multi-byte nop)
    if (line.size() > 3 && lower_str(line.substr(0, 3)) == "nop" &&
        (line[3] == ' ' || line[3] == '\t')) {
        std::string cnt = trim_str(line.substr(3));
        uint64_t n = 0;
        if (parse_u64(cnt, n) && n > 0 && n <= 16) {
            res.step.kind = StepKind::NopFill;
            res.step.size = static_cast<size_t>(n);
            return res;
        }
    }

    // Label definition: <name>: or "module"+offset:
    if (!line.empty() && line.back() == ':') {
        std::string body = trim_str(line.substr(0, line.size() - 1));
        if (body.empty()) {
            res.ok = false;
            res.err = "empty label";
            return res;
        }
        if (body[0] == '"') {
            std::string module;
            uint64_t offset = 0;
            std::string err;
            if (!parse_module_target(body, module, offset, err)) {
                res.ok = false;
                res.err = err;
                return res;
            }
            res.step.kind = StepKind::HookTarget;
            res.step.module = module;
            res.step.offset = offset;
            res.step.text = body;
            return res;
        }
        if (!valid_ident(body)) {
            res.ok = false;
            res.err = "invalid label: '" + body + "'";
            return res;
        }
        res.step.kind = StepKind::LabelDef;
        res.step.name = body;
        res.step.text = body + ":";
        return res;
    }

    // Anything else: bare assembly line.
    res.step.kind = StepKind::Asm;
    res.step.text = line;
    return res;
}

}  // namespace

bool aa_parse_text(const std::string& text, std::vector<Step>& enable,
                   std::vector<Step>& disable, std::string& err) {
    enable.clear();
    disable.clear();

    // Block state: 0 outside, 1 = [ENABLE], 2 = [DISABLE].
    int block = 0;
    bool saw_enable = false;
    bool saw_disable = false;

    std::istringstream ss(text);
    std::string raw;
    size_t line_no = 0;
    while (std::getline(ss, raw)) {
        ++line_no;
        std::string line = trim_str(strip_comment(raw));
        if (line.empty()) continue;

        // Block markers.
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            std::string tag = lower_str(line.substr(1, line.size() - 2));
            if (tag == "enable") {
                if (saw_enable) {
                    err = "script parse error at line " + std::to_string(line_no) +
                          ": duplicate [ENABLE] block";
                    return false;
                }
                saw_enable = true;
                block = 1;
                continue;
            }
            if (tag == "disable") {
                if (saw_disable) {
                    err = "script parse error at line " + std::to_string(line_no) +
                          ": duplicate [DISABLE] block";
                    return false;
                }
                saw_disable = true;
                block = 2;
                continue;
            }
            err = "script parse error at line " + std::to_string(line_no) +
                  ": unknown block '[" + line.substr(1, line.size() - 2) + "]'";
            return false;
        }

        // Any other line starting with '[' is a malformed block marker (e.g.
        // "[ENABLE] garbage"); reject it at parse time instead of letting it
        // fall through to the assembler as a confusing asm line.
        if (!line.empty() && line.front() == '[') {
            err = "script parse error at line " + std::to_string(line_no) +
                  ": unknown block '[" + line.substr(1, line.size() - 1) +
                  "'";
            return false;
        }

        if (block == 0) {
            err = "script parse error at line " + std::to_string(line_no) +
                  ": statement outside [ENABLE]/[DISABLE] block";
            return false;
        }

        ClassifyResult cr = classify(line, line_no);
        if (!cr.ok) {
            err = "script parse error at line " + std::to_string(line_no) + ": " +
                  cr.err;
            return false;
        }

        // Block-scoped keyword rules (clear capability boundary).
        if (block == 1) {
            if (cr.step.kind == StepKind::UnregisterSymbol) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": unregistersymbol is only allowed in [DISABLE]";
                return false;
            }
            if (cr.step.kind == StepKind::Dealloc) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": dealloc is only allowed in [DISABLE]";
                return false;
            }
            enable.push_back(cr.step);
        } else {
            if (cr.step.kind == StepKind::Alloc) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": alloc is only allowed in [ENABLE]";
                return false;
            }
            if (cr.step.kind == StepKind::RegisterSymbol) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": registersymbol is only allowed in [ENABLE]";
                return false;
            }
            if (cr.step.kind == StepKind::CreateThread) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": createThread is only allowed in [ENABLE]";
                return false;
            }
            if (cr.step.kind == StepKind::LabelDecl || cr.step.kind == StepKind::LabelDef) {
                err = "script parse error at line " + std::to_string(line_no) +
                      ": label is only allowed in [ENABLE]";
                return false;
            }
            disable.push_back(cr.step);
        }
    }

    if (!saw_enable && !saw_disable) {
        err = "script parse error: missing [ENABLE]/[DISABLE] block";
        return false;
    }
    return true;
}

}  // namespace aa
}  // namespace internal

namespace {

using deeptrace::Result;
using internal::aa::Step;
using internal::aa::StepKind;

// ---- executor shared state ----------------------------------------------

struct ScriptContext {
    std::string path;  // script path (owner for records)
    std::map<std::string, uintptr_t> symbols;  // alloc'd symbol -> address
    std::vector<std::string> alloc_order;      // alloc order (reverse rollback)
    std::vector<uintptr_t> hook_targets;       // hooked targets (reverse rollback)
    std::vector<std::string> deferred_threads; // createThread symbols (run after writes)
};

void rollback(ScriptContext& ctx) {
    for (auto it = ctx.hook_targets.rbegin(); it != ctx.hook_targets.rend(); ++it) {
        deeptrace::hook_clear(*it);
    }
    for (auto it = ctx.alloc_order.rbegin(); it != ctx.alloc_order.rend(); ++it) {
        deeptrace::script_free(*it);
    }
}

// ---- [ENABLE] execution -------------------------------------------------

// Resolve hook targets (module base + offset) into hook_addrs (keyed by
// source line) and pre-collect post-hook labels (labels defined after a hook
// target bind at target+5, the address right after the 5-byte jmp patch, so
// earlier blocks can reference them as forward labels). Returns false on
// module resolution failure.
bool resolve_enable(const std::vector<Step>& enable,
                    std::map<size_t, uintptr_t>& hook_addrs,
                    std::map<std::string, uintptr_t>& ext_labels,
                    std::string* out_module) {
    std::set<std::string> alloc_names;
    for (const Step& st : enable) {
        if (st.kind != StepKind::Alloc) continue;
        alloc_names.insert(st.name);
        // Near expression hint: "module"+offset must reference a loaded module.
        if (!st.text.empty() && st.text[0] == '"') {
            std::string module = st.text.substr(1, st.text.find('"', 1) - 1);
            uintptr_t base = 0;
            Result r = deeptrace::resolve_base(module, &base);
            if (r != Result::Ok) {
                if (out_module) *out_module = module;
                return false;
            }
        }
    }
    bool in_hook = false;
    uintptr_t hook_addr = 0;
    for (const Step& st : enable) {
        if (st.kind == StepKind::HookTarget) {
            uintptr_t base = 0;
            Result r = deeptrace::resolve_base(st.module, &base);
            if (r != Result::Ok) {
                if (out_module) *out_module = st.module;
                return false;
            }
            hook_addr = base + st.offset;
            hook_addrs[st.line] = hook_addr;
            in_hook = true;
            continue;
        }
        if (!in_hook) continue;
        if (st.kind == StepKind::LabelDecl || st.kind == StepKind::LabelDef) {
            if (alloc_names.count(st.name) != 0) {
                in_hook = false;  // label switches back to an alloc'd target
            } else {
                ext_labels[st.name] = hook_addr + 5;
            }
            continue;
        }
        if (st.kind == StepKind::Db || st.kind == StepKind::Asm ||
            st.kind == StepKind::NopFill) {
            continue;  // hook block content (nop filler / restore bytes)
        }
        in_hook = false;  // next section boundary
    }
    return true;
}

int exec_enable(const std::vector<Step>& enable, ScriptContext& ctx,
                const std::map<size_t, uintptr_t>& hook_addrs,
                const std::map<std::string, uintptr_t>& ext_labels) {
    // Phase 1: allocate all symbols first (independent of code layout).
    for (const Step& st : enable) {
        if (st.kind != StepKind::Alloc) continue;
        uintptr_t addr = 0;
        Result r = deeptrace::script_alloc(st.name, st.size, ctx.path, &addr);
        if (r != Result::Ok) {
            rollback(ctx);
            return internal::report_error(r, st.name);
        }
        ctx.symbols[st.name] = addr;
        ctx.alloc_order.push_back(st.name);
        char buf[96];
        std::snprintf(buf, sizeof buf, "alloc %s = %s (%llu bytes)", st.name.c_str(),
                      printer::format_address(addr).c_str(),
                      (unsigned long long)st.size);
        printer::print_message(buf);
    }

    // Phase 2: walk code writes and hook patches in order; createThread is
    // deferred so threads never start before their code is written.
    uintptr_t target = 0;
    bool target_set = false;
    std::vector<std::string> asm_lines;
    size_t first_asm_line = 0;
    bool in_hook = false;
    uintptr_t hook_addr = 0;
    bool hook_jmp_done = false;
    size_t hook_target_line = 0;

    auto flush_asm = [&]() -> bool {
        if (asm_lines.empty()) return true;
        if (!target_set) {
            printer::print_error("script error at line " +
                                 std::to_string(first_asm_line) +
                                 ": asm line has no write target");
            return false;
        }
        // External symbols: alloc'd symbols + pre-collected post-hook labels.
        std::map<std::string, uintptr_t> syms = ctx.symbols;
        syms.insert(ext_labels.begin(), ext_labels.end());
        std::string code;
        for (const std::string& l : asm_lines) code += l + "\n";
        std::vector<uint8_t> bytes;
        std::string text;
        Result r = deeptrace::asm_assemble_labels(code, target, syms, bytes, &text);
        if (r != Result::Ok) {
            rollback(ctx);
            internal::report_error(r, "");  // bool lambda: report then return false
            return false;
        }
        size_t written = 0;
        r = deeptrace::memory_write(target, bytes.data(), bytes.size(), &written);
        if (r != Result::Ok || written != bytes.size()) {
            rollback(ctx);
            internal::report_error(r != Result::Ok ? r : Result::WriteFault,
                                   printer::format_address(target));
            return false;
        }
        target += bytes.size();
        asm_lines.clear();
        first_asm_line = 0;
        return true;
    };

    // End a hook block; a hook target that was never followed by a jmp line
    // is an error (silently doing nothing would hide mistakes).
    auto end_hook = [&]() -> bool {
        if (!in_hook) return true;
        if (!hook_jmp_done) {
            printer::print_error("script error at line " +
                                 std::to_string(hook_target_line) +
                                 ": hook target must be followed by 'jmp <label>'");
            rollback(ctx);
            return false;
        }
        in_hook = false;
        return true;
    };

    for (const Step& st : enable) {
        switch (st.kind) {
            case StepKind::Alloc:
                if (!end_hook()) return 1;
                continue;  // done in phase 1
            case StepKind::LabelDecl:
            case StepKind::LabelDef:
                // Post-hook labels (non-alloc'd) were pre-collected; an alloc'd
                // label inside the hook block switches back to its target.
                if (in_hook && ctx.symbols.count(st.name) == 0) continue;
                if (!end_hook()) return 1;
                if (ctx.symbols.count(st.name) != 0) {
                    if (!flush_asm()) return 1;
                    target = ctx.symbols[st.name];
                    target_set = true;
                } else {
                    if (first_asm_line == 0) first_asm_line = st.line;
                    asm_lines.push_back(st.text);  // internal label in asm block
                }
                continue;
            case StepKind::RegisterSymbol:
                if (!end_hook()) return 1;
                continue;  // symbol already bound by alloc
            case StepKind::HookTarget:
                if (!end_hook()) return 1;
                if (!flush_asm()) return 1;
                hook_addr = hook_addrs.at(st.line);
                in_hook = true;
                hook_jmp_done = false;
                hook_target_line = st.line;
                continue;
            case StepKind::Asm:
                if (in_hook) {
                    // First asm line of the hook block must be "jmp <label>".
                    if (hook_jmp_done) {
                        printer::print_error(
                            "script error at line " + std::to_string(st.line) +
                            ": only 'jmp <label>' is supported after a hook target");
                        rollback(ctx);
                        return 1;
                    }
                    hook_jmp_done = true;
                    std::string rest = trim_str(st.text);
                    bool is_jmp = rest.size() >= 3 &&
                                  lower_str(rest.substr(0, 3)) == "jmp" &&
                                  (rest.size() == 3 || rest[3] == ' ' || rest[3] == '\t');
                    if (!is_jmp) {
                        printer::print_error(
                            "script error at line " + std::to_string(st.line) +
                            ": hook target must be followed by 'jmp <label>'");
                        rollback(ctx);
                        return 1;
                    }
                    std::string label = trim_str(rest.substr(3));
                    std::map<std::string, uintptr_t>::const_iterator it =
                        ctx.symbols.find(label);
                    if (it == ctx.symbols.end()) it = ext_labels.find(label);
                    if (it == ctx.symbols.end()) {
                        rollback(ctx);
                        return internal::report_error(Result::BadFormat,
                                                      "undefined label '" + label + "'");
                    }
                    deeptrace::HookInfo info;
                    Result r = deeptrace::hook_set(hook_addr, it->second, ctx.path, info);
                    if (r != Result::Ok) {
                        rollback(ctx);
                        return internal::report_error(r,
                                                      printer::format_address(hook_addr));
                    }
                    ctx.hook_targets.push_back(hook_addr);
                    char buf[128];
                    std::snprintf(buf, sizeof buf, "hook set %s -> %s (5 bytes)",
                                  printer::format_address(hook_addr).c_str(),
                                  printer::format_address(it->second).c_str());
                    printer::print_message(buf);
                    target = hook_addr + 5;
                    target_set = true;
                    continue;
                }
                if (first_asm_line == 0) first_asm_line = st.line;
                asm_lines.push_back(st.text);
                continue;
            case StepKind::NopFill:
                if (in_hook) continue;  // CE filler beyond the 5-byte patch
                if (!flush_asm()) return 1;
                {
                    std::vector<uint8_t> nops(st.size, 0x90);
                    size_t written = 0;
                    Result r = deeptrace::memory_write(target, nops.data(),
                                                       nops.size(), &written);
                    if (r != Result::Ok || written != nops.size()) {
                        rollback(ctx);
                        return internal::report_error(
                            r != Result::Ok ? r : Result::WriteFault,
                            printer::format_address(target));
                    }
                    target += nops.size();
                }
                continue;
            case StepKind::Db:
                if (in_hook) continue;  // restore bytes belong to [DISABLE]
                if (!flush_asm()) return 1;
                {
                    std::vector<uint8_t> bytes = spaced_hex(st.text);
                    if (bytes.empty()) {
                        rollback(ctx);
                        return internal::report_error(Result::InvalidArg, st.text);
                    }
                    size_t written = 0;
                    Result r = deeptrace::memory_write(target, bytes.data(),
                                                       bytes.size(), &written);
                    if (r != Result::Ok || written != bytes.size()) {
                        rollback(ctx);
                        return internal::report_error(
                            r != Result::Ok ? r : Result::WriteFault,
                            printer::format_address(target));
                    }
                    target += bytes.size();
                }
                continue;
            case StepKind::CreateThread:
                if (!end_hook()) return 1;
                if (!flush_asm()) return 1;
                if (ctx.symbols.count(st.name) == 0) {
                    rollback(ctx);
                    return internal::report_error(Result::NotFound, st.name);
                }
                ctx.deferred_threads.push_back(st.name);
                continue;
            case StepKind::UnregisterSymbol:
                continue;  // DISABLE-only; parser rejects in [ENABLE]
            case StepKind::Dealloc:
                continue;  // DISABLE-only; parser rejects in [ENABLE]
        }
    }
    if (!end_hook()) return 1;
    if (!flush_asm()) return 1;

    // Phase 3: trigger deferred threads after all memory is in place.
    for (const std::string& name : ctx.deferred_threads) {
        uint32_t tid = 0;
        Result r = deeptrace::thread_create_at(ctx.symbols[name], &tid);
        if (r != Result::Ok) {
            rollback(ctx);
            return internal::report_error(r, name);
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, "createThread %s: tid %u", name.c_str(), tid);
        printer::print_message(buf);
    }
    return 0;
}

// ---- [DISABLE] execution -------------------------------------------------

int exec_disable(const std::vector<Step>& disable) {
    // Reverse order: hooks restored before their memory is freed.
    for (auto it = disable.rbegin(); it != disable.rend(); ++it) {
        const Step& st = *it;
        switch (st.kind) {
            case StepKind::HookTarget: {
                uintptr_t base = 0;
                Result r = deeptrace::resolve_base(st.module, &base);
                if (r != Result::Ok) return internal::report_error(r, st.module);
                uintptr_t addr = base + st.offset;
                r = deeptrace::hook_clear(addr);
                if (r != Result::Ok) {
                    return internal::report_error(r, printer::format_address(addr));
                }
                printer::print_message("hook restored " + printer::format_address(addr));
                break;
            }
            case StepKind::Dealloc: {
                Result r = deeptrace::script_free(st.name);
                if (r != Result::Ok) return internal::report_error(r, st.name);
                printer::print_message("dealloc " + st.name);
                break;
            }
            default:
                break;  // db/asm/nop after hook: hook_clear restores from record
        }
    }
    return 0;
}

// ---- command entry points ------------------------------------------------

int script_run(const CommandRequest& req) {
    const std::string& path = req.args[0];
    std::string text;
    if (!internal::read_text_file(path, text)) {
        printer::print_error("cannot read file: " + path);
        return 2;
    }
    std::vector<Step> enable, disable;
    std::string err;
    if (!internal::aa::aa_parse_text(text, enable, disable, err)) {
        printer::print_error(err);
        return 2;
    }
    if (enable.empty()) {
        printer::print_error("script has no [ENABLE] block");
        return 2;
    }

    // Idempotency: script already enabled (per PID + path record).
    {
        std::vector<deeptrace::ScriptInfo> list;
        Result r = deeptrace::script_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        for (const auto& s : list) {
            if (s.path == path) {
                printer::print_message("already enabled");
                return 0;
            }
        }
    }

    ScriptContext ctx;
    ctx.path = path;
    std::map<size_t, uintptr_t> hook_addrs;
    std::map<std::string, uintptr_t> ext_labels;
    std::string bad_module;
    if (!resolve_enable(enable, hook_addrs, ext_labels, &bad_module)) {
        return internal::report_error(Result::NotFound, bad_module);
    }
    int rc = exec_enable(enable, ctx, hook_addrs, ext_labels);
    if (rc != 0) return rc;

    Result r = deeptrace::script_enable(path);
    if (r != Result::Ok) {
        rollback(ctx);
        return internal::report_error(r, "");
    }
    printer::print_message("script enabled");
    return 0;
}

int script_disable_cmd(const CommandRequest& req) {
    const std::string& path = req.args[0];
    std::string text;
    if (!internal::read_text_file(path, text)) {
        printer::print_error("cannot read file: " + path);
        return 2;
    }
    std::vector<Step> enable, disable;
    std::string err;
    if (!internal::aa::aa_parse_text(text, enable, disable, err)) {
        printer::print_error(err);
        return 2;
    }
    if (disable.empty()) {
        printer::print_error("script has no [DISABLE] block");
        return 2;
    }

    // Idempotency: script not enabled -> already disabled.
    {
        std::vector<deeptrace::ScriptInfo> list;
        Result r = deeptrace::script_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        bool found = false;
        for (const auto& s : list) {
            if (s.path == path) {
                found = true;
                break;
            }
        }
        if (!found) {
            printer::print_message("already disabled");
            return 0;
        }
    }

    int rc = exec_disable(disable);
    if (rc != 0) return rc;  // record kept: retryable

    Result r = deeptrace::script_disable(path);
    if (r != Result::Ok) return internal::report_error(r, "");
    printer::print_message("OK");
    return 0;
}

int script_status_cmd(const CommandRequest& req) {
    (void)req;
    std::vector<deeptrace::ScriptInfo> list;
    Result r = deeptrace::script_status(list);
    if (r != Result::Ok) return internal::report_error(r, "");
    printer::print_script_status(list);
    return 0;
}

}  // namespace

int cmd_script(const CommandRequest& req) {
    if (req.action == "run") return script_run(req);
    if (req.action == "disable") return script_disable_cmd(req);
    if (req.action == "status") return script_status_cmd(req);
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli
