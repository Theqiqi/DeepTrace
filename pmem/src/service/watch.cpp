#include "service/watch.h"
#include "service/session.h"
#include "service/store.h"
#include "algorithm/format.h"
#include "algorithm/hex.h"
#include "infrastructure/memory/memory.h"

#include <sstream>

namespace pmem {

namespace {

std::string watch_path(uint32_t pid) {
    return internal::state_dir(pid) + "\\watch.dat";
}

struct WatchRaw {
    std::string desc;
    uintptr_t address = 0;
    ValueType type = ValueType::Dword;
};

bool load_watch(uint32_t pid, std::vector<WatchRaw>& out) {
    out.clear();
    std::vector<std::string> lines;
    if (!internal::read_lines(watch_path(pid), lines)) return true;  // empty ok
    for (const auto& l : lines) {
        std::istringstream ss(l);
        std::string desc, addr_s, type_s;
        std::getline(ss, desc, '|');
        std::getline(ss, addr_s, '|');
        std::getline(ss, type_s, '|');
        uint64_t addr;
        if (!internal::parse_uint64(addr_s, addr)) continue;
        ValueType t = ValueType::Dword;
        if (!internal::parse_value_type(type_s, t)) continue;
        out.push_back(WatchRaw{desc, static_cast<uintptr_t>(addr), t});
    }
    return true;
}

bool save_watch(uint32_t pid, const std::vector<WatchRaw>& items) {
    std::vector<std::string> lines;
    for (const auto& w : items) {
        std::string type_s;
        switch (w.type) {
            case ValueType::Byte: type_s = "byte"; break;
            case ValueType::Word: type_s = "word"; break;
            case ValueType::Dword: type_s = "dword"; break;
            case ValueType::Qword: type_s = "qword"; break;
            case ValueType::Float: type_s = "float"; break;
            case ValueType::Double: type_s = "double"; break;
        }
        // sanitize desc: no '|'
        std::string d = w.desc;
        for (auto& c : d) if (c == '|') c = ' ';
        lines.push_back(d + "|" + std::to_string(w.address) + "|" + type_s);
    }
    return internal::write_lines(watch_path(pid), lines);
}

// Fill a WatchEntry from a stored raw item, reading the current value from
// the target when a process handle is available. Without a handle the entry
// is listed with an explicit "??" / valid=false (not silently stale).
void fill_entry(void* handle, const WatchRaw& w, WatchEntry& e) {
    e.description = w.desc;
    e.address = w.address;
    e.type = w.type;
    if (!handle) {
        e.valid = false;
        e.value = "??";
        return;
    }
    uint8_t buf[8];
    size_t need = internal::value_type_size(w.type);
    Result err;
    size_t n = internal::ReadRemoteMemory(handle, w.address, buf, need, &err);
    if (err == Result::Ok && n == need &&
        internal::format_value(buf, need, w.type, e.value)) {
        e.valid = true;
    } else {
        e.valid = false;
        e.value = "??";
    }
}

}  // namespace

Result watch_list(std::vector<WatchEntry>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.pid) return Result::NotAttached;
    std::vector<WatchRaw> items;
    load_watch(s.pid, items);
    uint32_t idx = 0;
    for (const auto& w : items) {
        WatchEntry e;
        e.index = idx++;
        fill_entry(s.handle, w, e);
        out.push_back(e);
    }
    return Result::Ok;
}

Result watch_add(const std::string& desc, uintptr_t addr, ValueType type) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    // verify address is readable
    uint8_t probe = 0;
    Result err = Result::Ok;
    size_t n = internal::ReadRemoteMemory(s.handle, addr, &probe, 1, &err);
    if (err != Result::Ok || n != 1) return Result::ReadFault;
    std::vector<WatchRaw> items;
    load_watch(s.pid, items);
    items.push_back(WatchRaw{desc, addr, type});
    save_watch(s.pid, items);
    return Result::Ok;
}

Result watch_remove(uint32_t index) {
    auto& s = internal::session();
    if (!s.pid) return Result::NotAttached;
    std::vector<WatchRaw> items;
    load_watch(s.pid, items);
    if (index >= items.size()) return Result::NotFound;
    items.erase(items.begin() + index);
    save_watch(s.pid, items);
    return Result::Ok;
}

Result watch_refresh(std::vector<WatchEntry>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    std::vector<WatchRaw> items;
    load_watch(s.pid, items);
    uint32_t idx = 0;
    for (const auto& w : items) {
        WatchEntry e;
        e.index = idx++;
        fill_entry(s.handle, w, e);
        out.push_back(e);
    }
    return Result::Ok;
}

Result watch_clear() {
    auto& s = internal::session();
    if (!s.pid) return Result::NotAttached;
    std::vector<WatchRaw> empty;
    save_watch(s.pid, empty);
    return Result::Ok;
}

}  // namespace pmem
