# DeepTrace — AI / Agent Documentation

> This directory is documentation for **AI / AI agents** (Claude Code, Codex, Cursor, custom agents, LLM tooling):
> read it first whenever you need to operate on process memory, debug, or disassemble in this repo, or need to build/invoke `deeptrace_cli`.
> The actionable **skill** lives at [`agents/skills/deeptrace-cli/SKILL.md`](../agents/skills/deeptrace-cli/SKILL.md) — load it, then install and call the tool step by step.

## 1. What this project is

The repository contains two independent Windows x64 C++20 projects:

| Project | Version | Description |
|---------|---------|-------------|
| **deeptrace** (`deeptrace/`) | 2.0.0 | Process-memory **static library**: process / memory / module / thread / debug / disasm / asm / resolve / watch / inject, **56 public APIs** |
| **deeptrace_cli** (`cli/`) | 2.1.0 | **Command-line program**: wraps the library into commands (`ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode`), pure-ASCII output |

Version convention: three-part tag (`v2.1.0` is current); `deeptrace_cli -v` prints `deeptrace_cli v2.1.0`.

## 2. Install (prefer the release download)

> Unlike the old "build from source" habit: **prefer downloading the official release**; build only as a fallback.

1. Download the packaged zip from GitHub Releases (version = latest repo tag, currently `v2.1.0`):
   `https://github.com/Theqiqi/DeepTrace/releases/download/v2.1.0/deeptrace_cli-v2.1.0-win64.zip`
   Unzip to get a single `deeptrace_cli.exe` (Release static runtime, no dependencies).
2. Verify: `deeptrace_cli.exe -v` (prints `deeptrace_cli v2.1.0`) + `-h` (command list).
3. Only if no release is available, build from source (needs Windows + VS2022/MSVC + CMake≥3.24 + Ninja + vcpkg):
   `deeptrace/script/build_release.bat` → `cli/script/build_release.bat` → `cli/script/package.bat v2.1.0`.

Full steps: the skill's [`references/INSTALL.md`](../agents/skills/deeptrace-cli/references/INSTALL.md).

## 3. Key facts (read before calling)

- Command format: `deeptrace_cli [options] <group> <action> [args...]`; most operations need `-p <pid>` to target a process.
- Exit codes: `0` success / `1` execution failure / `2` usage error.
- Addresses: hex with `0x` prefix (e.g. `0x14000D000`), 64-bit fixed width.
- State persistence: `%TEMP%\deeptrace_<pid>\` (watch/injection records persist across commands); debug breakpoints exist only inside a script session.
- **Debug has a single entry: `debug run <script.json>`** (v2.1.0): one invocation = one complete debug session; the JSON step array fully covers debug capabilities; the other standalone debug commands (step/break/registers etc.) were removed — calling them reports `unknown command`.
- Test target `deeptrace_target.exe`: ASLR disabled, fixed address `0x14000D000` holds `0x11223344` (for practice).

## 4. Command group overview

| Group | Purpose | Common actions |
|-------|---------|----------------|
| `ps` | Process | `list` / `attach` / `detach` / `info` / `suspend` / `resume` / `kill` |
| `mem` | Memory | `read` / `write` / `dump` / `regions` / `readval` |
| `module` | Module | `list` / `find` / `base` / `exports` / `dump` |
| `thread` | Thread | `list` / `suspend` / `resume` / `kill` |
| `debug` | Debug | **`run <script.json>`** (single entry) |
| `disasm` | Disassembly | `at <addr> [n]` / `range <a> <b>` |
| `resolve` | Resolve | `base <mod>` / `scan "<pattern>"` (AOB, `??` wildcard) |
| `convert` | Data conversion | `<type> <value>` → hex bytes (for scan; type: byte/word/dword/qword/float/double/string/hex) |
| `watch` | Watch | `add` / `list` / `remove` / `refresh` / `clear` |
| `dll` | DLL injection | `inject` / `eject` / `list` / `status` |
| `asm` | Assembly | `assemble "<code>" [--hex] [--c-array]` |
| `shellcode` | Injection | `inject` / `injectat` / `status` |

## 5. Documentation map (v2.1.0)

| Doc | Audience | Content |
|-----|----------|---------|
| [User manual](../docs/users/v2.1.0/USER_MANUAL.md) | Users / AI | All command usage, `debug run` script format and examples, FAQ, troubleshooting |
| [API reference](../docs/api/v2.1.0/README.md) | Integrators / AI | 56 public APIs, types, error codes, examples |
| [Developer docs](../docs/developers/v2.1.0/README.md) | Developers / AI | Architecture, building, testing, extending, ADRs |

## 6. Skill

Loadable skill: `agents/skills/deeptrace-cli/SKILL.md` (standard SKILL.md format: YAML frontmatter `name`/`description` + install/use/reference; written in Chinese). After loading, the agent will: prefer the release download → verify → call per the command table. References: `references/INSTALL.md` (install) and `references/USAGE.md` (command reference + `debug run` script format).
