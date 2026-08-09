#include "algorithm/format.h"

#include <cstdio>
#include <cstring>

namespace deeptrace::internal {

bool parse_value_type(const std::string& s, ValueType& out) {
    if (s == "byte") { out = ValueType::Byte; return true; }
    if (s == "word") { out = ValueType::Word; return true; }
    if (s == "dword") { out = ValueType::Dword; return true; }
    if (s == "qword") { out = ValueType::Qword; return true; }
    if (s == "float") { out = ValueType::Float; return true; }
    if (s == "double") { out = ValueType::Double; return true; }
    return false;
}

size_t value_type_size(ValueType type) {
    switch (type) {
        case ValueType::Byte: return 1;
        case ValueType::Word: return 2;
        case ValueType::Dword: return 4;
        case ValueType::Qword: return 8;
        case ValueType::Float: return 4;
        case ValueType::Double: return 8;
    }
    return 1;
}

bool format_value(const uint8_t* data, size_t len, ValueType type, std::string& out) {
    size_t need = value_type_size(type);
    if (len < need) return false;
    char buf[64];
    switch (type) {
        case ValueType::Byte:
            snprintf(buf, sizeof buf, "0x%02X", data[0]);
            break;
        case ValueType::Word: {
            uint16_t v;
            std::memcpy(&v, data, 2);
            snprintf(buf, sizeof buf, "0x%04X", v);
            break;
        }
        case ValueType::Dword: {
            uint32_t v;
            std::memcpy(&v, data, 4);
            snprintf(buf, sizeof buf, "0x%08X", v);
            break;
        }
        case ValueType::Qword: {
            uint64_t v;
            std::memcpy(&v, data, 8);
            snprintf(buf, sizeof buf, "0x%016llX", (unsigned long long)v);
            break;
        }
        case ValueType::Float: {
            float v;
            std::memcpy(&v, data, 4);
            snprintf(buf, sizeof buf, "%g", static_cast<double>(v));
            break;
        }
        case ValueType::Double: {
            double v;
            std::memcpy(&v, data, 8);
            snprintf(buf, sizeof buf, "%g", v);
            break;
        }
    }
    out = buf;
    return true;
}

}  // namespace deeptrace::internal
