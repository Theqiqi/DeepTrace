#include "printing/printer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <fcntl.h>
#include <io.h>

using namespace deeptrace_cli;

namespace {

// Redirect stdout to a temp file, run fn, restore, return captured text.
// Uses fd-level redirection because MSVC's stdout is not an assignable lvalue.
template <typename Fn>
std::string capture(Fn fn) {
    char tmpname[L_tmpnam] = {0};
    if (!std::tmpnam(tmpname)) return "";
    int out_fd = _open(tmpname, _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY,
                       _S_IREAD | _S_IWRITE);
    if (out_fd < 0) return "";
    // Flush any pre-test output (e.g. gtest's "Running main()" banner) that is
    // still buffered in stdout; otherwise the first redirected flush would
    // contaminate the captured file and make this test flaky.
    std::fflush(stdout);
    int saved = _dup(_fileno(stdout));
    _dup2(out_fd, _fileno(stdout));
    fn();
    std::fflush(stdout);
    _dup2(saved, _fileno(stdout));
    _close(saved);
    _close(out_fd);
    std::FILE* f = std::fopen(tmpname, "rb");
    std::string out;
    if (f) {
        char buf[256];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        std::fclose(f);
    }
    std::remove(tmpname);
    return out;
}

}  // namespace

TEST(Printer, ToAsciiPassthrough) {
    EXPECT_EQ(printer::to_ascii(L"kernel32.dll"), "kernel32.dll");
}

TEST(Printer, ToAsciiStripsNonAscii) {
    // U+4E2D is non-ascii; must become '?'
    EXPECT_EQ(printer::to_ascii(L"\x4E2D\x6587"), "??");
}

TEST(Printer, ToAsciiEmpty) {
    EXPECT_EQ(printer::to_ascii(L""), "");
}

TEST(Printer, FormatAddress) {
    EXPECT_EQ(printer::format_address(0x140001000ULL), "0x0000000140001000");
    EXPECT_EQ(printer::format_address(0), "0x0000000000000000");
}

TEST(Printer, PrintBytesHex) {
    std::vector<uint8_t> b = {0x48, 0x8B, 0xC3};
    auto s = capture([&] { printer::print_bytes_formatted(b, "hex"); });
    EXPECT_EQ(s, "48 8B C3\n");
}

TEST(Printer, PrintBytesDec) {
    std::vector<uint8_t> b = {0x0A, 0x0B};
    auto s = capture([&] { printer::print_bytes_formatted(b, "dec"); });
    EXPECT_EQ(s, "10 11\n");
}

TEST(Printer, PrintBytesBin) {
    std::vector<uint8_t> b = {0x05};
    auto s = capture([&] { printer::print_bytes_formatted(b, "bin"); });
    EXPECT_EQ(s, "00000101\n");
}

TEST(Printer, PrintBytesAscii) {
    std::vector<uint8_t> b = {'A', 'B', 0x01};
    auto s = capture([&] { printer::print_bytes_formatted(b, "ascii"); });
    EXPECT_EQ(s, "AB.\n");
}

TEST(Printer, PrintBytesEmpty) {
    std::vector<uint8_t> b;
    auto s = capture([&] { printer::print_bytes(b); });
    EXPECT_EQ(s, "\n");
}

TEST(Printer, HexDump) {
    std::vector<uint8_t> b = {0xDE, 0xAD, 0xBE, 0xEF};
    auto s = capture([&] { printer::print_hex_dump(0x1000, b); });
    EXPECT_NE(s.find("0x0000000000001000"), std::string::npos);
    EXPECT_NE(s.find("DE AD BE EF"), std::string::npos);
    EXPECT_NE(s.find("|....|"), std::string::npos);
}

TEST(Printer, PrintMessage) {
    auto s = capture([&] { printer::print_message("OK"); });
    EXPECT_EQ(s, "OK\n");
}

TEST(Printer, Version) {
    auto s = capture([&] { printer::print_version(); });
    EXPECT_EQ(s, "deeptrace_cli v2.5.0\n");
}

TEST(Printer, ErrorGoesToStderr) {
    // print_error writes to stderr; capture() only grabs stdout.
    auto s = capture([&] { printer::print_error("boom"); });
    EXPECT_EQ(s, "");
}
