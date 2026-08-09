#include "service/memory.h"
#include "service/session.h"
#include "algorithm/format.h"
#include "infrastructure/memory/memory.h"

namespace deeptrace {

Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read) {
    if (!buf || size == 0) return Result::InvalidArg;
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    Result err;
    size_t n = internal::ReadRemoteMemory(s.handle, addr, buf, size, &err);
    if (out_read) *out_read = n;
    return err;
}

Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written) {
    if (!buf || size == 0) return Result::InvalidArg;
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    Result err;
    size_t n = internal::WriteRemoteMemory(s.handle, addr, buf, size, &err);
    if (out_written) *out_written = n;
    return err;
}

Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out) {
    if (size == 0 || size > (64u << 20)) return Result::InvalidArg;
    out.resize(size);
    Result err;
    size_t n = internal::ReadRemoteMemory(internal::session().handle, addr, out.data(),
                                          size, &err);
    if (err != Result::Ok) return err;
    if (n != size) return Result::ReadFault;
    return Result::Ok;
}

Result memory_regions(std::vector<MemoryRegion>& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::EnumMemoryRegions(s.handle, out);
}

Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    uint8_t buf[8];
    size_t need = internal::value_type_size(type);
    Result err;
    size_t n = internal::ReadRemoteMemory(s.handle, addr, buf, need, &err);
    if (err != Result::Ok) return err;
    if (n != need) return Result::ReadFault;
    if (!internal::format_value(buf, need, type, out_text)) return Result::Error;
    return Result::Ok;
}

}  // namespace deeptrace
