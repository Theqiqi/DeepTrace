#include "service/asm.h"
#include "infrastructure/assembly/asmenc.h"
#include "algorithm/hex.h"

#include <capstone/capstone.h>

#include <cctype>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace deeptrace {

namespace {

// ---- label-aware assembly ----
//
// Keystone's x86 rel32 fixups are biased (they use the fixup field offset
// instead of the full instruction length), so PC-relative jmp/call to a
// symbol must be encoded ourselves: a near jmp is exactly E9 rel32 (5 bytes)
// and a near call is exactly E8 rel32 (5 bytes). Instructions without symbol
// references are delegated to Keystone unchanged.
//
// v2.5.0: symbol references are no longer limited to jmp/call. Memory
// operands "[sym]" are rewritten to RIP-relative "[rip+disp32]" (fixed length
// for the given operands, so a two-pass layout is exact), and "mov
// <acc>,[sym]" uses self-encoded moffs64 (A1/A3), which works for any 64-bit
// address. Immediate operands "sym" are substituted with the address literal;
// Keystone silently truncates operands that do not fit the chosen encoding
// (verified empirically), so the assembled instruction is re-decoded with
// Capstone and the referenced value must equal the symbol address - otherwise
// BadFormat. Complex memory expressions ("[rsp+sym]", "[sym+4]") and
// immediate references to stream-internal labels (layout-dependent value) are
// rejected explicitly rather than silently mis-encoded.

// How a symbol reference is encoded.
enum class RefKind {
    None,     // no symbol reference in this instruction
    Jump,     // jmp/call to a label or symbol (self-encoded rel32)
    MemRip,   // memory operand [sym]: rewritten to [rip+disp32]
    MemMoffs, // mov accumulator [sym]: self-encoded moffs64 (A1/A3)
    Imm       // immediate sym: substituted with the address literal
};

struct Line {
    std::string text;         // trimmed instruction text ("" for label-only line)
    std::string label;        // label defined by this line ("" if none)
    std::string ref_symbol;   // symbol referenced in the operand ("" if none)
    RefKind ref_kind = RefKind::None;
    size_t ref_pos = 0;       // token position in text (immediate substitution)
    bool moffs_load = false;  // MemMoffs: true = A1 (load), false = A3 (store)
    uint64_t addr = 0;        // resolved address of the instruction (after layout)
    std::vector<uint8_t> bytes;
};

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string trim_str(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::string lower_str(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Split on ';' or newline, trim, drop empties.
std::vector<std::string> split_lines(const std::string& code) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : code) {
        if (c == ';' || c == '\n' || c == '\r') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    for (auto& l : out) {
        size_t b = l.find_first_not_of(" \t");
        if (b != std::string::npos) {
            size_t e = l.find_last_not_of(" \t");
            l = l.substr(b, e - b + 1);
        }
    }
    return out;
}

// If the line is "name:" -> the label ("" otherwise). The whole trimmed line
// must be an identifier followed by ':'.
std::string parse_label_def(const std::string& line) {
    if (line.empty() || line.back() != ':') return "";
    std::string body = line.substr(0, line.size() - 1);
    if (body.empty() || !is_ident_char(body[0])) return "";
    for (char c : body) {
        if (!is_ident_char(c)) return "";
    }
    return body;
}

// Find a symbol reference in an instruction operand: the first whole-word
// token (after the mnemonic) that appears in the symbol set (external symbols
// or labels defined in this stream). Returns the token and its position
// ("" if none). The mnemonic itself is the first token and is not treated as
// a symbol.
bool find_ref_pos(const std::string& line,
                  const std::map<std::string, uintptr_t>& symbols,
                  const std::map<std::string, bool>& labels,
                  std::string& name, size_t& pos) {
    std::string tok;
    size_t tok_start = 0;
    size_t i = 0;
    bool first = true;  // first token = mnemonic, never a symbol ref
    while (i <= line.size()) {
        char c = i < line.size() ? line[i] : '\0';
        if (c && is_ident_char(c)) {
            if (tok.empty()) tok_start = i;
            tok.push_back(c);
        } else {
            if (!tok.empty()) {
                if (!first && (symbols.count(tok) || labels.count(tok))) {
                    name = tok;
                    pos = tok_start;
                    return true;
                }
                tok.clear();
                first = false;
            }
        }
        ++i;
    }
    return false;
}

bool is_jump_or_call(const std::string& line) {
    // first token
    size_t i = 0;
    std::string tok;
    while (i < line.size() && is_ident_char(line[i])) tok.push_back(line[i++]);
    return tok == "jmp" || tok == "call";
}

// Encode a near jmp (E9 rel32) or near call (E8 rel32) to the target. The rel
// computation is shared with hook patching (encode_jmp_rel32); only the
// opcode differs for call.
std::vector<uint8_t> encode_jump(uint64_t insn_addr, uint64_t target, bool is_call) {
    std::vector<uint8_t> b = internal::encode_jmp_rel32(insn_addr, target);
    if (is_call) b[0] = 0xE8;
    return b;
}

// Split a line into its two top-level operands (comma at bracket depth 0).
bool split_operands(const std::string& line, size_t start, std::string& op1,
                    std::string& op2) {
    size_t depth = 0;
    for (size_t i = start; i < line.size(); ++i) {
        char c = line[i];
        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
        } else if (c == ',' && depth == 0) {
            op1 = trim_str(line.substr(start, i - start));
            op2 = trim_str(line.substr(i + 1));
            return true;
        }
    }
    return false;
}

bool is_accumulator(const std::string& s) {
    std::string t = lower_str(s);
    return t == "rax" || t == "eax" || t == "ax";
}

// True when op is exactly "[name]" optionally prefixed with a size marker
// ("qword ptr [name]" / "dword ptr [name]" / "word ptr [name]" / "byte ptr
// [name]").
bool is_bare_mem_operand(const std::string& op, const std::string& name) {
    std::string pat = "[" + name + "]";
    size_t p = op.find(pat);
    if (p == std::string::npos) return false;
    std::string before = trim_str(op.substr(0, p));
    if (!before.empty()) {
        std::string b = lower_str(before);
        if (b != "qword ptr" && b != "dword ptr" && b != "word ptr" &&
            b != "byte ptr") {
            return false;
        }
    }
    if (!trim_str(op.substr(p + pat.size())).empty()) return false;
    return true;
}

// True for "mov [name],<acc>" (store, A3) / "mov <acc>,[name]" (load, A1).
bool is_mov_accumulator(const std::string& line, const std::string& name,
                        bool& load) {
    size_t i = 0;
    std::string mn;
    while (i < line.size() && is_ident_char(line[i])) mn.push_back(line[i++]);
    if (lower_str(mn) != "mov") return false;
    std::string op1, op2;
    if (!split_operands(line, i, op1, op2)) return false;
    if (is_bare_mem_operand(op1, name) && is_accumulator(op2)) {
        load = false;  // mov [name],rax -> store
        return true;
    }
    if (is_bare_mem_operand(op2, name) && is_accumulator(op1)) {
        load = true;  // mov rax,[name] -> load
        return true;
    }
    return false;
}

// Classify a non-jump/call symbol reference. Returns false (unsupported form,
// caller -> BadFormat) for immediate references to stream-internal labels
// (their value depends on layout, which is not resolvable for immediates).
//
// The memory-operand path requires the exact bare pattern "[name]": any other
// placement ("[rsp+name]", "[name+4]", "[ name ]") falls through to the
// immediate path, whose substitution + Capstone verification encodes them
// safely (moffs64 for the accumulator; truncation caught for others).
bool classify_ref(const std::string& line, size_t pos, const std::string& name,
                  bool is_stream_label, RefKind& kind, bool& moffs_load) {
    if (pos > 0 && line[pos - 1] == '[' &&
        pos + name.size() < line.size() && line[pos + name.size()] == ']') {
        if (is_mov_accumulator(line, name, moffs_load)) {
            kind = RefKind::MemMoffs;
        } else {
            kind = RefKind::MemRip;
        }
        return true;
    }
    // Immediate operand (external symbols only; stream labels rejected).
    if (is_stream_label) return false;
    kind = RefKind::Imm;
    return true;
}

// Rewrite "[name]" to "[rip+0x0]" (placeholder; same length as the final
// "[rip+disp32]" form).
std::string rewrite_mem_rip(const std::string& line, const std::string& name) {
    std::string pat = "[" + name + "]";
    size_t p = line.find(pat);
    if (p == std::string::npos) return line;
    std::string out = line;
    out.replace(p, pat.size(), "[rip+0x0]");
    return out;
}

// Fill the placeholder displacement: "[rip+0x0]" -> "[rip+<disp>]".
std::string rewrite_rip_disp(const std::string& placeholder, int64_t disp) {
    const std::string pat = "[rip+0x0]";
    size_t p = placeholder.find(pat);
    if (p == std::string::npos) return placeholder;
    char buf[48];
    if (disp >= 0) {
        snprintf(buf, sizeof buf, "[rip+0x%llX]", (unsigned long long)disp);
    } else {
        // negate without unsigned-unary-minus warning and without overflow
        // at INT64_MIN
        uint64_t abs_disp = static_cast<uint64_t>(-(disp + 1)) + 1;
        snprintf(buf, sizeof buf, "[rip-0x%llX]", (unsigned long long)abs_disp);
    }
    std::string out = placeholder;
    out.replace(p, pat.size(), buf);
    return out;
}

// Substitute the symbol token with its address literal.
std::string subst_imm(const std::string& line, size_t pos,
                      const std::string& name, uint64_t addr) {
    char buf[32];
    snprintf(buf, sizeof buf, "0x%llX", (unsigned long long)addr);
    std::string out = line;
    out.replace(pos, name.size(), buf);
    return out;
}

// Accumulator register of a MemMoffs line (the non-memory operand).
std::string moffs_reg_of(const std::string& line, const std::string& name) {
    size_t i = 0;
    while (i < line.size() && is_ident_char(line[i])) ++i;  // mnemonic
    std::string op1, op2;
    if (!split_operands(line, i, op1, op2)) return "rax";  // defensive
    if (is_bare_mem_operand(op1, name)) return op2;
    return op1;
}

// Self-encode a moffs64 access: A1 (load) / A3 (store) with an 8-byte address.
// The operand-size prefix depends only on the accumulator register (48 REX.W
// for rax, 66 for ax, none for eax), so the length is fixed for layout.
std::vector<uint8_t> encode_moffs(uint64_t addr, bool load,
                                  const std::string& acc_reg) {
    std::vector<uint8_t> b;
    std::string r = lower_str(acc_reg);
    if (r == "rax") {
        b.push_back(0x48);
    } else if (r == "ax") {
        b.push_back(0x66);
    }
    b.push_back(load ? 0xA1 : 0xA3);
    for (int k = 0; k < 8; ++k) {
        b.push_back(static_cast<uint8_t>(addr >> (8 * k)));
    }
    return b;
}

// Verify that the assembled instruction references `expected` as an immediate
// value or as an effective memory address (Capstone detail). Catches
// Keystone's silent truncation of operands that do not fit the chosen
// encoding (e.g. "mov ecx, 0x7FF6...": Keystone truncates to imm32).
//
// Note: follow the disasm module's pattern - use cs_disasm(count=1), NOT
// cs_disasm_iter + a stack cs_insn (crashes in the MSVC debug build).
bool verify_ref(const std::vector<uint8_t>& bytes, uint64_t insn_addr,
                uint64_t expected) {
    csh handle = 0;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return false;
    cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    cs_insn* insns = nullptr;
    size_t n = cs_disasm(handle, bytes.data(), bytes.size(), insn_addr, 1,
                         &insns);
    bool ok = false;
    if (n > 0 && insns[0].detail) {
        const cs_x86& x86 = insns[0].detail->x86;
        for (uint8_t k = 0; k < x86.op_count && !ok; ++k) {
            const cs_x86_op& op = x86.operands[k];
            if (op.type == X86_OP_IMM) {
                if (static_cast<uint64_t>(op.imm) == expected) ok = true;
            } else if (op.type == X86_OP_MEM) {
                uint64_t ea = 0;
                if (op.mem.base == X86_REG_RIP) {
                    ea = insns[0].address + insns[0].size +
                         static_cast<uint64_t>(static_cast<int64_t>(op.mem.disp));
                } else if (op.mem.base == X86_REG_INVALID &&
                           op.mem.index == X86_REG_INVALID) {
                    ea = static_cast<uint64_t>(static_cast<int64_t>(op.mem.disp));
                }
                if (ea == expected) ok = true;
            }
        }
    }
    if (insns) cs_free(insns, n);
    cs_close(&handle);
    return ok;
}

}  // namespace

