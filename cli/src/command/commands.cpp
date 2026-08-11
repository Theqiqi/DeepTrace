#include "command/commands.h"

#include <cstdio>
#include <sstream>

namespace deeptrace_cli {

namespace {

ParamSpec req(const char* name, const char* type) {
    ParamSpec p;
    p.name = name;
    p.type = type;
    p.required = true;
    return p;
}

ParamSpec opt(const char* name, const char* type, const char* def) {
    ParamSpec p;
    p.name = name;
    p.type = type;
    p.required = false;
    p.def = def;
    return p;
}

CommandSpec cmd(const char* group, const char* action, const char* usage,
                const char* brief, std::vector<ParamSpec> params) {
    CommandSpec c;
    c.group = group;
    c.action = action;
    c.usage = usage;
    c.brief = brief;
    c.params = std::move(params);
    return c;
}

}  // namespace

const std::vector<CommandSpec>& command_table() {
    static const std::vector<CommandSpec> kTable = {
        // ---- ps ----
        cmd("ps", "list", "ps list", "List all processes", {}),
        cmd("ps", "attach", "ps attach <pid>", "Attach to a process", {req("pid", "pid")}),
        cmd("ps", "detach", "ps detach", "Detach from current process", {}),
        cmd("ps", "info", "ps info", "Show current process info", {}),
        cmd("ps", "suspend", "ps suspend", "Suspend the attached process", {}),
        cmd("ps", "resume", "ps resume", "Resume the attached process", {}),
        cmd("ps", "kill", "ps kill [exit_code]", "Terminate the attached process",
            {opt("exit_code", "exit-code", "0")}),

        // ---- mem ----
        cmd("mem", "read", "mem read <address> [size] [format]",
            "Read memory (format: hex|dec|bin|ascii)",
            {req("address", "address"), opt("size", "number", "1"),
             opt("format", "format", "hex")}),
        cmd("mem", "write", "mem write <address> <value> [format]",
            "Write memory (format: hex|dec)",
            {req("address", "address"), req("value", "hex-bytes"),
             opt("format", "format-rw", "hex")}),
        cmd("mem", "dump", "mem dump <address> <size>",
            "Dump memory region in hex", {req("address", "address"), req("size", "number")}),
        cmd("mem", "regions", "mem regions", "List memory regions", {}),
        cmd("mem", "readval", "mem readval <address> <type>",
            "Read a typed value (byte|word|dword|qword|float|double)",
            {req("address", "address"), req("type", "value-type")}),

        // ---- module ----
        cmd("module", "list", "module list", "List all loaded modules", {}),
        cmd("module", "find", "module find <name>", "Find module by name",
            {req("name", "string")}),
        cmd("module", "base", "module base <name>", "Get module base address",
            {req("name", "string")}),
        cmd("module", "exports", "module exports <module>", "List module exports",
            {req("module", "string")}),
        cmd("module", "dump", "module dump <name> [output_file]",
            "Dump module contents: hex or to file",
            {req("name", "string"), opt("output_file", "string", "")}),

        // ---- thread ----
        cmd("thread", "list", "thread list", "List all threads in current process", {}),
        cmd("thread", "suspend", "thread suspend <tid>", "Suspend a thread",
            {req("tid", "tid")}),
        cmd("thread", "resume", "thread resume <tid>", "Resume a thread", {req("tid", "tid")}),
        cmd("thread", "kill", "thread kill <tid>", "Terminate a thread", {req("tid", "tid")}),

        // ---- debug (single entry: debug run, all debug ops via script) ----
        cmd("debug", "run", "debug run <script.json>",
            "Run a scripted debug session (one invocation = one session)",
            {req("script", "script-path")}),

        // ---- disasm ----
        cmd("disasm", "at", "disasm at <address> [count]", "Disassemble at address",
            {req("address", "address"), opt("count", "number", "10")}),
        cmd("disasm", "range", "disasm range <start> <end>", "Disassemble memory range",
            {req("start", "address"), req("end", "address")}),

        // ---- resolve ----
        cmd("resolve", "base", "resolve base <module>", "Get base address of module",
            {req("module", "string")}),
        cmd("resolve", "scan", "resolve scan <pattern>",
            "Pattern scan (AOB, e.g. \"48 8B ?? ?? 00\")", {req("pattern", "pattern")}),

        // ---- watch ----
        cmd("watch", "list", "watch list", "List all watch entries", {}),
        cmd("watch", "add", "watch add <description> <address> <type>",
            "Add watch (type: byte|word|dword|qword|float|double)",
            {req("description", "string"), req("address", "address"), req("type", "value-type")}),
        cmd("watch", "remove", "watch remove <index>", "Remove watch by index",
            {req("index", "index")}),
        cmd("watch", "refresh", "watch refresh", "Refresh all watch values", {}),
        cmd("watch", "clear", "watch clear", "Clear all watches", {}),

        // ---- dll ----
        cmd("dll", "inject", "dll inject <path>", "Inject a DLL into the target process",
            {req("path", "string")}),
        cmd("dll", "eject", "dll eject <path-or-address>", "Eject an injected DLL",
            {req("path-or-address", "string")}),
        cmd("dll", "list", "dll list", "List all injected DLLs", {}),
        cmd("dll", "status", "dll status", "Show DLL injection status", {}),

        // ---- asm ----
        cmd("asm", "assemble", "asm assemble <code> [--hex] [--c-array]",
            "Assemble x64 assembly to bytes (--hex: hex bytes, --c-array: C array)",
            {req("code", "string"), opt("--hex", "flag", ""), opt("--c-array", "flag", "")}),

        // ---- shellcode ----
        cmd("shellcode", "inject", "shellcode inject <hex_bytes>", "Inject shellcode (auto-alloc)",
            {req("hex_bytes", "hex-bytes")}),
        cmd("shellcode", "injectat", "shellcode injectat <address> <hex_bytes>",
            "Inject shellcode at address",
            {req("address", "address"), req("hex_bytes", "hex-bytes")}),
        cmd("shellcode", "status", "shellcode status", "Show shellcode inject status", {}),

        // ---- convert (standalone command, no sub-action) ----
        cmd("convert", "", "convert <type> <value>",
            "Convert typed data to hex bytes "
            "(type: byte|word|dword|qword|float|double|string|hex)",
            {req("type", "convert-type"), req("value", "convert-value")}),
    };
    return kTable;
}

const CommandSpec* find_command(const std::string& group, const std::string& action) {
    for (const auto& c : command_table()) {
        if (c.group == group && c.action == action) return &c;
    }
    return nullptr;
}

bool is_group(const std::string& group) {
    for (const auto& c : command_table()) {
        if (c.group == group) return true;
    }
    return false;
}

std::string command_usage(const CommandSpec& spec) {
    return spec.usage;
}

std::string build_help_text() {
    std::ostringstream os;
    os << "deeptrace_cli v2.1.0\n";
    os << "\n";
    os << "Usage: deeptrace_cli [options] <command> [args...]\n";
    os << "\n";
    os << "Options:\n";
    os << "  -p, --pid <pid>    Target process ID\n";
    os << "  -h, --help         Show help\n";
    os << "  -v, --version      Show version\n";

    std::string cur_group;
    size_t max_usage = 0;
    for (const auto& c : command_table()) {
        if (c.usage.size() > max_usage) max_usage = c.usage.size();
    }
    for (const auto& c : command_table()) {
        if (c.group != cur_group) {
            cur_group = c.group;
            os << "\n" << cur_group << ":\n";
        }
        os << "  " << c.usage;
        size_t pad = max_usage - c.usage.size();
        for (size_t i = 0; i < pad; ++i) os << " ";
        os << "  " << c.brief << "\n";
    }
    os << "\n";
    return os.str();
}

}  // namespace deeptrace_cli
