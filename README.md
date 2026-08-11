<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%20x64-blue" alt="Platform: Windows x64"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B20-yellowgreen" alt="C++20"/>
  <img src="https://img.shields.io/badge/build-CMake%20%2B%20Ninja%20%2B%20MSVC-informational" alt="CMake + Ninja + MSVC"/>
  <img src="https://img.shields.io/badge/version-v2.1.0-blueviolet" alt="v2.1.0"/>
  <img src="https://img.shields.io/badge/API-56%20functions-green" alt="56 API functions"/>
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="License"/>
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

**`deeptrace` — static library (C++20, 56 public APIs)**

- **Process** — enumerate / attach / detach / suspend / resume / terminate
- **Memory** — read / write / dump / regions / typed value reads (byte…double)
- **Module** — list / find / base / exports / dump
- **Thread** — list / suspend / resume / terminate
- **Debug** — attach / pause / resume / step into & over, software + hardware + page-guard breakpoints, registers
- **Disassembly & Assembly** — x64 disassembly (Capstone), x64 assembly (Keystone)
- **Resolve** — module base, AOB pattern scanning (`??` wildcards)
- **Watch** — persistent variable watches with live values
- **Injection** — DLL and shellcode injection with status tracking
- **State persistence** — watches / injection records survive across CLI invocations (`%TEMP%\deeptrace_<pid>\`); debug breakpoints exist only inside a `debug run` session and are restored when it ends

**`deeptrace_cli` — command-line tool (40 commands across 12 groups, pure ASCII output)**

```
ps, mem, module, thread, debug, disasm, resolve, convert, watch, dll, asm, shellcode
```

- One-shot commands with stable exit codes (`0` success / `1` failure / `2` usage error)
- Non-interactive, script-friendly, fixed-width tables
- Scripted debug sessions — `debug run <script.json>` is the single debug entry (v2.1.0): one invocation = one complete session (breakpoints/page-guards auto-restored)

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
cli\script\package.bat v2.1.0
```

On WSL use the matching `*_wsl.sh` scripts (they bridge to `cmd.exe`).

## Testing

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe          :: 96 unit tests (hex/scan/disasm/asm/format)
deeptrace\out\bin\Debug\deeptrace_integration_test.exe   :: real-target integration
cli\out\bin\Debug\deeptrace_cli_unit_test.exe            :: parser/printer/executor
cli\out\bin\Debug\deeptrace_cli_integration_test.exe     :: full CLI pipeline
python3 cli/test/e2e/test_cli_e2e.py                     :: 104 end-to-end checks
```

## Repository Layout

```
deeptrace/   static library (domain / algorithm / infrastructure / service) + include/deeptrace.h
cli/         command-line tool (command / interface / printing layers)
design/      design documents (v1.0.0 … v2.1.0)
docs/        api reference, developer docs, user manual (v2.1.0)
agents/      AI-agent setup guide (agents/README.md) + two skills (agents/deeptrace-cli-install.md, agents/deeptrace-cli-usage.md)
sandbox/     experiments (not part of the deliverable)
```

## Documentation

| Doc | Audience | Content |
|-----|----------|---------|
| [API Reference](docs/api/v2.1.0/README.md) | Developers integrating the library | All 56 functions, types, error codes |
| [Developer Docs](docs/developers/v2.1.0/README.md) | Contributors / maintainers | Architecture, building, testing, extending, ADRs |
| [User Manual](docs/users/v2.1.0/README.md) | End users | Getting started, command reference, FAQ, troubleshooting |

## AI / Agent Usage Guide

> **This section is an index for AI and AI agents** (custom agents, LLM tooling, etc.). If you are an AI reading this README, follow the steps below: **1) read the agent doc, 2) install the tool into your working directory, 3) download the skill into `.agents/`, 4) use the skill to call the tool.**

**Step 1 — Read the agent doc.** [agents/README.md](agents/README.md) is the setup guide for AI agents (written in English): project overview, key facts, and the two setup actions — install `deeptrace_cli` into your working directory and download the skill into `.agents/`.

**Step 2 — Install the tool into your working directory (prefer the release download).** This repo is a Windows x64 C++20 process-memory tool. Download `deeptrace_cli-v2.1.0-win64.zip` from the [v2.0.0 release](https://github.com/Theqiqi/DeepTrace/releases/tag/v2.0.0) (single exe, no dependencies) and extract it into the **current directory** so `deeptrace_cli.exe` is available; build from source only when no release is available (build order: `deeptrace` library first, then `deeptrace_cli`). Details: [agents/README.md](agents/README.md) or the install skill [deeptrace-cli-install.md](agents/deeptrace-cli-install.md).

**Step 3 — Download the skills into `.agents/`.** Copy the two skills into `.agents/` in your current directory: `agents/deeptrace-cli-install.md` (install/build) and `agents/deeptrace-cli-usage.md` (usage), both written in Chinese.

**Step 4 — Use the skill to call the tool.** The agent can run commands such as `deeptrace_cli -p <pid> mem read 0x14000D000 4 hex`. Key facts: exit codes `0`/`1`/`2` (success / failure / usage error); addresses are `0x`-prefixed hex; state (watches/injections) persists in `%TEMP%\deeptrace_<pid>\`; debug is scripted via the single entry `debug run <script.json>` (v2.1.0); test target `deeptrace_target.exe` has ASLR disabled with known values at fixed addresses (e.g. `0x14000D000` = `0x11223344`).

Additional references: [User Manual](docs/users/v2.1.0/USER_MANUAL.md) (full command reference), [API Reference](docs/api/v2.1.0/README.md) (library API for code integration).

## License

Distributed under the [MIT License](LICENSE). Copyright (c) 2026 Theqiqi.