Result asm_assemble(const std::string& code, std::vector<uint8_t>& out,
                    std::string* out_text) {
    out.clear();
    // Support multiple lines separated by ';' or newline.
    std::stringstream ss(code);
    std::string line;
    bool ok = true;
    while (std::getline(ss, line, ';')) {
        if (line.empty()) continue;
        // trim
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        std::string inst = line.substr(b, e - b + 1);
        std::vector<uint8_t> bytes;
        if (!internal::asm_one(inst, bytes)) {
            ok = false;
            break;
        }
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    if (!ok) return Result::BadFormat;
    if (out_text) {
        *out_text = internal::hex_encode(out.data(), out.size());
    }
    return Result::Ok;
}

Result asm_assemble_labels(const std::string& code, uintptr_t base_addr,
                           const std::map<std::string, uintptr_t>& symbols,
                           std::vector<uint8_t>& out, std::string* out_text) {
    out.clear();
    if (code.empty()) return Result::InvalidArg;

    std::vector<std::string> lines = split_lines(code);
    if (lines.empty()) return Result::InvalidArg;

    // Pass 0: collect label names defined in this stream (they are valid
    // reference targets even though they are not in the external symbols).
    std::map<std::string, bool> labels;
    for (const auto& raw : lines) {
        std::string lbl = parse_label_def(raw);
        if (!lbl.empty()) labels[lbl] = true;
    }

    // Pass 1: parse label definitions and instruction lines; classify symbol
    // references so pass 2 can lay each encoding out at its fixed size.
    std::vector<Line> parsed;
    for (const auto& raw : lines) {
        std::string label = parse_label_def(raw);
        if (!label.empty()) {
            Line l;
            l.label = label;
            parsed.push_back(std::move(l));
            continue;
        }
        Line l;
        l.text = raw;
        std::string ref;
        size_t ref_pos = 0;
        if (find_ref_pos(raw, symbols, labels, ref, ref_pos)) {
            l.ref_symbol = ref;
            l.ref_pos = ref_pos;
            if (is_jump_or_call(raw)) {
                l.ref_kind = RefKind::Jump;
            } else {
                bool moffs_load = false;
                if (!classify_ref(raw, ref_pos, ref, labels.count(ref) != 0,
                                  l.ref_kind, moffs_load)) {
                    return Result::BadFormat;  // complex expr / stream-label imm
                }
                l.moffs_load = moffs_load;
            }
        }
        parsed.push_back(std::move(l));
    }

    // Pass 2: layout. Jump refs always encode as near jumps (5 bytes); MemRip
    // and MemMoffs have operand-determined fixed sizes; Imm refs encode with
    // the (known external) value and are verified against silent truncation.
    std::map<std::string, uint64_t> label_addrs;
    uint64_t cursor = base_addr;
    for (auto& l : parsed) {
        if (!l.label.empty()) {
            label_addrs[l.label] = cursor;
            continue;
        }
        l.addr = cursor;
        switch (l.ref_kind) {
            case RefKind::None:
                if (!internal::asm_one(l.text, l.bytes)) return Result::BadFormat;
                if (l.bytes.empty()) return Result::BadFormat;
                break;
            case RefKind::Jump:
                l.bytes = encode_jump(l.addr, 0, l.text.rfind("call", 0) == 0);
                // rel filled in pass 3
                break;
            case RefKind::MemMoffs:
                l.bytes = encode_moffs(0, l.moffs_load,
                                       moffs_reg_of(l.text, l.ref_symbol));
                // address filled in pass 3
                break;
            case RefKind::MemRip:
                if (!internal::asm_one(rewrite_mem_rip(l.text, l.ref_symbol),
                                       l.bytes)) {
                    return Result::BadFormat;
                }
                if (l.bytes.empty()) return Result::BadFormat;
                break;
            case RefKind::Imm: {
                auto sit = symbols.find(l.ref_symbol);
                if (sit == symbols.end()) return Result::BadFormat;
                std::string sub =
                    subst_imm(l.text, l.ref_pos, l.ref_symbol, sit->second);
                if (!internal::asm_one(sub, l.bytes)) return Result::BadFormat;
                if (l.bytes.empty()) return Result::BadFormat;
                if (!verify_ref(l.bytes, l.addr, sit->second)) {
                    return Result::BadFormat;  // silent truncation detected
                }
                // A value above 0xFFFFFFFF requires a 64-bit immediate
                // (encoding length >= 9). Any shorter encoding means the
                // operand was truncated: an imm32 sign-extension can otherwise
                // reproduce a kernel-range address, which a 32-bit destination
                // would not actually receive.
                if (sit->second > 0xFFFFFFFFull && l.bytes.size() < 9) {
                    return Result::BadFormat;
                }
                break;
            }
        }
        cursor += l.bytes.size();
    }

    // Pass 3: resolve references. Internal labels (defined in this stream)
    // take precedence, then external symbols. Undefined -> BadFormat.
    for (auto& l : parsed) {
        if (l.ref_symbol.empty()) continue;
        uint64_t target = 0;
        auto lit = label_addrs.find(l.ref_symbol);
        if (lit != label_addrs.end()) {
            target = lit->second;
        } else {
            auto sit = symbols.find(l.ref_symbol);
            if (sit == symbols.end()) return Result::BadFormat;
            target = sit->second;
        }
        switch (l.ref_kind) {
            case RefKind::Jump:
                l.bytes = encode_jump(l.addr, target,
                                      l.text.rfind("call", 0) == 0);
                break;
            case RefKind::MemMoffs:
                l.bytes = encode_moffs(target, l.moffs_load,
                                       moffs_reg_of(l.text, l.ref_symbol));
                break;
            case RefKind::MemRip: {
                int64_t disp =
                    static_cast<int64_t>(target) -
                    static_cast<int64_t>(l.addr + l.bytes.size());
                if (disp > INT32_MAX || disp < INT32_MIN) {
                    return Result::BadFormat;  // outside the rip+disp32 range
                }
                std::string sub = rewrite_rip_disp(
                    rewrite_mem_rip(l.text, l.ref_symbol), disp);
                std::vector<uint8_t> final_bytes;
                if (!internal::asm_one(sub, final_bytes)) {
                    return Result::BadFormat;
                }
                // Layout integrity: the final encoding must keep the length
                // that pass 2 laid out.
                if (final_bytes.size() != l.bytes.size()) {
                    return Result::BadFormat;
                }
                l.bytes = std::move(final_bytes);
                break;
            }
            case RefKind::Imm:
                break;  // encoded and verified in pass 2
            case RefKind::None:
                break;
        }
    }

    // Pass 4: emit.
    for (const auto& l : parsed) {
        if (!l.label.empty()) continue;
        out.insert(out.end(), l.bytes.begin(), l.bytes.end());
    }

    if (out_text) {
        *out_text = internal::hex_encode(out.data(), out.size());
    }
    return Result::Ok;
}

}  // namespace deeptrace
