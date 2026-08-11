# DeepTrace — AI / Agent Setup Guide

> This directory is documentation for **AI / AI agents** (Claude Code, Codex, Cursor, custom agents, LLM tools).
> Read it whenever you need to operate on process memory, debug, or disassemble with this repo.
> After reading, do the two setup steps: **① install `deeptrace_cli` into your working directory, ② download the skill into `.agents/`.**

## 1. What this project is

Windows x64 C++20 process-memory tool, two parts:

| Project | Version | What it is |
|---|---|---|
| `deeptrace` (`deeptrace/`) | 2.0.0 | Static library: process / memory / module / thread / debug / disasm / asm / resolve / watch / inject — **56 public APIs** |
| `deeptrace_cli` (`cli/`) | 2.1.0 | CLI: **12 groups / 40 commands**, pure-ASCII output |

## 2. Setup — do both steps

### ① Install `deeptrace_cli` into your working directory

Download the release zip and extract it **into the current directory**, so `deeptrace_cli.exe` is available where you work:

```bash
curl -fL -o deeptrace_cli-v2.1.0-win64.zip \
  "https://github.com/Theqiqi/DeepTrace/releases/download/v2.0.0/deeptrace_cli-v2.1.0-win64.zip"
unzip -o deeptrace_cli-v2.1.0-win64.zip       # -> deeptrace_cli.exe in the current directory
./deeptrace_cli.exe -v                        # -> deeptrace_cli v2.1.0
```

> No release available / download fails? Build from source (Windows + VS2022/MSVC + CMake ≥ 3.24 + Ninja + vcpkg; from WSL use the `*_wsl.sh` scripts): `deeptrace/script/build_release.bat` → `cli/script/build_release.bat` → `cli/script/package.bat v2.1.0`. Details: [references/INSTALL.md](skills/deeptrace-cli/references/INSTALL.md).

### ② Download the skill into `.agents/`

Copy the skill and its references into the runtime skill directory so your agent can load it:

```bash
mkdir -p .agents/skills/deeptrace-cli/references
cp agents/skills/deeptrace-cli/SKILL.md         .agents/skills/deeptrace-cli/
cp agents/skills/deeptrace-cli/references/*.md  .agents/skills/deeptrace-cli/references/
```

The skill (written in Chinese) tells the agent when and how to install and call the tool: [SKILL.md](skills/deeptrace-cli/SKILL.md).

## 3. Key facts (when calling the tool)

- Command format: `deeptrace_cli [options] <group> <action> [args...]`; most operations need `-p <pid>`.
- Exit codes: `0` success / `1` failure / `2` usage error.
- Addresses: `0x`-prefixed hex (e.g. `0x14000D000`).
- State: watches / injection records persist in `%TEMP%\deeptrace_<pid>\`; debug breakpoints exist only inside a `debug run` session.
- Debug single entry: `debug run <script.json>` (v2.1.0).
- Test target `deeptrace_target.exe`: ASLR disabled, `0x14000D000` = `0x11223344`.

## 4. More

- User manual: [docs/users/v2.1.0/USER_MANUAL.md](../docs/users/v2.1.0/USER_MANUAL.md)
- API reference: [docs/api/v2.1.0/README.md](../docs/api/v2.1.0/README.md)
- Developer docs: [docs/developers/v2.1.0/README.md](../docs/developers/v2.1.0/README.md)
