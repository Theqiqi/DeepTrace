# DeepTrace Developer Documentation

> Audience: every developer who joins the project (newcomers / contributors / maintainers). For function-level API details see the [API Documentation](../../api/v2.13.0/README.md); not repeated here.

## Project Overview

This repository contains two independent Windows x64 C++20 projects:

- **deeptrace** (`deeptrace/`) — a process memory operation **static library**: process/memory/module/thread/debug/disassembly/assembly/resolve/watch/inject + **script engine (AA-style: named allocs/hooks/enable-state)** + **pointer-chain scan**, 74 public APIs.
- **deeptrace_cli** (`cli/`) — a **command-line program**: wraps the library capabilities as commands (ps/mem/module/thread/debug/disasm/asm/resolve/watch/dll/shellcode/**script**/**batch**/**ptrscan**/**bin2hex**), pure ASCII output. Since v2.1.0 the debug group exposes a **single entry `debug run <script.json>`**; since v2.3.0 a **`script run <file.aa>`** engine executes CE-style scripts; since v2.13.0 the conversion layer (asm↔bin↔hex) is closed with `disasm file` + `bin2hex`.

## Directory Overview

```
deeptrace/   static library (four src layers: domain/algorithm/infrastructure/service + include/deeptrace.h)
cli/         command-line program (three src layers: command/interface/printing + main.cpp)
design/      design documents (v1.0.0 … v2.13.0)
docs/api/    public API reference documentation
docs/developers/  developer documentation (this doc set)
agents/      AI-agent setup guide (agents/README.md) + two skills (agents/deeptrace-cli-install.md, agents/deeptrace-cli-usage.md)
sandbox/     experimental verification project (not part of the deliverable)
```

## Quick Start

```bat
:: 1. Build the deeptrace static library (Debug)
deeptrace\script\build_debug.bat
:: 2. Build deeptrace_cli (Debug)
cli\script\build_debug.bat
:: 3. Run
cli\out\bin\Debug\deeptrace_cli.exe -h
cli\out\bin\Debug\deeptrace_cli.exe -p <pid> mem read <address> 4 hex
cli\out\bin\Debug\deeptrace_cli.exe -p <pid> script run cheat.aa
```

In a WSL environment use the corresponding `*_wsl.sh` scripts (they bridge to cmd.exe automatically).

## Documentation Map

| Document | Audience | Content |
|----------|----------|---------|
| [BUILDING.md](BUILDING.md) | Newcomers | Environment requirements, Debug/Release builds, WSL, packaging, common issues |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Newcomers / maintainers | Layered architecture, data flow, cross-project dependencies, state persistence |
| [TESTING.md](TESTING.md) | Contributors | Unit/integration/e2e tests, target program, writing new tests |
| [EXTENDING.md](EXTENDING.md) | Contributors | Extension guide: adding commands/APIs/algorithms/engines/script keywords |
| [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) | Maintainers | Technical decision records (ADR) |
| [ANALYSIS.md](ANALYSIS.md) | Maintainers | Analysis phase output (code analysis/reader profiles/doc requirements) |
| [DESIGN.md](DESIGN.md) | Maintainers | Design phase output (structure design/example design/tech choices) |
| [CHANGELOG.md](CHANGELOG.md) | Maintainers | Documentation change history |
