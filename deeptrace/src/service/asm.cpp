#include "service/asm.h"
#include "infrastructure/assembly/asmenc.h"
#include "algorithm/hex.h"

#include <sstream>

namespace deeptrace {

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

}  // namespace deeptrace
