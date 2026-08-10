<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%20x64-blue" alt="Platform: Windows x64"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B20-yellowgreen" alt="C++20"/>
  <img src="https://img.shields.io/badge/build-CMake%20%2B%20Ninja%20%2B%20MSVC-informational" alt="CMake + Ninja + MSVC"/>
  <img src="https://img.shields.io/badge/version-v1.3-blueviolet" alt="v1.3"/>
  <img src="https://img.shields.io/badge/API-55%20functions-green" alt="55 API functions"/>
  <img src="https://img.shields.io/badge/license-none%20yet-lightgrey" alt="License"/>
</p>

# DeepTrace

**A Windows process memory tool: a static library (`deeptrace`) plus a command-line front-end (`deeptrace_cli`) for inspecting and modifying running processes.**

Read this in: [English](README.md) | [简体中文](README.zh-CN.md)

DeepTrace lets you enumerate processes, read/write process memory, inspect modules and exports, control threads, set breakpoints and single-step under a debugger, disassemble code, assemble instructions, scan for byte patterns (AOB), watch variables, and inject DLLs or shellcode — all from a single portable executable.

```
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
44 33 22 11
```

## Features

**`deeptrace` — static library (C++20, 55 public APIs)**

- **Process** — enumerate / attach / detach / suspend / resume / terminate
- **Memory** — read / write / dump / regions / typed value reads (byte…double)
- **Module** — list / find / base / exports / dump
- **Thread** — list / suspend / resume / terminate
- **Debug** — attach / pause / resume / step into & over, software + hardware + page-guard breakpoints, registers
- **Disassembly & Assembly** — x64 disassembly (Capstone), x64 assembly (Keystone)
- **Resolve** — module base, AOB pattern scanning (`??` wildcards)
- **Watch** — persistent variable watches with live values
- **Injection** — DLL and shellcode injection with status tracking
- **State persistence** — breakpoints / watches / injections survive across CLI invocations (`%TEMP%\deeptrace_<pid>\`)

**`deeptrace_cli` — command-line tool (53 commands, pure ASCII output)**

```
ps, mem, module, thread, debug, disasm, resolve, watch, dll, asm, shellcode
```

- One-shot commands with stable exit codes (`0` success / `1` failure / `2` usage error)
- Non-interactive, script-friendly, fixed-width tables

## Quick Start

### Download

Grab a release zip (`deeptrace_cli-<version>-win64.zip`) — it contains a single portable `deeptrace_cli.exe` (static runtime, no DLLs needed).

Or build from source (see [Building](#building)).

### Try it

```bat
:: show help / version
deeptrace_cli -h
deeptrace_cli -v

:: list running processes
deeptrace_cli ps list

:: read 4 bytes of a process at an address (hex)
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex

:: read a typed value
deeptrace_cli -p 1234 mem readval 0x14000D000 dword
0x11223344

:: assemble instructions to bytes
deeptrace_cli asm assemble "nop; ret"
90C3
```

> The address `0x14000D000` and value `0x11223344` above come from the test target `deeptrace_target.exe` (ASLR disabled, fixed base). The test target ships only in the Debug build (`cli/out/bin/Debug/`) — practice on it before touching real programs.

## Building

Requires: **Windows 10/11 x64**, **Visual Studio 2022** (MSVC), **CMake ≥ 3.24**, **Ninja**, **vcpkg**. Keystone/Capstone sources are vendored under `deeptrace/third_party/` — no network needed.

```bat
:: 1. build the deeptrace static library (Debug)
deeptrace\script\build_debug.bat

:: 2. build deeptrace_cli (Debug)
cli\script\build_debug.bat

:: release builds (/MT static runtime)
deeptrace\script\build_release.bat
cli\script\build_release.bat

:: package a zip into cli\out\dist\
cli\script\package.bat v1.3
```

On WSL use the matching `*_wsl.sh` scripts (they bridge to `cmd.exe`).

## Testing

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe          :: 96 unit tests (hex/scan/disasm/asm/format)
deeptrace\out\bin\Debug\deeptrace_integration_test.exe   :: real-target integration
cli\out\bin\Debug\deeptrace_cli_unit_test.exe            :: parser/printer/executor
cli\out\bin\Debug\deeptrace_cli_integration_test.exe     :: full CLI pipeline
python3 cli/test/e2e/test_cli_e2e.py                     :: 47 end-to-end checks
```

## Repository Layout

```
deeptrace/   static library (domain / algorithm / infrastructure / service) + include/deeptrace.h
cli/         command-line tool (command / interface / printing layers)
design/      design documents (v1.0 / v1.1 / v1.2)
docs/        api reference, developer docs, user manual (v1.3)
sandbox/     experiments (not part of the deliverable)
```

## Documentation

| Doc | Audience | Content |
|-----|----------|---------|
| [API Reference](docs/api/v1.3/README.md) | Developers integrating the library | All 55 functions, types, error codes |
| [Developer Docs](docs/developers/v1.3/README.md) | Contributors / maintainers | Architecture, building, testing, extending, ADRs |
| [User Manual](docs/users/v1.3/README.md) | End users | Getting started, command reference, FAQ, troubleshooting |

## AI / Agent Usage Guide

> **This section is an index for AI and AI agents** (Claude Code, Codex, Cursor, custom agents, LLM tools, etc.). If you are an AI reading this README, follow the steps below: **1) install the software, 2) install the prompts, 3) use the prompts to call the tool.**

**Step 1 — Install the software.** This repo is a Windows x64 C++20 process-memory tool. `deeptrace_cli` must be built (or unpacked from a release zip) before it can be used. See the [install prompt](docs/agents/deeptrace-cli-install.md) for agent-facing install/verify instructions (build order: `deeptrace` library first, then `deeptrace_cli`).

**Step 2 — Install the prompts.** This repo ships two agent prompts in standard SKILL format (YAML frontmatter + instructions). Load them into your agent so it knows when and how to use this tool:

| Prompt | Purpose | File |
|--------|---------|------|
| **Install prompt** | Guides the agent to build/verify `deeptrace_cli` before use | [docs/agents/deeptrace-cli-install.md](docs/agents/deeptrace-cli-install.md) |
| **Usage prompt** | Commands & workflows for calling `deeptrace_cli` (process memory, debug, disasm/asm, inject, etc.) | [docs/agents/deeptrace-cli-usage.md](docs/agents/deeptrace-cli-usage.md) |

**Step 3 — Use the prompts to call the tool.** With the usage prompt loaded, the agent can run commands such as `deeptrace_cli -p <pid> mem read 0x14000D000 4 hex`. Key facts: exit codes `0`/`1`/`2` (success / failure / usage error); addresses are `0x`-prefixed hex; state (breakpoints/watches/injections) persists in `%TEMP%\deeptrace_<pid>\`; test target `deeptrace_target.exe` has ASLR disabled with known values at fixed addresses (e.g. `0x14000D000` = `0x11223344`).

Additional references: [User Manual](docs/users/v1.3/USER_MANUAL.md) (full command reference), [API Reference](docs/api/v1.3/README.md) (library API for code integration).

## License

Not yet specified. See [issue tracker](https://github.com/Theqiqi/DeepTrace/issues) if you have licensing questions.
