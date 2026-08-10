# Developer Documentation — Design Phase (v1.3.0)

> This file is the output of stage 2 of `.flow/developer_docs_development_process.md`:
> 2.1 Documentation structure design
> 2.2 Example design
> 2.3 Technology choices

---

## 2.1 Documentation Structure Design

### 2.1.1 Directory Structure

```
docs/developers/v1.3.0/
├── README.md               # project intro + quick start (main entry, ≤50 lines)
├── BUILDING.md             # build guide
├── ARCHITECTURE.md         # architecture overview
├── TESTING.md              # testing guide
├── EXTENDING.md            # extension guide
├── DESIGN_DECISIONS.md     # technical decision records (ADR)
├── ANALYSIS.md             # analysis phase output (code analysis/readers/requirements)
├── DESIGN.md               # this file (design phase output)
└── CHANGELOG.md            # documentation version change history
```

### 2.1.2 Per-Document Section Planning

**README.md** (≤50 lines)
1. Project overview (one line each for the deeptrace library and deeptrace_cli)
2. Directory overview
3. Quick start (2 build commands + 1 run command)
4. Documentation map (links to all documents)

**BUILDING.md**
1. Environment requirements (Windows x64 / VS2022 MSVC / CMake+Ninja / vcpkg / WSL optional)
2. Debug build (deeptrace → cli, with artifact verification)
3. Release build (same, /MT explanation)
4. WSL build bridge
5. Packaging (zip archive)
6. Common build issues (LNK2038 runtime mismatch, vcpkg SSL, keystone LLVM python, etc.)

**ARCHITECTURE.md**
1. Overview diagram (deeptrace four layers + cli three layers)
2. deeptrace layer explanation (per-layer responsibilities + forbidden items + dependency direction)
3. cli layer explanation (per-layer responsibilities + forbidden items + dependency direction)
4. Data flow (the full chain from argv to output)
5. Cross-project dependencies (find_library references)
6. State persistence (state files)
7. Session lifecycle (attach/detach, debug attach/detach)

**TESTING.md**
1. Test system overview table
2. Running unit tests
3. Running integration tests (needs target)
4. Running e2e (needs Debug build + testdll.dll)
5. Target program explanation (ASLR disabled, etc.)
6. Writing new tests (templates + requirements)

**EXTENDING.md**
1. Extension point overview (command layer/API layer/algorithm layer/engine layer)
2. Adding a new CLI command (complete example + expected output)
3. Adding a new public API (layer-by-layer change checklist)
4. Adding a new algorithm (pure computation + unit tests)
5. Replacing engines (keystone/capstone precedent)
6. Testing requirements

**DESIGN_DECISIONS.md**
1. Why a static library + CLI two-project setup
2. Why four-layer / three-layer layering
3. Why Keystone / Capstone (source-built rather than vcpkg)
4. Why Debug=/MDd, Release=/MT
5. Why state files persist to %TEMP%
6. Why the target has ASLR disabled
7. Why the static library does not merge dependencies (consumers link explicitly)
8. Why cs_disasm and not cs_disasm_iter (crash pitfall)

### 2.1.3 Cross-Reference Planning (Reachable Both Ways)

- README → all documents (main entry)
- BUILDING/ARCHITECTURE/TESTING/EXTENDING/DESIGN_DECISIONS ↔ cross-link related sections
- Function-level explanations → link `docs/api/v1.3.0/` (Modules/*.md), not duplicated here
- Each document states its target audience at the top

---

## 2.2 Example Design

| Example | Where it lives | Form | Verification |
|---------|----------------|------|--------------|
| Build and run from scratch | README / BUILDING | command sequence | build artifacts exist + CLI runs |
| Adding the `ps list2` command | EXTENDING | code snippet + expected output | compared against existing cmd_process.cpp, logically equivalent |
| Adding a new public API | EXTENDING | layer-by-layer change checklist | compared against existing APIs |
| Adding a new algorithm | EXTENDING | code skeleton | follows algorithm/scan.h style |
| A complete program calling the library | linked to API docs | existing compilable examples | docs/api/v1.3.0/Examples/src/ (already compiled and verified) |

Constraint: build-style examples must be real compilable code, not pseudocode; every embedded command has a verification record.

---

## 2.3 Technology Choices

| Item | Choice | Rationale |
|------|--------|-----------|
| Documentation format | Markdown | consistent with existing docs in design/, .flow/, docs/api/; readable in IDEs/browsers/Code |
| Code blocks | fenced code blocks with language tags | syntax highlighting, copy-friendly |
| Naming | English file names, English content | consistent with docs/api/, filesystem-friendly |
| Function descriptions | link to API docs | avoid duplication, single source of truth |
| Architecture diagrams | ASCII/Mermaid mix | renders in plain Markdown, no external dependencies |

No mixed formats; Markdown throughout.
