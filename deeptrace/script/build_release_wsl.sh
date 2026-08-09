#!/bin/bash
# WSL -> cmd.exe -> .bat -> MSVC(cl.exe)
set -e
cd "$(dirname "$0")/.."
cmd.exe /c "script\\build_release.bat"
