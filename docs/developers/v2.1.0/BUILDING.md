# Build Guide (BUILDING)

> Audience: developers new to the project. This document explains from scratch how to build both projects.
> For function-level API details see the [API Documentation](../../api/v2.1.0/README.md).

## 1. Environment Requirements

| Item | Requirement |
|------|-------------|
| OS | Windows 10/11 x64 (Windows x64 targets only) |
| Compiler | MSVC (cl.exe), provided by VS2022 (Community/Professional/Enterprise all work) |
| Build tools | CMake (≥3.24) + Ninja (bundled with VS2022: `Common7\IDE\CommonExtensions\Microsoft\CMake`) |
| Package manager | vcpkg (manifest mode; build scripts auto-locate the VS-bundled vcpkg or `VCPKG_ROOT`) |
| Third-party | No network download needed: keystone/capstone ship as source under `deeptrace/third_party/`; LLVM build needs Python, embedded at `deeptrace/third_party/python/` |
| WSL (optional) | WSL environments build through `*_wsl.sh` which bridges the Windows toolchain |

The build scripts handle everything automatically: vswhere locates VS → vcvars64 → vcpkg discovery → cmake configure+build. **No manual PATH setup required.**

## 2. Debug Build

Build order is fixed: **deeptrace first, then cli** (cli references deeptrace's build output via `find_library`).

### 2.1 Windows

```bat
deeptrace\script\build_debug.bat
cli\script\build_debug.bat
```

### 2.2 WSL

```bash
deeptrace/script/build_debug_wsl.sh
cli/script/build_debug_wsl.sh
```

The WSL scripts are equivalent to `cmd.exe /c script\build_debug.bat` and behave identically to Windows.

### 2.3 Artifact Verification

```
deeptrace/out/lib/Debug/deeptrace.lib      # static library
deeptrace/out/bin/Debug/deeptrace_target.exe  # test target program
deeptrace/out/bin/Debug/testdll.dll        # companion DLL for inject tests
cli/out/bin/Debug/deeptrace_cli.exe        # command-line main program
cli/out/bin/Debug/deeptrace_target.exe     # target program for cli e2e
```

Verify the build:

```bat
cli\out\bin\Debug\deeptrace_cli.exe -v
:: deeptrace_cli v2.1.0
```

## 3. Release Build

```bat
deeptrace\script\build_release.bat
cli\script\build_release.bat
```

- Release uses `/MT` (MultiThreaded static runtime) + vcpkg triplet `x64-windows-static`, producing a **single self-contained exe with no DLLs**, ready to distribute.
- Verify: `cli/out/bin/Release/deeptrace_cli.exe -v`

> Note: the first Release build compiles keystone's LLVM (X86 backend), which takes a while — this is expected.

## 4. Packaging (zip Archive)

```bat
cli\script\package.bat          :: default version v2.1.0
cli\script\package.bat v1.4     :: specify a version
```

Process: build deeptrace Release → build cli Release → collect `deeptrace_cli.exe` → zip. Output: `cli/out/dist/deeptrace_cli-<version>-win64.zip` (contains the exe plus LICENSE). Under WSL use `cli/script/package_wsl.sh [version]`.

## 5. Common Build Issues

| Symptom | Cause & Solution |
|---------|------------------|
| `LNK2038: runtime library mismatch` | Runtime library mismatch. Debug uses `/MDd`, Release uses `/MT`; both projects must link the same configuration; do not mix debug/release .lib files |
| `cs_disasm`/`cs_free`/`ks_*` unresolved symbols | deeptrace is a static library and does not bundle third-party deps; the CLI must explicitly link `keystone.lib` + `capstone.lib` (handled in cli/src/CMakeLists.txt — do not remove) |
| vcpkg download failure / SSL error | The build scripts set `GIT_SSL_NO_VERIFY=1`; or run `vcpkg install` first to install the gtest dependency |
| LLVM build reports python not found | `PYTHON_EXECUTABLE` should point to `deeptrace/third_party/python/python.exe` (set by build_debug.bat; set it yourself for manual cmake) |
| cmake configure reports stale cache | Delete `out/build/<config>/CMakeCache.txt` and rerun (keystone/capstone objects are reused, incremental build) |
| `.bat` commands garbled at runtime | Scripts must use CRLF line endings (enforced by `.gitattributes`; after editing under WSL, convert back with `sed -i 's/\r*$/\r/'`) |

## 6. Directory Conventions

```
<project>/out/
├── build/<config>/      CMake build directory (Ninja)
├── bin/<config>/        exe/dll artifacts
└── lib/<config>/        lib artifacts
```

The public header is referenced directly across projects: cli's include path points to `../../deeptrace/include` (no install intermediate layer).
