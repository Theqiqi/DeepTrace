#include "service/disasm.h"
#include "service/session.h"
#include "infrastructure/disassembly/disasm.h"
#include "infrastructure/memory/memory.h"

namespace deeptrace {

namespace {

// Decode `count` instructions from a buffer starting at base_addr. Pure
// computation: no process I/O (used by both disasm_at for remote memory and
// `disasm file` for local binary files). Stops at the first undecodable byte
// (same truncation semantics as the original remote decoder).
Result decode_buffer(const uint8_t* data, size_t size, uintptr_t base_addr,
                     uint32_t count, std::vector<Instruction>& out) {
    out.clear();
    if ((data == nullptr && size > 0) || count == 0 || count > 10000)
        return Result::InvalidArg;
    uint32_t decoded = 0;
    size_t off = 0;
    while (off < size && decoded < count) {
        internal::DecodedInsn insn;
        uint64_t cur = base_addr + off;
        if (!internal::disasm_one(data + off, size - off, cur, insn)) break;
        if (insn.length == 0) break;
        Instruction inst;
        inst.address = cur;
        inst.text = insn.text;
        inst.bytes.assign(data + off, data + off + insn.length);
        out.push_back(inst);
        off += insn.length;
        ++decoded;
    }
    return Result::Ok;
}

}  // namespace

// Public: disassemble a local byte buffer (base_addr 0 for files). No session
// required. v2.13.0.
Result disasm_buffer(const uint8_t* data, size_t size, uintptr_t base_addr,
                     uint32_t count, std::vector<Instruction>& out) {
    return decode_buffer(data, size, base_addr, count, out);
}

Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out) {
    if (count == 0 || count > 10000) return Result::InvalidArg;
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    size_t size = static_cast<size_t>(count) * 15;  // x64 max instr length
    std::vector<uint8_t> buf(size);
    Result err;
    size_t got = internal::ReadRemoteMemory(s.handle, addr, buf.data(), size, &err);
    if (err != Result::Ok || got == 0) return Result::ReadFault;
    return decode_buffer(buf.data(), got, addr, count, out);
}

Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out) {
    if (end < start) return Result::InvalidArg;
    if (end - start > (64u << 20)) return Result::InvalidArg;
    uint32_t count = static_cast<uint32_t>((end - start) / 1 + 1);
    return disasm_at(start, count, out);
}

}  // namespace deeptrace
