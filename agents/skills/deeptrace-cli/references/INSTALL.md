# deeptrace-cli 安装参考

> 顺序:**优先下载发布版本**;仅当发布版不存在或不可下载时,才从源码构建。目标:把 `deeptrace_cli.exe` 安装到**当前工作目录**。

## 1. 优先:下载发布版本并解压到当前目录

官方打包 zip 命名:`deeptrace_cli-<版本>-win64.zip`(Release 静态运行时,内含单个 `deeptrace_cli.exe`,免依赖)。当前发布版挂在 `v2.0.0` release 下。

先确认发布版存在(HTTP 200;404/网络失败 → 跳第 3 节源码构建):

```bash
VERSION=v2.1.0
curl -sI -f \
  "https://github.com/Theqiqi/DeepTrace/releases/download/v2.0.0/deeptrace_cli-$VERSION-win64.zip" \
  && echo "release available"
```

确认后下载并解压到**当前目录**:

```bash
curl -fL -o deeptrace_cli-$VERSION-win64.zip \
  "https://github.com/Theqiqi/DeepTrace/releases/download/v2.0.0/deeptrace_cli-$VERSION-win64.zip"
unzip -o deeptrace_cli-$VERSION-win64.zip     # 解压出 deeptrace_cli.exe 到当前目录
# Windows cmd/PowerShell(无 unzip):
# powershell -c "Expand-Archive -Path deeptrace_cli-$VERSION-win64.zip -DestinationPath ."
```

> 也可以在仓库内找到本地打包产物:`cli/out/dist/deeptrace_cli-<版本>-win64.zip`(若已存在,直接解压到当前目录,同样免构建)。

## 2. 验证安装

```bash
./deeptrace_cli.exe -v   # 应输出 deeptrace_cli v2.1.0
./deeptrace_cli.exe -h   # 应输出命令列表(12 组)
```

两条输出正常即安装完成,可进入[使用参考](USAGE.md)。

## 3. 回退:从源码构建

仅在无法下载发布版时执行。构建要求:Windows x64 + VS2022(MSVC)+ CMake≥3.24 + Ninja + vcpkg;WSL 环境用同名 `*_wsl.sh` 脚本(自动桥接 cmd.exe)。

```bash
# 顺序固定:先 deeptrace 静态库,后 cli(CLI 经 find_library 引用库产物)
deeptrace/script/build_release.bat       # Windows:库 Release(/MT 静态运行时)
cli/script/build_release.bat             # Windows:CLI Release
# WSL: deeptrace/script/build_release_wsl.sh && cli/script/build_release_wsl.sh

# 打包(默认版本 v1.3.0,建议显式传当前版本)
cli/script/package.bat v2.1.0            # 产物: cli/out/dist/deeptrace_cli-v2.1.0-win64.zip
```

三方引擎(keystone/capstone)已内置 `deeptrace/third_party/`,构建无需联网下载依赖。

## 4. 构建问题排查

- 编译/链接失败:见[开发者文档](../../../../docs/developers/v2.1.0/BUILDING.md)「常见编译问题」(如 LNK2038 运行库不匹配)。
- `.bat` 必须保持 CRLF 行尾;若在 WSL/Linux 编辑后解析错乱,先转回 CRLF。
