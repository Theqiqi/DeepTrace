#include "service/disasm.h"
#include "service/session.h"
#include "infrastructure/disassembly/disasm.h"
#include "infrastructure/memory/memory.h"

namespace pmem {

namespace {

Result disasm_bytes(uintptr_t addr, size_t size, uint32_t count,
                    std::vector<Instruction>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    std::vector<uint8_t> buf(size);
    Result err;
    size_t got = internal::ReadRemoteMemory(s.handle, addr, buf.data(), size, &err);
    if (err != Result::Ok || got == 0) return Result::ReadFault;

    uint32_t decoded = 0;
    size_t off = 0;
    while (off < got && decoded < count) {
        internal::DecodedInsn insn;
        uint64_t cur = addr + off;
        if (!internal::disasm_one(buf.data() + off, got - off, cur, insn)) break;
        if (insn.length == 0) break;
        Instruction inst;
        inst.address = cur;
        inst.text = insn.text;
        inst.bytes.assign(buf.begin() + off, buf.begin() + off + insn.length);
        out.push_back(inst);
        off += insn.length;
        ++decoded;
    }
    return Result::Ok;
}

}  // namespace

Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out) {
    if (count == 0 || count > 10000) return Result::InvalidArg;
    size_t size = static_cast<size_t>(count) * 15;  // x64 max instr length
    return disasm_bytes(addr, size, count, out);
}

Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out) {
    if (end < start) return Result::InvalidArg;
    if (end - start > (64u << 20)) return Result::InvalidArg;
    uint32_t count = static_cast<uint32_t>((end - start) / 1 + 1);
    return disasm_bytes(start, end - start, count, out);
}

}  // namespace pmem
