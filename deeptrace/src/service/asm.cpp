#include "service/asm.h"
#include "infrastructure/assembly/asmenc.h"
#include "algorithm/hex.h"

#include <cctype>
#include <map>
#include <sstream>
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

struct Line {
    std::string text;         // trimmed instruction text ("" for label-only line)
    std::string label;        // label defined by this line ("" if none)
    std::string ref_symbol;   // symbol referenced in the operand ("" if none)
    uint64_t addr = 0;        // resolved address of the instruction (after layout)
    std::vector<uint8_t> bytes;
};

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
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
// or labels defined in this stream). Returns "" if none. The mnemonic itself
// is the first token and is not treated as a symbol.
std::string find_ref(const std::string& line,
                     const std::map<std::string, uintptr_t>& symbols,
                     const std::map<std::string, bool>& labels) {
    std::string tok;
    size_t i = 0;
    bool first = true;  // first token = mnemonic, never a symbol ref
    while (i <= line.size()) {
        char c = i < line.size() ? line[i] : '\0';
        if (c && is_ident_char(c)) {
            tok.push_back(c);
        } else {
            if (!tok.empty()) {
                if (!first && (symbols.count(tok) || labels.count(tok))) return tok;
                tok.clear();
                first = false;
            }
        }
        ++i;
    }
    return "";
}

bool is_jump_or_call(const std::string& line) {
    // first token
    size_t i = 0;
    std::string tok;
    while (i < line.size() && is_ident_char(line[i])) tok.push_back(line[i++]);
    return tok == "jmp" || tok == "call";
}

// Encode a near jmp (E9 rel32) or near call (E8 rel32) to the target.
std::vector<uint8_t> encode_jump(uint64_t insn_addr, uint64_t target, bool is_call) {
    int64_t rel = static_cast<int64_t>(target) - (static_cast<int64_t>(insn_addr) + 5);
    std::vector<uint8_t> b;
    b.push_back(is_call ? 0xE8 : 0xE9);
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<uint8_t>((static_cast<uint64_t>(rel) >> (8 * i)) & 0xFF));
    }
    return b;
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

    // Pass 1: parse label definitions and instruction lines. Reject label
    // references on non-jump/call instructions (out of scope).
    std::vector<Line> parsed;
    for (const auto& raw : lines) {
        std::string label = parse_label_def(raw);
        if (!label.empty()) {
            Line l;
            l.label = label;
            parsed.push_back(std::move(l));
            continue;
        }
        std::string ref = find_ref(raw, symbols, labels);
        if (!ref.empty() && !is_jump_or_call(raw)) {
            return Result::BadFormat;  // symbol ref only supported on jmp/call
        }
        Line l;
        l.text = raw;
        l.ref_symbol = ref;
        parsed.push_back(std::move(l));
    }

    // Pass 2: layout. Label references always encode as near jumps (5 bytes),
    // so a single pass is exact. Non-ref instructions use Keystone for length.
    std::map<std::string, uint64_t> label_addrs;
    uint64_t cursor = base_addr;
    for (auto& l : parsed) {
        if (!l.label.empty()) {
            label_addrs[l.label] = cursor;
            continue;
        }
        l.addr = cursor;
        if (!l.ref_symbol.empty()) {
            l.bytes = encode_jump(l.addr, 0, l.text.rfind("call", 0) == 0);
            // ref already validated as jmp/call; rel filled in pass 3
            cursor += l.bytes.size();
            continue;
        }
        if (!internal::asm_one(l.text, l.bytes)) return Result::BadFormat;
        if (l.bytes.empty()) return Result::BadFormat;
        cursor += l.bytes.size();
    }

    // Resolve references: internal labels (defined in this stream) take
    // precedence, then external symbols. Undefined -> BadFormat.
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
        bool is_call = l.text.rfind("call", 0) == 0;
        l.bytes = encode_jump(l.addr, target, is_call);
    }

    // Pass 3: emit.
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
