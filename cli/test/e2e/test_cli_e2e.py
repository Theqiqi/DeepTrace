#!/usr/bin/env python3
"""deeptrace_cli e2e tests - independent Python system (not part of CMake).

Starts deeptrace_target.exe, then drives the real deeptrace_cli.exe binary via
cmd.exe /c and asserts on stdout/exit codes.

Run from the project root:
    python3 cli/test/e2e/test_cli_e2e.py

All tests passing -> exit 0; any failure -> exit 1.
"""

import json
import os
import re
import subprocess
import sys
import time

PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CLI_DIR = os.path.join(PROJECT_ROOT, "cli")
BIN_DIR = os.path.join(CLI_DIR, "out", "bin", "Debug")
CLI_EXE = os.path.join(BIN_DIR, "deeptrace_cli.exe")
TARGET_EXE = os.path.join(BIN_DIR, "deeptrace_target.exe")


def win_path(p):
    """Convert a WSL path like /mnt/c/... to C:\\... (for Windows-side args
    such as dll paths injected into the target process)."""
    if os.name == "nt":
        return p
    m = re.match(r"^/mnt/([a-zA-Z])/(.*)$", p)
    if m:
        return m.group(1).upper() + ":\\" + m.group(2).replace("/", "\\")
    return p


passed = 0
failed = 0
failures = []


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"PASS: {name}")
    else:
        failed += 1
        failures.append(name)
        print(f"FAIL: {name} {detail}")


def run_cli(args):
    """Run deeptrace_cli.exe directly and return (exit_code, stdout, stderr).

    The exe is a Windows binary executed from WSL; the WSL interop layer
    builds the Windows command line with proper quoting, so args containing
    spaces/commas (e.g. assembly strings like "mov rax, 0") arrive intact.
    """
    p = subprocess.run([CLI_EXE] + args, capture_output=True, text=True,
                       cwd=BIN_DIR, timeout=30)
    return p.returncode, p.stdout, p.stderr


def start_target():
    # Direct exec: WSL runs .exe with the /mnt/c/... path.
    p = subprocess.Popen([TARGET_EXE], stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, cwd=BIN_DIR)
    # read banner
    lines = []
    deadline = time.time() + 10
    while time.time() < deadline:
        line = p.stdout.readline()
        if line:
            lines.append(line.rstrip())
            if "WORKER_TID:" in line:
                break
        else:
            break
    return p, lines


def parse_pid(lines):
    for line in lines:
        m = re.search(r"PID:\s*(\d+)", line)
        if m:
            return int(m.group(1))
    return None


def parse_addr(lines, key):
    """Address after '@0x' on the line containing key."""
    for line in lines:
        if key in line:
            m = re.search(r"@0x([0-9a-fA-F]+)", line)
            if m:
                return "0x" + m.group(1)
    return None


