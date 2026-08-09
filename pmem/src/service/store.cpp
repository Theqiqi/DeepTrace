#include "service/store.h"
#include "service/session.h"
#include "algorithm/hex.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace pmem::internal {

namespace {
std::string breaks_path(uint32_t pid) { return state_dir(pid) + "\\breaks.dat"; }
std::string injects_path(uint32_t pid) { return state_dir(pid) + "\\injects.dat"; }
}  // namespace

bool read_lines(const std::string& path, std::vector<std::string>& out) {
    out.clear();
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) out.push_back(line);
    }
    return true;
}

bool write_lines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;
    for (const auto& l : lines) f << l << "\n";
    return true;
}

bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

bool remove_file(const std::string& path) {
    return ::remove(path.c_str()) == 0;
}

std::vector<SwBreakRecord> load_sw_breaks(uint32_t pid) {
    std::vector<SwBreakRecord> out;
    std::vector<std::string> lines;
    if (!read_lines(breaks_path(pid), lines)) return out;
    for (const auto& l : lines) {
        // format: SW|<addr>|<orig>
        if (l.rfind("SW|", 0) != 0) continue;
        std::istringstream ss(l.substr(3));
        std::string addr_s, orig_s;
        std::getline(ss, addr_s, '|');
        std::getline(ss, orig_s, '|');
        uint64_t addr;
        uint64_t orig;
        if (!parse_uint64(addr_s, addr) || !parse_uint64(orig_s, orig)) continue;
        SwBreakRecord r;
        r.address = static_cast<uintptr_t>(addr);
        r.original = static_cast<uint8_t>(orig);
        out.push_back(r);
    }
    return out;
}

bool save_sw_breaks(uint32_t pid, const std::vector<SwBreakRecord>& recs) {
    std::vector<std::string> lines;
    for (const auto& r : recs) {
        char buf[80];
        snprintf(buf, sizeof buf, "SW|%llu|%u", (unsigned long long)r.address,
                 (unsigned)r.original);
        lines.emplace_back(buf);
    }
    return write_lines(breaks_path(pid), lines);
}

std::vector<HwBreakRecord> load_hw_breaks(uint32_t pid) {
    std::vector<HwBreakRecord> out;
    std::vector<std::string> lines;
    if (!read_lines(breaks_path(pid), lines)) return out;
    for (const auto& l : lines) {
        if (l.rfind("HW|", 0) != 0) continue;
        std::istringstream ss(l.substr(3));
        std::string addr_s, idx_s;
        std::getline(ss, addr_s, '|');
        std::getline(ss, idx_s, '|');
        uint64_t addr;
        uint64_t idx;
        if (!parse_uint64(addr_s, addr) || !parse_uint64(idx_s, idx)) continue;
        HwBreakRecord r;
        r.address = static_cast<uintptr_t>(addr);
        r.index = static_cast<int>(idx);
        out.push_back(r);
    }
    return out;
}

bool save_hw_breaks(uint32_t pid, const std::vector<HwBreakRecord>& recs) {
    std::vector<std::string> lines;
    // keep existing software records
    auto sw = load_sw_breaks(pid);
    for (const auto& r : sw) {
        char buf[80];
        snprintf(buf, sizeof buf, "SW|%llu|%u", (unsigned long long)r.address,
                 (unsigned)r.original);
        lines.emplace_back(buf);
    }
    for (const auto& r : recs) {
        char buf[80];
        snprintf(buf, sizeof buf, "HW|%llu|%d", (unsigned long long)r.address, r.index);
        lines.emplace_back(buf);
    }
    return write_lines(breaks_path(pid), lines);
}

std::vector<InjectRecord> load_injects(uint32_t pid) {
    std::vector<InjectRecord> out;
    std::vector<std::string> lines;
    if (!read_lines(injects_path(pid), lines)) return out;
    for (const auto& l : lines) {
        // format: <kind>|<path-or-hex>|<addr>|<tid>
        std::istringstream ss(l);
        std::string kind, path, addr_s, tid_s;
        std::getline(ss, kind, '|');
        std::getline(ss, path, '|');
        std::getline(ss, addr_s, '|');
        std::getline(ss, tid_s, '|');
        uint64_t addr, tid;
        if (!parse_uint64(addr_s, addr) || !parse_uint64(tid_s, tid)) continue;
        InjectRecord r;
        r.kind = kind;
        r.path = path;
        r.address = static_cast<uintptr_t>(addr);
        r.thread_id = static_cast<uint32_t>(tid);
        out.push_back(r);
    }
    return out;
}

bool save_injects(uint32_t pid, const std::vector<InjectRecord>& recs) {
    std::vector<std::string> lines;
    for (const auto& r : recs) {
        lines.push_back(r.kind + "|" + r.path + "|" + std::to_string(r.address) + "|" +
                        std::to_string(r.thread_id));
    }
    return write_lines(injects_path(pid), lines);
}

}  // namespace pmem::internal
