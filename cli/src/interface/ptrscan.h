#pragma once
// ptrscan module: parse and validate the `resolve ptrscan <file.json>` config
// (v2.12.0). Pure standard library + shared json parser; no deeptrace types
// (mirrors interface/batch.h conventions).
//
// JSON shape (flat, human/AI friendly):
//   { "version": 1,
//     "target": "0x7FF62A1B2100",   // value address to reverse-walk (required)
//     "module": "Game.exe",         // anchor module ("" = no anchoring)
//     "max_offset": 2048,           // pointer value within target +/- this
//     "max_level": 5,               // max chain depth
//     "max_results": 10000,         // snapshot output cap
//     "threads": 0,                 // scan threads, 0 = hardware_concurrency
//     "rescan": { "target": "0x.." } // optional: snapshot + rescan filter
//   }

#include <cstdint>
#include <string>

namespace deeptrace_cli {
namespace ptrscan {

struct Config {
    uintptr_t target = 0;          // required: value address
    std::string module;            // anchor module name ("" = none)
    uint32_t max_offset = 2048;    // default
    uint32_t max_level = 5;        // default
    uint32_t max_results = 10000;  // default
    uint32_t threads = 0;          // 0 = hardware_concurrency
    bool has_rescan = false;       // rescan segment present
    uintptr_t rescan_target = 0;   // new target for rescan filtering
};

// Parse and validate ptrscan JSON text/file. On failure returns false and
// fills err with a message including line/col (JSON) or field name
// (validation).
bool parse_text(const std::string& text, Config& out, std::string& err);
bool parse_file(const std::string& path, Config& out, std::string& err);

}  // namespace ptrscan
}  // namespace deeptrace_cli
