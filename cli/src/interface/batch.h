#pragma once
// batch module: parse and validate a batch-locator JSON file consumed by
// `mem batch read/write` (v2.9.0). Pure standard library; no deeptrace or
// platform types (mirrors interface/script.h conventions).
//
// JSON shape (flat, human/AI friendly):
//   { "version": 1, "process": "Game.exe",
//     "values": {
//       "<name>": { locator..., "type": "...", [count], [value] }
//     } }
// locator root source (exactly one of):
//   "module": "<name>" + "base": "<off>"  -> module_base + base
//   "symbol": "<sym>"   (+ "base": "<off>") -> script_symbol + base
//   "base": "<addr>"                        -> absolute address
// "offsets": ["0x10", ...] -> pointer chain (each level reads a qword).
// "type": byte|word|dword|qword|float|double|string|bytes (+"count" for bytes).

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace_cli {
namespace batch {

// One parsed locator definition (a `values` map entry).
struct OffsetPath {
    std::string name;                  // map key under "values"
    std::string module;                // root source: module name ("" = none)
    std::string symbol;                // root source: script symbol ("" = none)
    uint64_t base = 0;                 // absolute addr / module offset / symbol offset
    std::vector<uint64_t> offsets;     // pointer-chain offsets (empty = single-level)
    std::string type;                  // byte|word|dword|qword|float|double|string|bytes
    uint32_t count = 0;                // bytes type only (>0)
    std::string value;                 // write mode only (required + validated there)
};

struct File {
    std::string process;               // optional; validated against the attached process
    std::vector<OffsetPath> items;
};

// Parse and validate a batch-locator JSON file. write_mode additionally
// requires and validates the per-item `value` field. On failure returns false
// and fills err with a message including line/col (JSON) or item name
// (validation).
bool parse_file(const std::string& path, bool write_mode, File& out, std::string& err);

// Parse and validate batch JSON text (used by tests). Same semantics as
// parse_file.
bool parse_text(const std::string& text, bool write_mode, File& out, std::string& err);

}  // namespace batch
}  // namespace deeptrace_cli
