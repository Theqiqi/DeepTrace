#!/usr/bin/env python3
"""deeptrace_cli e2e tests - independent Python system (not part of CMake).

Starts deeptrace_target.exe, then drives the real deeptrace_cli.exe binary via
cmd.exe /c and asserts on stdout/exit codes.

Run from the project root:
    python3 cli/test/e2e/test_cli_e2e.py

All tests passing -> exit 0; any failure -> exit 1.
"""

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
    check("version string", "deeptrace_cli v1.4.0" in out, repr(out))

    # ---- unknown command ----
    code, _, err = run_cli(["bogus", "cmd"])
    check("unknown group exit 2", code == 2)
    check("unknown group msg", "unknown command group" in err, repr(err))

    # ---- open nonexistent process ----
    code, _, err = run_cli(["ps", "attach", "99999999"])
    check("attach missing process exit 1", code == 1)

    # ---- invalid arguments ----
    code, _, err = run_cli(["mem", "read", "zzz"])
    check("invalid address exit 2", code == 2)

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

        # ---- resolve scan by typed value: g_int = dword 0x11223344 (LE bytes 44 33 22 11) ----
        code, out, _ = run_cli(["-p", str(pid), "resolve", "scan", "287454020", "dword"])
        check("resolve scan dword value exit 0", code == 0)
        want_int = g_int.lower().lstrip("0x").lstrip("0").lstrip("x")
        got_int = [h.lower().lstrip("0x").lstrip("0") for h in out.split()]
        check("resolve scan dword value finds g_int",
              any(want_int == g or want_int in g for g in got_int), repr(out[:300]))

        # ---- resolve scan by float value: g_float = 3.14159f ----
        code, out, _ = run_cli(["-p", str(pid), "resolve", "scan", "3.14159", "float"])
        check("resolve scan float value exit 0", code == 0)

        # ---- resolve scan by string value ----
        code, out, _ = run_cli(["-p", str(pid), "resolve", "scan", "hi", "string"])
        check("resolve scan string value exit 0", code == 0)

        # ---- resolve scan invalid type/value -> usage error exit 2 ----
        code, _, err = run_cli(["-p", str(pid), "resolve", "scan", "100", "bogus"])
        check("resolve scan invalid type exit 2", code == 2)
        check("resolve scan invalid type msg", "invalid type" in err, repr(err))
        code, _, err = run_cli(["-p", str(pid), "resolve", "scan", "xyz", "dword"])
        check("resolve scan invalid value exit 2", code == 2)
        check("resolve scan invalid value msg", "invalid value for type 'dword'" in err,
              repr(err))

        # ---- thread ----
        code, out, _ = run_cli(["-p", str(pid), "thread", "list"])
        check("thread list exit 0", code == 0)

        # ---- registers ----
        code, out, _ = run_cli(["-p", str(pid), "debug", "registers"])
        check("registers exit 0", code == 0)
        check("registers have rip", re.search(r"rip\s", out) is not None,
              repr(out[:200]))

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

        # ---- debug break / status ----
        code, out, _ = run_cli(["-p", str(pid), "debug", "status"])
        check("debug status exit 0", code == 0)
        code, _, _ = run_cli(["-p", str(pid), "debug", "break", g_int])
        check("debug break exit 0", code == 0)
        run_cli(["-p", str(pid), "debug", "clear", g_int])
        check("debug clear exit 0", code == 0)

        # ---- debug attach must not kill the target ----
        code, _, _ = run_cli(["-p", str(pid), "debug", "attach"])
        check("debug attach exit 0", code == 0)
        code, out, _ = run_cli(["ps", "list"])
        check("target alive after debug attach", str(pid) in out, repr(out[:200]))

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
