# DeepTrace Developer Documentation

> Audience: every developer who joins the project (newcomers / contributors / maintainers).
> For function-level API details see the [API Documentation](../../api/v1.3.0/README.md); not repeated here.

## Project Overview

This repository contains two independent Windows x64 C++20 projects:

- **deeptrace** (`deeptrace/`) — a process memory operation **static library**: process/memory/module/thread/debug/disassembly/assembly/resolve/watch/inject, 55 public APIs.
- **deeptrace_cli** (`cli/`) — a **command-line program**: wraps the library capabilities as commands (ps/mem/module/thread/debug/disasm/asm/resolve/watch/dll/shellcode), pure ASCII output.

## Directory Overview

```
deeptrace/   static library (four src layers: domain/algorithm/infrastructure/service + include/deeptrace.h)
cli/         command-line program (three src layers: command/interface/printing + main.cpp)
design/      design documents (v1.0.0 / v1.1.0 / v1.2.0)
docs/api/    public API reference documentation
docs/developers/  developer documentation (this doc set)
agents/      agent-facing install & usage prompts (for AI / LLM tools)
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
```

In a WSL environment use the corresponding `*_wsl.sh` scripts (they bridge to cmd.exe automatically).

## Documentation Map

| Document | Audience | Content |
|----------|----------|---------|
| [BUILDING.md](BUILDING.md) | Newcomers | Environment requirements, Debug/Release builds, WSL, packaging, common issues |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Newcomers / maintainers | Layered architecture, data flow, cross-project dependencies, state persistence |
| [TESTING.md](TESTING.md) | Contributors | Unit/integration/e2e tests, target program, writing new tests |
| [EXTENDING.md](EXTENDING.md) | Contributors | Extension guide: adding commands/APIs/algorithms/engines |
| [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) | Maintainers | Technical decision records (ADR) |
| [ANALYSIS.md](ANALYSIS.md) | Maintainers | Analysis phase output (code analysis/reader profiles/doc requirements) |
| [DESIGN.md](DESIGN.md) | Maintainers | Design phase output (structure design/example design/tech choices) |
| [CHANGELOG.md](CHANGELOG.md) | Maintainers | Documentation change history |
