# DeepTrace — AI / Agent Setup Guide

> This is the document for **AI / AI agents** (custom agents, LLM tooling).
> Read it whenever you need to operate on process memory, debug, or disassemble with this repo.
> After reading, do these two things automatically: **① download and install `deeptrace_cli` into your current directory, ② download the two skills below into `.agents/` in your current directory.**

## 1. What this project is

Windows x64 C++20 process-memory tool, two parts:

| Project | Version | What it is |
|---|---|---|
| `deeptrace` (`deeptrace/`) | 2.13.0 | Static library: process / memory / module / thread / debug / disasm / asm / resolve / watch / inject / **script engine / hook / pointer-chain scan** — **74 public APIs** |
| `deeptrace_cli` (`cli/`) | 2.13.0 | CLI: **15 groups / 55 commands**, pure-ASCII output |

## 2. Do these two things

### ① Download and install the software

Download the release zip and extract it **into the current directory**:

```bash
curl -fL -o deeptrace_cli-v2.13.0-win64.zip \
  "https://github.com/Theqiqi/DeepTrace/releases/download/v2.13.0/deeptrace_cli-v2.13.0-win64.zip"
unzip -o deeptrace_cli-v2.13.0-win64.zip       # -> deeptrace_cli.exe in the current directory
./deeptrace_cli.exe -v                        # -> deeptrace_cli v2.13.0
```

> v2.13.0 is the latest published release and matches the repo code — full feature set (`script run`, `mem batch`, `resolve ptrscan`, `hex2bin`/`bin2hex` conversion layer) included. Build from source only when you need a feature newer than the latest release (Windows + VS2022/MSVC + CMake ≥ 3.24 + Ninja + vcpkg; from WSL use the `*_wsl.sh` scripts). See the install skill: [deeptrace-cli-install.md](deeptrace-cli-install.md).

### ② Download the skills into `.agents/`

Copy the two skills into `.agents/` in your current directory so they can be loaded:

```bash
mkdir -p .agents
cp agents/deeptrace-cli-install.md  .agents/
cp agents/deeptrace-cli-usage.md    .agents/
```

- [deeptrace-cli-install.md](deeptrace-cli-install.md) — install / build skill (Chinese): download, install, and build `deeptrace_cli`.
- [deeptrace-cli-usage.md](deeptrace-cli-usage.md) — usage skill (Chinese): command groups, examples, and the `debug run` script format.

## 3. Key facts

- Command format: `deeptrace_cli [options] <group> <action> [args...]`; most operations need `-p <pid>`.
- Exit codes: `0` success / `1` failure / `2` usage error.
- Addresses: `0x`-prefixed hex (e.g. `0x14000D000`), **or a script symbol name** (e.g. `mem read sunObjPtr` after `script run`; v2.6.0).
- State: watches / injection / script / hook records persist in `%TEMP%\deeptrace_<pid>\`; debug breakpoints exist only inside a `debug run` session.
- Debug single entry: `debug run <script.json>` (v2.1.0).
- Scripts: `script run <file.aa>` runs a CE-style `[ENABLE]` block (alloc / registersymbol / hook / createThread), `script disable` undoes it; both idempotent.
- `mem batch <read|write> <file.json>` executes JSON pointer-chain locators; `--format csv|json` exports results for other tools/AI (v2.9.0–v2.10.0).
- `resolve ptrscan <config.json>` finds pointer chains that reach a value; printed chains plug straight into `mem batch` (v2.12.0).
- Offline conversion ring: `asm file` → `hex2bin` → `bin2hex` / `disasm file` → `shellcode injectfile` (v2.2.0–v2.13.0).
- Test target `deeptrace_target.exe`: ASLR disabled, `0x14000D000` = `0x11223344`.

## 4. More

- User manual: [docs/users/v2.13.0/USER_MANUAL.md](../docs/users/v2.13.0/USER_MANUAL.md)
- API reference: [docs/api/v2.13.0/README.md](../docs/api/v2.13.0/README.md)
- Developer docs: [docs/developers/v2.13.0/README.md](../docs/developers/v2.13.0/README.md)
