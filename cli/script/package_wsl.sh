#!/bin/bash
# WSL -> cmd.exe -> package.bat:构建 Release 并打包 deeptrace_cli.zip
# 用法:bash package_wsl.sh [版本]    (默认 v1.3.0)
set -e
cd "$(dirname "$0")/.."
VERSION="${1:-v1.3.0}"
cmd.exe /c "script\\package.bat $VERSION"
