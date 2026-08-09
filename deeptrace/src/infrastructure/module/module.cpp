#include "infrastructure/module/module.h"
#include "infrastructure/memory/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <cstring>
#include <string>

namespace deeptrace::internal {

namespace {

struct PEImageDosHeader {
    uint16_t e_magic;           // 0x00 'MZ'
    uint8_t e_reserved[0x3A];   // 0x02 .. 0x3B
    uint32_t e_lfanew;          // 0x3C PE header RVA
};

struct PEImageFileHeader {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct PEImageOptionalHeader64 {
    uint16_t Magic;                    // 0x20B
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    // data directories[16]
    struct { uint32_t VirtualAddress; uint32_t Size; } DataDirectory[16];
};

struct PEExportDirectory {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
};

}  // namespace

Result EnumModules(void* hprocess, std::vector<ModuleInfo>& out) {
    out.clear();
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                             ::GetProcessId(static_cast<HANDLE>(hprocess)));
    if (snap == INVALID_HANDLE_VALUE) {
        // fallback to EnumProcessModulesEx
        HMODULE mods[1024];
        DWORD needed = 0;
        if (::EnumProcessModulesEx(static_cast<HANDLE>(hprocess), mods,
                                   sizeof(mods), &needed, LIST_MODULES_ALL)) {
            DWORD count = needed / sizeof(HMODULE);
            for (DWORD i = 0; i < count; ++i) {
                MODULEINFO mi = {};
                ::GetModuleInformation(static_cast<HANDLE>(hprocess), mods[i], &mi, sizeof(mi));
                wchar_t name[MAX_PATH] = {0};
                wchar_t path[MAX_PATH] = {0};
                ::GetModuleBaseNameW(static_cast<HANDLE>(hprocess), mods[i], name, MAX_PATH);
                ::GetModuleFileNameExW(static_cast<HANDLE>(hprocess), mods[i], path, MAX_PATH);
                ModuleInfo info;
                info.base = reinterpret_cast<uintptr_t>(mods[i]);
                info.size = mi.SizeOfImage;
                info.name = name;
                info.path = path;
                out.push_back(info);
            }
        }
        return out.empty() ? Result::Error : Result::Ok;
    }
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    if (::Module32FirstW(snap, &me)) {
        do {
            ModuleInfo info;
            info.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            info.size = me.modBaseSize;
            info.name = me.szModule;
            info.path = me.szExePath;
            out.push_back(info);
        } while (::Module32NextW(snap, &me));
    }
    ::CloseHandle(snap);
    return Result::Ok;
}

Result ParseExports(void* hprocess, uintptr_t base, std::vector<ExportInfo>& out) {
    out.clear();
    uint8_t hdr[0x1000];
    Result r;
    size_t rd = ReadRemoteMemory(hprocess, base, hdr, sizeof(hdr), &r);
    if (r != Result::Ok || rd < sizeof(PEImageDosHeader)) return Result::ReadFault;

    PEImageDosHeader dos;
    std::memcpy(&dos, hdr, sizeof(dos));
    if (dos.e_magic != 0x5A4D) return Result::BadFormat;
    uint32_t e_lfanew = dos.e_lfanew;
    if (e_lfanew + 24 > rd) return Result::BadFormat;

    uint32_t pe_sig = 0;
    std::memcpy(&pe_sig, hdr + e_lfanew, 4);
    if (pe_sig != 0x00004550) return Result::BadFormat;  // PE\0\0

    PEImageFileHeader fh;
    std::memcpy(&fh, hdr + e_lfanew + 4, sizeof(fh));
    size_t opt_off = e_lfanew + 4 + sizeof(PEImageFileHeader);
    if (opt_off + sizeof(PEImageOptionalHeader64) > rd) return Result::BadFormat;

    PEImageOptionalHeader64 oh;
    std::memcpy(&oh, hdr + opt_off, sizeof(oh));
    if (oh.Magic != 0x20B) return Result::BadFormat;

    const uint32_t exp_rva = oh.DataDirectory[0].VirtualAddress;
    const uint32_t exp_size = oh.DataDirectory[0].Size;
    if (exp_rva == 0) return Result::Ok;  // no exports

    // Read export directory.
    std::vector<uint8_t> exp_buf(exp_size ? exp_size : 4096);
    rd = ReadRemoteMemory(hprocess, base + exp_rva, exp_buf.data(), exp_buf.size(), &r);
    if (r != Result::Ok) return Result::ReadFault;

    PEExportDirectory ed;
    std::memcpy(&ed, exp_buf.data(), sizeof(ed));
    if (ed.NumberOfNames == 0) return Result::Ok;

    // Read names RVAs, ordinals, function RVAs.
    std::vector<uint8_t> names_buf(ed.NumberOfNames * 4);
    std::vector<uint8_t> ords_buf(ed.NumberOfNames * 2);
    std::vector<uint8_t> funcs_buf(ed.NumberOfFunctions * 4);
    size_t got = ReadRemoteMemory(hprocess, base + ed.AddressOfNames, names_buf.data(),
                                  names_buf.size(), &r);
    if (r != Result::Ok) return Result::ReadFault;
    got = ReadRemoteMemory(hprocess, base + ed.AddressOfNameOrdinals, ords_buf.data(),
                           ords_buf.size(), &r);
    if (r != Result::Ok) return Result::ReadFault;
    got = ReadRemoteMemory(hprocess, base + ed.AddressOfFunctions, funcs_buf.data(),
                           funcs_buf.size(), &r);
    if (r != Result::Ok) return Result::ReadFault;

    for (uint32_t i = 0; i < ed.NumberOfNames; ++i) {
        uint32_t name_rva = 0;
        std::memcpy(&name_rva, names_buf.data() + i * 4, 4);
        uint16_t ord = 0;
        std::memcpy(&ord, ords_buf.data() + i * 2, 2);
        char name[512] = {0};
        if (name_rva == 0) continue;
        size_t n = ReadRemoteMemory(hprocess, base + name_rva, name, sizeof(name) - 1, &r);
        if (r != Result::Ok) continue;
        name[n] = 0;
        uint32_t func_rva = 0;
        if (ord < ed.NumberOfFunctions) {
            std::memcpy(&func_rva, funcs_buf.data() + ord * 4, 4);
        }
        ExportInfo ei;
        ei.name = name;
        ei.address = base + func_rva;
        out.push_back(ei);
    }
    return Result::Ok;
}

}  // namespace deeptrace::internal
