#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>

namespace pmem::internal {

// Parse a value type name string ("byte"/"word"/"dword"/"qword"/"float"/"double").
// Returns false on unknown type.
bool parse_value_type(const std::string& s, ValueType& out);

// Format raw little-endian bytes according to type into pure ASCII text.
// Requires at least the bytes the type needs; otherwise returns false.
bool format_value(const uint8_t* data, size_t len, ValueType type, std::string& out);

// Size in bytes for a value type.
size_t value_type_size(ValueType type);

}  // namespace pmem::internal
