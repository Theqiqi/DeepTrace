# 编译指南(BUILDING)

> 目标读者:新入项目开发者。本文档从零开始说明如何编译两个项目。
> 函数级 API 说明见 [API 文档](../../api/v1.3/README.md)。

## 1. 环境要求

| 项 | 要求 |
|----|------|
| 操作系统 | Windows 10/11 x64(仅 Windows x64 目标) |
| 编译器 | MSVC(cl.exe),由 VS2022 提供(Community/Professional/Enterprise 均可) |
| 构建工具 | CMake(≥3.24)+ Ninja(VS2022 自带:`Common7\IDE\CommonExtensions\Microsoft\CMake`) |
| 包管理 | vcpkg(manifest 模式;构建脚本自动查找 VS 自带 vcpkg 或 `VCPKG_ROOT`) |
| 第三方 | 无需联网下载:keystone/capstone 已以源码形式放在 `deeptrace/third_party/`;LLVM 构建需要 Python,已内嵌于 `deeptrace/third_party/python/` |
| WSL(可选) | WSL 环境通过 `*_wsl.sh` 桥接 Windows 工具链 |

构建脚本自动完成:vswhere 定位 VS → vcvars64 → 定位 vcpkg → cmake configure+build。**无需手动配置 PATH。**

## 2. Debug 构建

构建顺序固定:**先 deeptrace,后 cli**(cli 通过 `find_library` 引用 deeptrace 的构建产物)。

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

WSL 脚本 = `cmd.exe /c script\build_debug.bat`,与 Windows 行为完全一致。

### 2.3 产物验证

```
deeptrace/out/lib/Debug/deeptrace.lib      # 静态库
deeptrace/out/bin/Debug/deeptrace_target.exe  # 测试目标程序
deeptrace/out/bin/Debug/testdll.dll        # 注入测试伴生 DLL
cli/out/bin/Debug/deeptrace_cli.exe        # 命令行主程序
cli/out/bin/Debug/deeptrace_target.exe     # cli e2e 用目标程序
```

验证运行:

```bat
cli\out\bin\Debug\deeptrace_cli.exe -v
:: deeptrace_cli v1.0.0
```

## 3. Release 构建

```bat
deeptrace\script\build_release.bat
cli\script\build_release.bat
```

- Release 使用 `/MT`(MultiThreaded 静态运行时)+ vcpkg triplet `x64-windows-static`,
  产物为**单文件免 DLL**,可直接分发。
- 产物验证:`cli/out/bin/Release/deeptrace_cli.exe -v`

> 注意:首次 Release 构建需编译 keystone 的 LLVM(X86 后端),耗时较长,属正常现象。

## 4. 打包(zip 归档)

```bat
cli\script\package.bat          :: 默认版本 v1.3
cli\script\package.bat v1.4     :: 指定版本
```

流程:构建 deeptrace Release → 构建 cli Release → 收集 `deeptrace_cli.exe` → 打 zip。
产物:`cli/out/dist/deeptrace_cli-<版本>-win64.zip`(内含唯一 exe)。
WSL 下用 `cli/script/package_wsl.sh [版本]`。

## 5. 常见编译问题

| 现象 | 原因与解决 |
|------|-----------|
| `LNK2038: runtime library mismatch` | 运行库不匹配。Debug 用 `/MDd`、Release 用 `/MT`,两项目必须同配置链接;确认未混用 debug/release 的 .lib |
| `cs_disasm`/`cs_free`/`ks_*` 链接未解析 | deeptrace 是静态库,不合并三方依赖;CLI 必须显式链接 `keystone.lib` + `capstone.lib`(cli/src/CMakeLists.txt 已处理,勿删除) |
| vcpkg 下载失败 / SSL 错误 | 构建脚本已设 `GIT_SSL_NO_VERIFY=1`;或先 `vcpkg install` 安装 gtest 依赖 |
| LLVM 构建报找不到 python | `PYTHON_EXECUTABLE` 指向 `deeptrace/third_party/python/python.exe`(build_debug.bat 已设置;手动 cmake 时需自行设置) |
| cmake configure 报陈旧缓存 | 删除 `out/build/<配置>/CMakeCache.txt` 后重跑(keystone/capstone 对象会复用,增量构建) |
| `.bat` 运行时报命令错乱 | 脚本必须 CRLF 行尾(`.gitattributes` 已强制;在 WSL 编辑后先 `sed -i 's/\r*$/\r/'` 转回) |

## 6. 目录约定

```
<项目>/out/
├── build/<配置>/      CMake 构建目录(Ninja)
├── bin/<配置>/        exe/dll 产物
└── lib/<配置>/        lib 产物
```

公共头跨项目直接引用:`cli` 的 include 路径指向 `../../deeptrace/include`(无 install 中间层)。