def main():
    global passed, failed
    if not os.path.isfile(CLI_EXE):
        print(f"FATAL: deeptrace_cli.exe not found at {CLI_EXE}")
        return 1
    if not os.path.isfile(TARGET_EXE):
        print(f"FATAL: deeptrace_target.exe not found at {TARGET_EXE}")
        return 1

    # ---- no args ----
    code, out, err = run_cli([])
    check("no args exit 1", code == 1)
    check("no args message", "Missing command" in err or "Missing command" in out,
          repr(err))

    # ---- help / version ----
    code, out, _ = run_cli(["-h"])
    check("help exit 0", code == 0)
    check("help contains mem read", "mem read" in out)
    check("help contains shellcode", "shellcode inject" in out)
    code, out, _ = run_cli(["--help"])
    check("long help exit 0", code == 0)
    code, out, _ = run_cli(["-v"])
    check("version exit 0", code == 0)
    check("version string", "deeptrace_cli v2.12.0" in out, repr(out))

    # ---- unknown command ----
    code, _, err = run_cli(["bogus", "cmd"])
    check("unknown group exit 2", code == 2)
    check("unknown group msg", "unknown command group" in err, repr(err))

    # ---- open nonexistent process ----
    code, _, err = run_cli(["ps", "attach", "99999999"])
    check("attach missing process exit 1", code == 1)

    # ---- invalid arguments ----
    # v2.6.0: symbol-shaped tokens (e.g. "zzz") now parse; only shapes that are
    # neither a number nor an identifier are rejected at parse time.
    code, _, err = run_cli(["mem", "read", "foo bar"])
    check("invalid address exit 2", code == 2)
    code, _, err = run_cli(["mem", "read", "a-b"])
    check("invalid address dash exit 2", code == 2)

    # ---- convert (standalone, no process needed) ----
    code, out, _ = run_cli(["convert", "byte", "255"])
    check("convert byte exit 0", code == 0)
    check("convert byte FF", "FF" in out, repr(out))
    code, out, _ = run_cli(["convert", "word", "0x0102"])
    check("convert word LE", "02 01" in out, repr(out))
    code, out, _ = run_cli(["convert", "dword", "100"])
    check("convert dword exit 0", code == 0)
    check("convert dword 64 00 00 00", "64 00 00 00" in out, repr(out))
    code, out, _ = run_cli(["convert", "qword", "0x1122334455667788"])
    check("convert qword LE", "88 77 66 55 44 33 22 11" in out, repr(out))
    code, out, _ = run_cli(["convert", "float", "1.0"])
    check("convert float IEEE754", "00 00 80 3F" in out, repr(out))
    code, out, _ = run_cli(["convert", "double", "1.0"])
    check("convert double IEEE754", "00 00 00 00 00 00 F0 3F" in out, repr(out))
    code, out, _ = run_cli(["convert", "string", "hi"])
    check("convert string ascii", "68 69" in out, repr(out))
    code, out, _ = run_cli(["convert", "hex", "DEADBEEF"])
    check("convert hex passthrough", "DE AD BE EF" in out, repr(out))
    code, _, err = run_cli(["convert", "bogus", "1"])
    check("convert invalid type exit 2", code == 2)
    check("convert invalid type msg", "invalid type" in err, repr(err))
    code, _, err = run_cli(["convert", "dword", "xyz"])
    check("convert invalid value exit 2", code == 2)
    check("convert invalid value msg", "invalid value for type 'dword'" in err,
          repr(err))
    code, _, _ = run_cli(["convert", "byte", "256"])
    check("convert overflow exit 2", code == 2)
    code, _, err = run_cli(["convert", "dword"])
    check("convert missing value exit 2", code == 2)
    check("convert missing value msg", "missing argument: value" in err, repr(err))

    # ---- target ----
    proc, lines = start_target()
    pid = parse_pid(lines)
    if pid is None:
        print("FATAL: could not parse target PID from banner")
        return 1
    print(f"target pid = {pid}")
    g_int = parse_addr(lines, "g_int")
    g_bytes = parse_addr(lines, "g_bytes")
    print(f"g_int = {g_int}, g_bytes = {g_bytes}")
    if g_int is None:
        print("FATAL: could not parse g_int address")
        return 1

    try:
        # ---- ps list contains target ----
        code, out, _ = run_cli(["ps", "list"])
        check("ps list exit 0", code == 0)
        check("ps list contains target pid", str(pid) in out, repr(out[:200]))

        # ---- v2.11.0: ps attach surfaces granted permissions ----
        # Our own target grants full access: the summary must include the
        # memory read/write semantic names.
        code, out, _ = run_cli(["ps", "attach", str(pid)])
        check("ps attach exit 0", code == 0, repr(out))
        check("ps attach shows permissions",
              "permissions:" in out and "read" in out and "write" in out,
              repr(out))

        # ---- ps info with -p ----
        code, out, _ = run_cli(["-p", str(pid), "ps", "info"])
        check("ps info exit 0", code == 0)
        check("ps info shows pid", f"PID: {pid}" in out, repr(out))

        # ---- cross-process read known value ----
        # g_int holds dword 0x11223344 -> bytes 44 33 22 11 (little-endian)
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "4", "hex"])
        check("mem read exit 0", code == 0)
        check("mem read value 44 33 22 11", "44 33 22 11" in out, repr(out))

        # ---- readval ----
        code, out, _ = run_cli(["-p", str(pid), "mem", "readval", g_int, "dword"])
        check("mem readval dword", code == 0 and "0x11223344" in out, repr(out))

        # ---- cross-process modify known value ----
        code, _, _ = run_cli(["-p", str(pid), "mem", "write", g_int, "CAFEBABE", "hex"])
        check("mem write exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "4", "hex"])
        check("mem write applied", "CA FE BA BE" in out, repr(out))
        # restore original dword 0x11223344 (bytes 44 33 22 11)
        run_cli(["-p", str(pid), "mem", "write", g_int, "44332211", "hex"])

        # ---- module / resolve ----
        code, out, _ = run_cli(["-p", str(pid), "module", "base", "deeptrace_target.exe"])
        check("module base exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "resolve", "scan", "DE AD BE EF"])
        check("resolve scan exit 0", code == 0)
        # printer pads addresses to 16 hex digits; compare with the target's
        # known address after stripping leading zeros
        want = g_bytes.lower().lstrip("0x").lstrip("0").lstrip("x")
        got = [h.lower().lstrip("0x").lstrip("0") for h in out.split()]
        check("resolve scan finds g_bytes", any(want == g or want in g for g in got),
              repr(out[:300]))

        # ---- convert output feeds resolve scan (v1.4.1 chain) ----
        # convert dword 0x11223344 -> "44 33 22 11", scan must find g_int
        code, out, _ = run_cli(["convert", "dword", "287454020"])
        check("convert g_int bytes exit 0", code == 0)
        check("convert g_int bytes", "44 33 22 11" in out, repr(out))
        pattern = out.strip()
        code, out2, _ = run_cli(["-p", str(pid), "resolve", "scan", pattern])
        check("convert output scan exit 0", code == 0)
        want_int = g_int.lower().lstrip("0x").lstrip("0").lstrip("x")
        got_int = [h.lower().lstrip("0x").lstrip("0") for h in out2.split()]
        check("convert output scan finds g_int",
              any(want_int == g or want_int in g for g in got_int), repr(out2[:300]))

        # ---- resolve scan is pattern-only again (v1.4.0 typed syntax gone) ----
        code, _, err = run_cli(["-p", str(pid), "resolve", "scan", "48 8B", "dword"])
        check("resolve scan typed syntax rejected exit 2", code == 2)
        check("resolve scan typed syntax msg", "too many arguments" in err, repr(err))

        # ---- v2.12.0: pointer-chain scan (resolve ptrscan) ----
        # Wire a heap slot (script symbol psym) holding a qword pointer to the
        # known module global g_int64, then reverse-walk from g_int64's address
        # with max_offset=0 (exact-pointer match). The chain line must contain
        # psym's padded address + " +0".
        g_int64 = parse_addr(lines, "g_int64")
        check("ptrscan g_int64 parsed", g_int64 is not None, repr(g_int64))
        ptr_aa = os.path.join(BIN_DIR, "e2e_ptrscan.aa")
        with open(ptr_aa, "w") as f:
            f.write("[ENABLE]\nalloc(psym,16)\nregistersymbol(psym)\n\n"
                    "[DISABLE]\ndealloc(psym)\nunregistersymbol(psym)\n")
        code, out, _ = run_cli(["-p", str(pid), "script", "run", win_path(ptr_aa)])
        check("ptrscan script run exit 0", code == 0, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "status"])
        check("ptrscan script status exit 0", code == 0)
        m = re.search(r"alloc\s+psym\s+(0x[0-9A-Fa-f]+)", out)
        check("ptrscan symbol psym allocated", m is not None, repr(out))
        psym = m.group(1)
        # slot holds the qword pointer value g_int64; mem write hex consumes
        # bytes in stored (little-endian) order -> emit LE byte hex string
        _ptrv = int(g_int64, 16)
        ptr_bytes = "".join("%02x" % ((_ptrv >> (8 * i)) & 0xFF) for i in range(8))
        code, out, _ = run_cli(["-p", str(pid), "mem", "write", "psym", ptr_bytes, "hex"])
        check("ptrscan mem write psym exit 0", code == 0, repr(out))

        scan_json = os.path.join(BIN_DIR, "e2e_ptrscan.json")
        with open(scan_json, "w") as f:
            f.write('{"version":1,"target":"%s","max_offset":0,'
                    '"max_level":2,"threads":2}' % g_int64)
        code, out, _ = run_cli(["-p", str(pid), "resolve", "ptrscan", win_path(scan_json)])
        check("ptrscan snapshot exit 0", code == 0, repr(out))
        want_line = "0x" + format(int(psym, 16), "016X") + " +0"
        check("ptrscan finds wired chain", want_line in out, repr(out))
        check("ptrscan snapshot summary", "chains)" in out, repr(out))

        # rescan keeps chains still leading to the (new) target: retarget the
        # slot to g_int and rescan against g_int -> chain survives.
        _intv = int(g_int, 16)
        code, _, _ = run_cli(["-p", str(pid), "mem", "write", "psym",
                              "".join("%02x" % ((_intv >> (8 * i)) & 0xFF)
                                       for i in range(8)), "hex"])
        rescan_json = os.path.join(BIN_DIR, "e2e_ptrscan_rescan.json")
        with open(rescan_json, "w") as f:
            f.write('{"version":1,"target":"%s","max_offset":0,'
                    '"max_level":2,"threads":2,"rescan":{"target":"%s"}}'
                    % (g_int, g_int))
        code, out, _ = run_cli(["-p", str(pid), "resolve", "ptrscan",
                                win_path(rescan_json)])
        check("ptrscan rescan keep exit 0", code == 0, repr(out))
        check("ptrscan rescan keeps chain", want_line in out, repr(out))
        check("ptrscan rescan summary", "after rescan" in out, repr(out))

        # rescan against a different target intersects the chain away.
        rescan2_json = os.path.join(BIN_DIR, "e2e_ptrscan_rescan2.json")
        with open(rescan2_json, "w") as f:
            f.write('{"version":1,"target":"%s","max_offset":0,'
                    '"max_level":2,"threads":2,"rescan":{"target":"%s"}}'
                    % (g_int, g_int64))
        code, out, _ = run_cli(["-p", str(pid), "resolve", "ptrscan",
                                win_path(rescan2_json)])
        check("ptrscan rescan drop exit 0", code == 0, repr(out))
        check("ptrscan rescan drops chain", "No chains found" in out, repr(out))

        # config errors: bad version -> exit 2 (no attach needed)
        bad_json = os.path.join(BIN_DIR, "e2e_ptrscan_bad.json")
        with open(bad_json, "w") as f:
            f.write('{"version":9,"target":"0x10"}')
        code, _, err = run_cli(["resolve", "ptrscan", win_path(bad_json)])
        check("ptrscan bad version exit 2", code == 2)
        check("ptrscan bad version msg", "invalid version" in err, repr(err))

        run_cli(["-p", str(pid), "script", "disable", win_path(ptr_aa)])
        for tmp in (ptr_aa, scan_json, rescan_json, rescan2_json, bad_json):
            os.remove(tmp)

        # ---- thread ----
        code, out, _ = run_cli(["-p", str(pid), "thread", "list"])
        check("thread list exit 0", code == 0)


        # ---- disasm ----
        if g_bytes:
            code, out, _ = run_cli(["-p", str(pid), "disasm", "at", g_bytes, "2"])
            check("disasm at exit 0", code == 0)

        # ---- asm ----
        code, out, _ = run_cli(["asm", "assemble", "nop; nop; ret"])
        check("asm assemble exit 0", code == 0)
        check("asm bytes 9090C3", "9090C3" in out, repr(out))
        code, out, _ = run_cli(["asm", "assemble", "nop", "--c-array"])
        check("asm c-array", "unsigned char code[]" in out, repr(out))
        code, out, _ = run_cli(["asm", "assemble", "add rax, 0"])
        check("asm add exit 0", code == 0)
        check("asm add bytes 4883C000", "4883C000" in out, repr(out))

        # ---- v2.2.0: asm file / hex2bin / shellcode staged ops ----
        asm_path = os.path.join(BIN_DIR, "e2e_code.asm")
        bin_path = os.path.join(BIN_DIR, "e2e_code.bin")
        with open(asm_path, "w") as f:
            f.write("xor eax, eax\nret\n")
        code, out, _ = run_cli(["asm", "file", win_path(asm_path), "--out",
                                win_path(bin_path)])
        check("asm file exit 0", code == 0)
        check("asm file bytes 31C0C3", "31C0C3" in out, repr(out))
        with open(bin_path, "rb") as f:
            data = f.read()
        check("asm file wrote bin", data == bytes([0x31, 0xC0, 0xC3]), repr(data))
        os.remove(asm_path)
        os.remove(bin_path)
        code, _, err = run_cli(["asm", "file", "no_such.asm"])
        check("asm file missing exit 2", code == 2)
        check("asm file missing msg", "cannot read file" in err, repr(err))

        code, out, _ = run_cli(["hex2bin", "DEADBEEF", win_path(bin_path)])
        check("hex2bin exit 0", code == 0)
        with open(bin_path, "rb") as f:
            data = f.read()
        check("hex2bin bytes", data == bytes([0xDE, 0xAD, 0xBE, 0xEF]), repr(data))
        code, _, err = run_cli(["hex2bin", "ABC", win_path(bin_path)])
        check("hex2bin bad hex exit 2", code == 2)

        # shellcode staged ops: alloc (write only) -> run (trigger x2) -> free
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "alloc", "C3"])
        check("shellcode alloc exit 0", code == 0)
        m = re.search(r"0x([0-9A-Fa-f]{16})", out)
        check("shellcode alloc prints address", bool(m), repr(out))
        sc_addr = "0x" + m.group(1) if m else "0x1"
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "status"])
        check("shellcode status exit 0", code == 0)
        check("shellcode status lists record", sc_addr.lower() in out.lower(), repr(out))
        code, _, _ = run_cli(["-p", str(pid), "shellcode", "alloc", "zzzz"])
        check("shellcode alloc bad source exit 2", code == 2)
        code, _, _ = run_cli(["-p", str(pid), "shellcode", "run", sc_addr])
        check("shellcode run exit 0", code == 0)
        code, _, _ = run_cli(["-p", str(pid), "shellcode", "run", sc_addr])
        check("shellcode run repeat exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "free", sc_addr])
        check("shellcode free exit 0", code == 0)
        check("shellcode free OK", "OK" in out, repr(out))
        code, _, err = run_cli(["-p", str(pid), "shellcode", "run", sc_addr])
        check("shellcode run after free exit 1", code == 1)
        check("shellcode run after free NotFound", "NotFound" in err, repr(err))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after staged ops", str(pid) in out, repr(out[:200]))

        # shellcode exec: one invocation = complete flow, from a .bin file
        code, _, _ = run_cli(["hex2bin", "C3", win_path(bin_path)])
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "exec", win_path(bin_path)])
        check("shellcode exec bin exit 0", code == 0)
        m = re.search(r"0x([0-9A-Fa-f]{16})", out)
        if m:
            run_cli(["-p", str(pid), "shellcode", "free", "0x" + m.group(1)])
        os.remove(bin_path)
        code, _, err = run_cli(["-p", str(pid), "shellcode", "exec", "zzzz"])
        check("shellcode exec bad source exit 2", code == 2)
        check("shellcode exec bad source msg", "invalid shellcode source" in err,
              repr(err))

        # shellcode exec from an .asm source (assembled in memory)
        with open(asm_path, "w") as f:
            f.write("ret\n")
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "exec", win_path(asm_path)])
        check("shellcode exec asm exit 0", code == 0)
        m = re.search(r"0x([0-9A-Fa-f]{16})", out)
        if m:
            run_cli(["-p", str(pid), "shellcode", "free", "0x" + m.group(1)])
        os.remove(asm_path)

        # shellcode injectfile: .bin file -> inject (execute immediately)
        code, _, _ = run_cli(["hex2bin", "C3", win_path(bin_path)])
        code, out, _ = run_cli(["-p", str(pid), "shellcode", "injectfile",
                                win_path(bin_path)])
        check("shellcode injectfile exit 0", code == 0)
        m = re.search(r"0x([0-9A-Fa-f]{16})", out)
        if m:
            run_cli(["-p", str(pid), "shellcode", "free", "0x" + m.group(1)])
        os.remove(bin_path)
        code, _, err = run_cli(["-p", str(pid), "shellcode", "injectfile", "no_such.bin"])
        check("shellcode injectfile missing exit 2", code == 2)
        check("shellcode injectfile missing msg", "cannot read file" in err, repr(err))

        # ---- watch ----
        run_cli(["-p", str(pid), "watch", "clear"])
        code, out, _ = run_cli(["-p", str(pid), "watch", "add", "w1", g_int, "dword"])
        check("watch add exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "watch", "refresh"])
        check("watch refresh exit 0", code == 0)
        check("watch shows value", "0x11223344" in out, repr(out))
        run_cli(["-p", str(pid), "watch", "remove", "0"])
        # watch list must show live values too (list reads target memory)
        code, out, _ = run_cli(["-p", str(pid), "watch", "add", "w2", g_int, "dword"])
        check("watch list value exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "watch", "list"])
        check("watch list shows value", "0x11223344" in out, repr(out))
        run_cli(["-p", str(pid), "watch", "remove", "0"])

        # ---- v2.1.0: standalone debug commands are removed (single entry: debug run) ----
        for action in ["step", "break", "registers", "attach", "status", "pause",
                       "resume", "next", "register", "guard", "hbreak", "hclear",
                       "clear", "detach", "unguard"]:
            code, _, err = run_cli(["-p", str(pid), "debug", action])
            check(f"debug {action} rejected exit 2", code == 2)
            check(f"debug {action} unknown command msg", "unknown command" in err,
                  repr(err))
        # the target must be intact (no 0xCC pollution from rejected commands)
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "1", "hex"])
        check("target intact after rejected debug cmds", "44" in out, repr(out))

        # ---- debug run: scripted session (one invocation = one session) ----
        # Scripts are real fixtures under cli/test/scripts/; tests substitute
        # the %G_INT% placeholder with the runtime address and write a temp
        # copy under BIN_DIR (the CLI is a Windows binary, so it needs a
        # Windows path to read the file).
        SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "cli", "test", "scripts")

        def materialize(fixture, tag):
            with open(os.path.join(SCRIPTS_DIR, fixture)) as f:
                content = f.read()
            content = content.replace("%G_INT%", g_int)
            tmp = os.path.join(BIN_DIR, f"e2e_{tag}.json")
            with open(tmp, "w") as f:
                f.write(content)
            return win_path(tmp)

        script_win = materialize("debug_session.json", "session")
        code, out, _ = run_cli(["-p", str(pid), "debug", "run", script_win])
        check("debug run scripted session exit 0", code == 0)
        check("debug run step headers", "[1] status" in out, repr(out[:200]))
        check("debug run watch value", "0x11223344" in out, repr(out))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after debug run", str(pid) in out, repr(out[:200]))
        os.remove(os.path.join(BIN_DIR, "e2e_session.json"))

        # ---- debug run: write with space-separated hex bytes ----
        script2_win = materialize("debug_write.json", "write")
        code, _, _ = run_cli(["-p", str(pid), "debug", "run", script2_win])
        check("debug run spaced write exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "4", "hex"])
        check("debug run spaced write applied", "BE BA FE CA" in out, repr(out))
        run_cli(["-p", str(pid), "mem", "write", g_int, "44332211", "hex"])
        os.remove(os.path.join(BIN_DIR, "e2e_write.json"))

        # ---- debug run: script errors -> exit 2 ----
        code, _, err = run_cli(["-p", str(pid), "debug", "run", "no_such.json"])
        check("debug run missing file exit 2", code == 2)
        check("debug run missing file msg", "cannot open script file" in err, repr(err))
        script3_win = materialize("debug_bad.json", "bad")
        code, _, err = run_cli(["-p", str(pid), "debug", "run", script3_win])
        check("debug run unknown op exit 2", code == 2)
        check("debug run unknown op msg", "unknown op" in err, repr(err))
        os.remove(os.path.join(BIN_DIR, "e2e_bad.json"))

        # ---- v2.3.0: AA-style script engine (script run/disable/status) ----
        # Real fixtures under cli/test/scripts/ (script_call.aa / script_bad.aa);
        # copied to a temp file next to the CLI (Windows-readable path).
        SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "cli", "test", "scripts")

        def materialize_aa(fixture, tag, hook_off=""):
            with open(os.path.join(SCRIPTS_DIR, fixture)) as f:
                content = f.read()
            content = content.replace("%HOOK_OFF%", hook_off)
            tmp = os.path.join(BIN_DIR, f"e2e_{tag}.aa")
            with open(tmp, "w") as f:
                f.write(content)
            return win_path(tmp)

        script_aa_win = materialize_aa("script_call.aa", "call")
        code, out, _ = run_cli(["-p", str(pid), "script", "run", script_aa_win])
        check("script run exit 0", code == 0)
        check("script run alloc summary", "alloc newmem" in out, repr(out))
        check("script run createThread", "createThread" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "run", script_aa_win])
        check("script run idempotent exit 0", code == 0)
        check("script run already enabled", "already enabled" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "status"])
        check("script status exit 0", code == 0)
        check("script status lists script", "e2e_call.aa" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "disable", script_aa_win])
        check("script disable exit 0", code == 0)
        check("script disable dealloc", "dealloc newmem" in out, repr(out))
        check("script disable OK", "OK" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "disable", script_aa_win])
        check("script disable idempotent exit 0", code == 0)
        check("script disable already disabled", "already disabled" in out, repr(out))
        # parse error -> exit 2 (real fixture script_bad.aa)
        bad_win = materialize_aa("script_bad.aa", "bad")
        code, _, err = run_cli(["-p", str(pid), "script", "run", bad_win])
        check("script run bad block exit 2", code == 2)
        check("script run bad block msg", "unknown block" in err, repr(err))
        os.remove(os.path.join(BIN_DIR, "e2e_call.aa"))
        os.remove(os.path.join(BIN_DIR, "e2e_bad.aa"))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after script ops", str(pid) in out, repr(out[:200]))

        # ---- v2.4.0: script check (syntax + assembly precheck, no attach) ----
        # Pure local validation: no -p, no target side effects.
        ok_win = materialize_aa("script_call.aa", "check_ok")
        code, out, _ = run_cli(["script", "check", ok_win])
        check("script check valid exit 0", code == 0)
        check("script check OK summary", "OK (" in out and "steps" in out, repr(out))
        os.remove(os.path.join(BIN_DIR, "e2e_check_ok.aa"))

        # hook fixture with a materialized offset passes check
        hook_win = materialize_aa("script_hook.aa", "check_hook", hook_off="0x1000")
        code, out, _ = run_cli(["script", "check", hook_win])
        check("script check hook exit 0", code == 0)
        os.remove(os.path.join(BIN_DIR, "e2e_check_hook.aa"))

        # syntax error -> exit 2 (real fixture script_bad.aa)
        bad_check_win = materialize_aa("script_bad.aa", "check_bad")
        code, _, err = run_cli(["script", "check", bad_check_win])
        check("script check bad block exit 2", code == 2)
        check("script check bad block msg", "script parse error" in err, repr(err))
        os.remove(os.path.join(BIN_DIR, "e2e_check_bad.aa"))

        # assembly precheck failure -> exit 2 (real fixture script_badasm.aa)
        badasm_check_win = materialize_aa("script_badasm.aa", "check_badasm")
        code, _, err = run_cli(["script", "check", badasm_check_win])
        check("script check bad asm exit 2", code == 2)
        check("script check bad asm msg", "BadFormat" in err, repr(err))
        os.remove(os.path.join(BIN_DIR, "e2e_check_badasm.aa"))

        # missing file -> exit 2
        code, _, err = run_cli(["script", "check", "no_such.aa"])
        check("script check missing file exit 2", code == 2)
        check("script check missing file msg", "cannot read file" in err, repr(err))

        # hook structure errors detected statically (no attach)
        struct_cases = [
            ("[ENABLE]\n\"m.dll\"+100:\nnop 2\n", "hook target must be followed"),
            ("[ENABLE]\n\"m.dll\"+100:\njmp nonexist\n", "undefined label"),
            ("[ENABLE]\nalloc(n,8)\n\"m.dll\"+100:\njmp n\nmov eax,1\n",
             "only 'jmp <label>'"),
        ]
        for i, (content, msg) in enumerate(struct_cases):
            tmp = os.path.join(BIN_DIR, f"e2e_struct_{i}.aa")
            with open(tmp, "w") as f:
                f.write(content)
            code, _, err = run_cli(["script", "check", win_path(tmp)])
            check(f"script check struct case {i} exit 2", code == 2)
            check(f"script check struct case {i} msg", msg in err, repr(err))
            os.remove(tmp)
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after script check", str(pid) in out, repr(out[:200]))

        # ---- v2.5.0: artificial pointer (symbol refs on any instruction) ----
        # The aptr fixture spawns a thread that writes two known 64-bit values
        # into two slots: slotA via moffs64 (mov [slotA],rax), slotB via
        # RIP-relative (mov [slotB],rcx). Read both back through the CLI.
        aptr_win = materialize_aa("script_aptr.aa", "aptr")
        code, out, _ = run_cli(["script", "check", aptr_win])
        check("script check aptr exit 0", code == 0)
        check("script check aptr OK", "OK (" in out, repr(out))
        os.remove(os.path.join(BIN_DIR, "e2e_aptr.aa"))

        aptr_win = materialize_aa("script_aptr.aa", "aptr")
        code, out, _ = run_cli(["-p", str(pid), "script", "run", aptr_win])
        check("script run aptr exit 0", code == 0)
        check("script run aptr thread", "createThread" in out, repr(out))
        # status must list the two slots under the script (owner preserved)
        code, out, _ = run_cli(["-p", str(pid), "script", "status"])
        check("script status aptr exit 0", code == 0)
        m_a = re.search(r"alloc\s+slotA\s+0x([0-9A-Fa-f]{16})", out)
        m_b = re.search(r"alloc\s+slotB\s+0x([0-9A-Fa-f]{16})", out)
        check("script status lists slotA", bool(m_a), repr(out))
        check("script status lists slotB", bool(m_b), repr(out))
        if m_a and m_b:
            code, out, _ = run_cli(["-p", str(pid), "mem", "read",
                                    "0x" + m_a.group(1), "8", "hex"])
            check("aptr moffs64 value read back",
                  "88 77 66 55 44 33 22 11" in out, repr(out))
            code, out, _ = run_cli(["-p", str(pid), "mem", "read",
                                    "0x" + m_b.group(1), "8", "hex"])
            check("aptr rip-relative value read back",
                  "00 FF EE DD CC BB AA 99" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "script", "disable", aptr_win])
        check("script disable aptr exit 0", code == 0)
        check("script disable aptr dealloc", "dealloc" in out, repr(out))
        os.remove(os.path.join(BIN_DIR, "e2e_aptr.aa"))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after aptr ops", str(pid) in out, repr(out[:200]))

        # ---- v2.6.0: symbol addressing (address args accept script symbols) ----
        # After `script run` of the aptr fixture, `mem read slotA` resolves the
        # symbol to its recorded address without a manual address lookup.
        aptr_win = materialize_aa("script_aptr.aa", "aptr")
        code, out, _ = run_cli(["-p", str(pid), "script", "run", aptr_win])
        check("symbol addressing script run exit 0", code == 0)
        # read by symbol name
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", "slotA", "8", "hex"])
        check("symbol addressing mem read slotA",
              "88 77 66 55 44 33 22 11" in out, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", "slotB", "8", "hex"])
        check("symbol addressing mem read slotB",
              "00 FF EE DD CC BB AA 99" in out, repr(out))
        # readval by symbol
        code, out, _ = run_cli(["-p", str(pid), "mem", "readval", "slotA", "qword"])
        check("symbol addressing readval slotA", "0x1122334455667788" in out,
              repr(out))
        # v2.8.0: write by symbol - dynamically retarget the artificial pointer
        # (slotA held 0x1122334455667788 from the thread; write a new target)
        code, out, _ = run_cli(["-p", str(pid), "mem", "write", "slotA",
                                "8877665544332211", "hex"])
        check("symbol addressing mem write slotA exit 0", code == 0, repr(out))
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", "slotA", "8", "hex"])
        check("symbol addressing mem write readback",
              "88 77 66 55 44 33 22 11" in out, repr(out))
        # dec format: fixed 8-byte little-endian (1122334455667788 dec =
        # 0x0003FCC1DA8C9C4C -> bytes 4C 9C 8C DA C1 FC 03 00)
        code, _, _ = run_cli(["-p", str(pid), "mem", "write", "slotA",
                              "1122334455667788", "dec"])
        check("symbol addressing mem write dec exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "mem", "read", "slotA", "8", "hex"])
        check("symbol addressing mem write dec readback",
              "4C 9C 8C DA C1 FC 03 00" in out, repr(out))
        # restore the original pointer value (0x1122334455667788 = bytes
        # 88 77 66 55 44 33 22 11) so the watch case below sees it again
        code, _, _ = run_cli(["-p", str(pid), "mem", "write", "slotA",
                              "8877665544332211", "hex"])
        check("symbol addressing mem write restore exit 0", code == 0)
        # unknown symbol on write -> NotFound
        code, _, err = run_cli(["-p", str(pid), "mem", "write", "nosuch",
                                "1122334455667788", "dec"])
        check("symbol addressing mem write unknown exit 1", code == 1)
        check("symbol addressing mem write unknown msg", "NotFound" in err,
              repr(err))
        # watch add by symbol -> live value
        code, _, _ = run_cli(["-p", str(pid), "watch", "clear"])
        code, out, _ = run_cli(["-p", str(pid), "watch", "add", "aptr_sym", "slotA",
                                "qword"])
        check("symbol addressing watch add exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "watch", "list"])
        check("symbol addressing watch list value", "0x1122334455667788" in out,
              repr(out))
        run_cli(["-p", str(pid), "watch", "remove", "0"])
        # unknown symbol -> NotFound (business error, exit 1)
        code, _, err = run_cli(["-p", str(pid), "mem", "read", "no_such_sym", "8"])
        check("symbol addressing unknown symbol exit 1", code == 1)
        check("symbol addressing unknown symbol msg", "NotFound" in err, repr(err))
        code, _, err = run_cli(["-p", str(pid), "watch", "add", "d", "nosuch", "qword"])
        check("symbol addressing watch unknown exit 1", code == 1)
        # disable frees the slots by name; re-reading the symbol -> NotFound
        code, _, _ = run_cli(["-p", str(pid), "script", "disable", aptr_win])
        code, _, err = run_cli(["-p", str(pid), "mem", "read", "slotA", "8"])
        check("symbol addressing after disable exit 1", code == 1)
        os.remove(os.path.join(BIN_DIR, "e2e_aptr.aa"))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after symbol addressing", str(pid) in out,
              repr(out[:200]))

        # ---- v2.7.0: real near allocation (alloc third arg within +/-2GB) ----
        # alloc(nearmem,64,"deeptrace_target.exe"+1000) must place the buffer
        # inside the +/-2GB window around the module base + 0x1000 anchor, so
        # RIP-relative rel32 jumps to it never overflow.
        code, out, _ = run_cli(["-p", str(pid), "module", "base",
                                "deeptrace_target.exe"])
        check("near module base exit 0", code == 0)
        m_base = re.search(r"0x([0-9A-Fa-f]{16})", out)
        check("near module base parsed", bool(m_base), repr(out))
        if m_base:
            anchor = int(m_base.group(1), 16) + 0x1000
            tmp_aa = os.path.join(BIN_DIR, "e2e_near.aa")
            with open(tmp_aa, "w") as f:
                f.write('[ENABLE]\nalloc(nearmem,64,"deeptrace_target.exe"+1000)\n'
                        '[DISABLE]\ndealloc(nearmem)\n')
            code, out, _ = run_cli(["-p", str(pid), "script", "run", win_path(tmp_aa)])
            check("near script run exit 0", code == 0)
            check("near alloc summary", "alloc nearmem" in out, repr(out))
            m_near = re.search(r"0x([0-9A-Fa-f]{16})", out)
            check("near alloc address parsed", bool(m_near), repr(out))
            if m_near:
                nearmem = int(m_near.group(1), 16)
                dist = abs(nearmem - anchor)
                check("near alloc within +-2GB", dist <= 0x7FFFFFFF,
                      f"nearmem={nearmem:#x} anchor={anchor:#x} dist={dist:#x}")
                # the near-allocated symbol still resolves by name (v2.6.0)
                code, out, _ = run_cli(["-p", str(pid), "mem", "read", "nearmem",
                                        "8", "hex"])
                check("near symbol read exit 0", code == 0)
            code, out, _ = run_cli(["-p", str(pid), "script", "disable",
                                    win_path(tmp_aa)])
            check("near script disable exit 0", code == 0)
            os.remove(tmp_aa)
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after near allocation", str(pid) in out,
              repr(out[:200]))

        # ---- v2.9.0: batch locator JSON (mem batch read/write) ----
        # JSON-driven pointer chains: module+base / symbol / absolute + offsets.
        # The fixture allocs three slots; wire values, then drive batch files.
        pc_win = materialize_aa("script_ptrchain.aa", "ptrchain")
        code, out, _ = run_cli(["-p", str(pid), "script", "run", pc_win])
        check("batch script run exit 0", code == 0)
        code, out, _ = run_cli(["-p", str(pid), "script", "status"])
        m_p = re.search(r"alloc\s+ptr_slot\s+0x([0-9A-Fa-f]{16})", out)
        m_d = re.search(r"alloc\s+data_slot\s+0x([0-9A-Fa-f]{16})", out)
        m_s = re.search(r"alloc\s+str_slot\s+0x([0-9A-Fa-f]{16})", out)
        check("batch status lists ptr_slot", bool(m_p), repr(out))
        check("batch status lists data_slot", bool(m_d), repr(out))
        check("batch status lists str_slot", bool(m_s), repr(out))
        if m_p and m_d and m_s:
            ptr_slot = int(m_p.group(1), 16)
            data_slot = int(m_d.group(1), 16)
            str_slot = int(m_s.group(1), 16)
            # wire: data_slot = 0x1122334455667788, ptr_slot -> data_slot,
            # str_slot = "hello" (no thread race; fully deterministic)
            code, _, _ = run_cli(["-p", str(pid), "mem", "write",
                                  f"0x{data_slot:X}", "8877665544332211", "hex"])
            check("batch wire data_slot exit 0", code == 0)
            # address as little-endian bytes (mem write hex stores memory order)
            addr_hex = f"{data_slot:016X}"
            le_hex = "".join(reversed([addr_hex[i:i + 2]
                                        for i in range(0, 16, 2)]))
            code, _, _ = run_cli(["-p", str(pid), "mem", "write",
                                  f"0x{ptr_slot:X}", le_hex, "hex"])
            check("batch wire ptr_slot exit 0", code == 0)
            code, _, _ = run_cli(["-p", str(pid), "mem", "write",
                                  f"0x{str_slot:X}", "68656c6c6f", "hex"])
            check("batch wire str_slot exit 0", code == 0)
            code, out, _ = run_cli(["-p", str(pid), "module", "base",
                                    "deeptrace_target.exe"])
            m_base = re.search(r"0x([0-9A-Fa-f]{16})", out)
            g_int64 = parse_addr(lines, "g_int64")
            if m_base and g_int64:
                mod_off = int(g_int64, 16) - int(m_base.group(1), 16)
                # batch read: every locator source + type
                batch_json = os.path.join(BIN_DIR, "e2e_batch_read.json")
                with open(batch_json, "w") as f:
                    f.write(json.dumps({
                        "version": 1,
                        "process": "deeptrace_target.exe",
                        "values": {
                            "chain_qword": {"symbol": "ptr_slot",
                                            "offsets": ["0x0"],
                                            "type": "qword"},
                            "data_direct": {"symbol": "data_slot",
                                            "type": "qword"},
                            "str": {"symbol": "str_slot",
                                    "type": "string"},
                            "buf": {"symbol": "str_slot", "type": "bytes",
                                    "count": 5},
                            "mod_qword": {"module": "deeptrace_target.exe",
                                          "base": f"0x{mod_off:X}",
                                          "type": "qword"},
                            "abs_dword": {"base": g_int, "type": "dword"},
                        },
                    }))
                code, out, _ = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(batch_json)])
                check("batch read exit 0", code == 0, repr(out))
                check("batch read header",
                      "NAME" in out and "ADDRESS" in out and "VALUE" in out,
                      repr(out))
                check("batch read chain value",
                      "chain_qword" in out and "0x1122334455667788" in out,
                      repr(out))
                check("batch read data_direct",
                      "data_direct" in out and "0x1122334455667788" in out,
                      repr(out))
                check("batch read string", "str" in out and "hello" in out,
                      repr(out))
                check("batch read bytes",
                      "buf" in out and "68 65 6C 6C 6F" in out, repr(out))
                check("batch read mod_qword",
                      "mod_qword" in out and "0x1122334455667788" in out,
                      repr(out))
                check("batch read abs_dword",
                      "abs_dword" in out and "0x11223344" in out, repr(out))
                # batch write: chain (qword), string, module+base
                batch_w = os.path.join(BIN_DIR, "e2e_batch_write.json")
                with open(batch_w, "w") as f:
                    f.write(json.dumps({
                        "values": {
                            "chain_write": {"symbol": "ptr_slot",
                                            "offsets": ["0x0"],
                                            "type": "qword",
                                            "value": "0x99AABBCCDDEEFF00"},
                            "str_write": {"symbol": "str_slot",
                                          "type": "string",
                                          "value": "world"},
                            "mod_write": {"module": "deeptrace_target.exe",
                                          "base": f"0x{mod_off:X}",
                                          "type": "qword",
                                          "value": "0x8877665544332211"},
                        },
                    }))
                code, out, _ = run_cli(["-p", str(pid), "mem", "batch", "write",
                                        win_path(batch_w)])
                check("batch write exit 0", code == 0, repr(out))
                check("batch write OK lines",
                      "OK chain_write" in out and "OK str_write" in out and
                      "OK mod_write" in out, repr(out))
                # readback: chain end (data_slot) now 0x99AABBCCDDEEFF00
                code, out, _ = run_cli(["-p", str(pid), "mem", "read",
                                        f"0x{data_slot:X}", "8", "hex"])
                check("batch chain write readback",
                      "00 FF EE DD CC BB AA 99" in out, repr(out))
                # readback: str_slot now "world"
                code, out, _ = run_cli(["-p", str(pid), "mem", "read",
                                        f"0x{str_slot:X}", "8", "hex"])
                check("batch string write readback",
                      "77 6F 72 6C 64" in out, repr(out))
                # readback: g_int64 now 0x8877665544332211 (module+base write)
                code, out, _ = run_cli(["-p", str(pid), "mem", "read",
                                        g_int64, "8", "hex"])
                check("batch module write readback",
                      "11 22 33 44 55 66 77 88" in out, repr(out))
                # restore g_int64 to its original value
                code, _, _ = run_cli(["-p", str(pid), "mem", "write", g_int64,
                                      "8877665544332211", "hex"])
                check("batch restore g_int64 exit 0", code == 0)
                # ---- v2.10.0: CSV/JSON export (--format/--out) ----
                # JSON export to stdout is parseable and carries status/error
                code, out, _ = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(batch_json), "--format", "json"])
                check("batch json export exit 0", code == 0, repr(out[:200]))
                try:
                    rows = json.loads(out)
                    # note: batch write above retargeted the chain end to
                    # 0x99AABBCCDDEEFF00, so the exported chain value is that
                    ok = isinstance(rows, list) and len(rows) == 6 and \
                         all(r.get("status") == "ok" for r in rows) and \
                         any(r.get("name") == "chain_qword" and
                             r.get("value") == "0x99AABBCCDDEEFF00" for r in rows)
                    check("batch json export parseable", ok, repr(out[:400]))
                except Exception as e:
                    check("batch json export parseable", False, repr(e))
                # CSV export to file: header + data row + status column
                exp_csv = os.path.join(BIN_DIR, "e2e_batch_export.csv")
                code, _, _ = run_cli(["-p", str(pid), "mem", "batch", "read",
                                      win_path(batch_json), "--format", "csv",
                                      "--out", win_path(exp_csv)])
                check("batch csv export exit 0", code == 0)
                if os.path.isfile(exp_csv):
                    with open(exp_csv, "r") as f:
                        csv_content = f.read()
                    check("batch csv header",
                          csv_content.startswith("name,address,value,status,error"),
                          repr(csv_content[:200]))
                    check("batch csv ok status", "ok" in csv_content,
                          repr(csv_content[:400]))
                    check("batch csv chain row",
                          "chain_qword,0x" in csv_content and
                          ",0x99AABBCCDDEEFF00,ok," in csv_content,
                          repr(csv_content[:400]))
                    os.remove(exp_csv)
                # JSON export includes error rows with messages (exit 1)
                exp_bad = os.path.join(BIN_DIR, "e2e_batch_export_bad.json")
                with open(exp_bad, "w") as f:
                    f.write('{ "values": { "x": { "symbol": "no_such_sym",'
                            ' "type": "qword" } } }')
                code, out, _ = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(exp_bad), "--format", "json"])
                check("batch json export error exit 1", code == 1, repr(out[:200]))
                try:
                    rows = json.loads(out)
                    err = rows[0].get("error", "") if rows else ""
                    ok = len(rows) == 1 and rows[0].get("status") == "error" and \
                         "NotFound" in err
                    check("batch json export error row", ok, repr(out[:300]))
                except Exception as e:
                    check("batch json export error row", False, repr(e))
                os.remove(exp_bad)
                # write-mode export: per-item result rows (value empty)
                exp_w = os.path.join(BIN_DIR, "e2e_batch_write_export.json")
                code, _, _ = run_cli(["-p", str(pid), "mem", "batch", "write",
                                      win_path(batch_w), "--format", "json",
                                      "--out", win_path(exp_w)])
                check("batch write export exit 0", code == 0)
                if os.path.isfile(exp_w):
                    with open(exp_w, "r") as f:
                        wcontent = f.read()
                    check("batch write export rows",
                          "chain_write" in wcontent and "str_write" in wcontent,
                          repr(wcontent[:300]))
                    check("batch write export ok status", "\"status\":\"ok\"" in wcontent,
                          repr(wcontent[:300]))
                    os.remove(exp_w)
                os.remove(batch_json)
                os.remove(batch_w)
                # errors: bad version -> 2; unknown symbol -> 1; mismatch -> 1
                bad = os.path.join(BIN_DIR, "e2e_batch_bad.json")
                with open(bad, "w") as f:
                    f.write('{ "version": 2, "values": {} }')
                code, _, err = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(bad)])
                check("batch bad version exit 2", code == 2, repr(err))
                os.remove(bad)
                nosym = os.path.join(BIN_DIR, "e2e_batch_nosym.json")
                with open(nosym, "w") as f:
                    f.write('{ "values": { "x": { "symbol": "no_such_sym",'
                            ' "type": "qword" } } }')
                code, _, err = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(nosym)])
                check("batch unknown symbol exit 1", code == 1)
                check("batch unknown symbol msg", "NotFound" in err, repr(err))
                os.remove(nosym)
                mismatch = os.path.join(BIN_DIR, "e2e_batch_mismatch.json")
                with open(mismatch, "w") as f:
                    f.write('{ "process": "notepad.exe", "values": {} }')
                code, _, err = run_cli(["-p", str(pid), "mem", "batch", "read",
                                        win_path(mismatch)])
                check("batch process mismatch exit 1", code == 1)
                check("batch process mismatch msg", "process mismatch" in err,
                      repr(err))
                os.remove(mismatch)
            else:
                print("SKIP: batch module+base (module base or g_int64 missing)")
        code, _, _ = run_cli(["-p", str(pid), "script", "disable", pc_win])
        check("batch script disable exit 0", code == 0)
        os.remove(os.path.join(BIN_DIR, "e2e_ptrchain.aa"))
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after batch locators", str(pid) in out,
              repr(out[:200]))

        # ---- dll inject round trip (companion testdll.dll) ----
        # The path must be a Windows path: it is written into the target and
        # loaded with LoadLibraryA there (the target cannot see /mnt/c/...).
        dll = win_path(os.path.join(BIN_DIR, "testdll.dll"))
        if os.path.isfile(os.path.join(BIN_DIR, "testdll.dll")):
            code, out, _ = run_cli(["-p", str(pid), "dll", "inject", dll])
            check("dll inject exit 0", code == 0)
            code, out, _ = run_cli(["-p", str(pid), "dll", "list"])
            check("dll list exit 0", code == 0)
            check("dll list shows testdll", "testdll" in out, repr(out))
            code, _, _ = run_cli(["-p", str(pid), "dll", "eject", dll])
            check("dll eject exit 0", code == 0)
        else:
            print("SKIP: dll inject (testdll.dll not in cli out dir)")

    finally:
        # cleanup: terminate target
        subprocess.run(["taskkill.exe", "/f", "/im", "deeptrace_target.exe"],
                       capture_output=True)
        try:
            proc.kill()
        except Exception:
            pass

    print()
    print(f"passed={passed} failed={failed}")
    if failures:
        print("failures:", ", ".join(failures))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
