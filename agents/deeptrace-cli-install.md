# deeptrace-cli 安装技能(下载 / 安装 / 编译)

> 目标:把 `deeptrace_cli.exe` 安装到**当前工作目录**。顺序:**优先下载发布版**;仅当发布版不存在或不可下载时,才从源码编译。
>
> 当前仓库代码与最新已发布版本同步(**v2.13.0**,2026-08-12 发布),下载发布版即得完整功能(脚本引擎/`mem batch`/`resolve ptrscan`/`hex2bin`/`bin2hex` 等全部包含)。

## 1. 优先:下载发布版并解压到当前目录

发布版 zip 挂在仓库 [GitHub Releases](https://github.com/Theqiqi/DeepTrace/releases) 下(文件名 `deeptrace_cli-<版本>-win64.zip`,内含单个 `deeptrace_cli.exe`,Release 静态运行时,免依赖)。

```bash
# 优先 v2.13.0(已发布,与仓库代码同步);404 → 自动查最新已发布版本
VERSION=v2.13.0
if curl -sI -f -m 10 \
  "https://github.com/Theqiqi/DeepTrace/releases/download/$VERSION/deeptrace_cli-$VERSION-win64.zip" >/dev/null 2>&1; then
  echo "release $VERSION available"
else
  VERSION=$(curl -fsSL https://api.github.com/repos/Theqiqi/DeepTrace/releases/latest \
    | grep '"tag_name"' | head -1 | sed 's/.*: "//;s/".*//')
  echo "fallback to latest release $VERSION"
fi

# 下载并解压到当前目录
curl -fL -o deeptrace_cli-$VERSION-win64.zip \
  "https://github.com/Theqiqi/DeepTrace/releases/download/$VERSION/deeptrace_cli-$VERSION-win64.zip"
unzip -o deeptrace_cli-$VERSION-win64.zip     # 解压出 deeptrace_cli.exe 到当前目录
# Windows cmd/PowerShell 无 unzip 时:
# powershell -c "Expand-Archive -Path deeptrace_cli-$VERSION-win64.zip -DestinationPath ."

# 验证
./deeptrace_cli.exe -v   # 应输出 deeptrace_cli v2.13.0
./deeptrace_cli.exe -h   # 应输出命令列表(15 组 / 55 命令)
```

> ⚠️ 若 `-h` 命令组少于 15(如 12 组,无 script/batch/ptrscan/hex2bin/bin2hex),说明拿到的是旧发布版 → 重新下载最新发布版或改用第 2 节源码编译。
> 也可以在仓库内找到本地打包产物:`cli/out/dist/deeptrace_cli-<版本>-win64.zip`(若已存在,直接解压到当前目录,同样免构建)。

## 2. 回退:从源码编译

仅在无法下载发布版、或发布版功能不足时执行。要求:Windows x64 + VS2022(MSVC)+ CMake≥3.24 + Ninja + vcpkg;WSL 环境用同名 `*_wsl.sh` 脚本(自动桥接 cmd.exe)。

```bash
# 顺序固定:先 deeptrace 静态库,后 cli(CLI 经 find_library 引用库产物)
deeptrace/script/build_release.bat       # Windows:库 Release(/MT 静态运行时)
cli/script/build_release.bat             # Windows:CLI Release
# WSL: deeptrace/script/build_release_wsl.sh && cli/script/build_release_wsl.sh

# 打包(默认版本 v1.3.0,建议显式传当前版本)
cli/script/package.bat v2.13.0           # 产物: cli/out/dist/deeptrace_cli-v2.13.0-win64.zip
```

三方引擎(keystone/capstone)已内置 `deeptrace/third_party/`,编译无需联网下载依赖。

## 3. 问题排查

- 编译/链接失败:见[开发者文档](../docs/developers/v2.13.0/BUILDING.md)「常见编译问题」(如 LNK2038 运行库不匹配)。
- `.bat` 必须保持 CRLF 行尾;若在 WSL/Linux 编辑后解析错乱,先转回 CRLF。
