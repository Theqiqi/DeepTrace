#pragma once
// Minimal recursive-descent JSON parser (standard library only), shared by
// batch (v2.9.0) and ptrscan (v2.12.0) config file parsing. Produces a small
// value tree: objects (ordered members), arrays, strings, numbers (kept as
// raw literal text), booleans, null. Supported string escapes: \" and \\.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace deeptrace_cli {
namespace jsn {

struct JVal {
    enum class Kind { Null, Bool, Num, Str, Arr, Obj };
    Kind kind = Kind::Null;
    bool b = false;
    std::string str;  // string content, or raw number literal text
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;
};

// Parse `text` as a single JSON value; on failure returns false and fills err
// with a message including line/col. `prefix` names the error source
// ("batch" / "ptrscan"): "<prefix> parse error at line L col C: ...".
bool parse(const std::string& text, JVal& out, std::string& err,
           const std::string& prefix);

// Object member lookup (nullptr when absent).
const JVal* find_member(const JVal& o, const std::string& key);

// Number-like member: returns raw literal text for Str/Num kinds.
bool member_raw(const JVal* v, std::string& raw);

// Parse an unsigned integer from a hex ("0x..") or decimal literal.
bool parse_uint(const std::string& s, uint64_t& out);

}  // namespace jsn
}  // namespace deeptrace_cli
