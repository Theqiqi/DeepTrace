# 开发者文档 - 分析阶段(v1.3)

> 本文件是 `.flow/developer_docs_development_process.md` 第 1 阶段的产出:
> 1.1 代码分析(代码结构分析 + 架构要点)
> 1.2 读者分析
> 1.3 文档需求分析

---

## 1.1 代码分析

### 1.1.1 项目组成

本仓库包含两个独立的 CMake 项目:

| 项目 | 目录 | 程序类型 | 职责 |
|------|------|---------|------|
| deeptrace | `deeptrace/` | Windows x64 静态库 | 进程内存操作能力:进程/内存/模块/线程/调试/反汇编/汇编/解析/监视/注入 |
| deeptrace_cli | `cli/` | Windows x64 命令行 exe | 把 deeptrace 公共 API 包装为命令行命令,纯 ASCII 输出 |

另有一个实验性目录 `sandbox/`(独立 CMake 项目,用于验证第三方库/环境,不参与交付)。

### 1.1.2 deeptrace 静态库分层(四层)

```
deeptrace/src/
├── domain/          数据层:公共数据结构 + 枚举(仅类型,无逻辑)
│   └── types.h
├── algorithm/       算法层:纯计算,无 I/O、无 WinAPI
│   ├── hex.{h,cpp}      hex 编解码
│   ├── scan.{h,cpp}     AOB 模式匹配(纯字节流)
│   └── format.{h,cpp}   数值/字节格式化
├── infrastructure/  原子化层:WinAPI 最小封装 + 三方引擎适配,按能力类型分子目录
│   ├── process/     OpenProcess / 快照 / 挂起 / 恢复 / 结束
│   ├── memory/      Read/WriteProcessMemory / VirtualQueryEx
│   ├── module/      模块快照 / PE 导出解析
│   ├── thread/      线程快照 / Suspend / Resume / Terminate
│   ├── debug/       调试器(附加/暂停/单步/寄存器/断点写入)
│   ├── inject/      VirtualAllocEx / 远端线程 / LoadLibrary 路径
│   ├── disassembly/ 反汇编(内部实现:Capstone)
│   └── assembly/    汇编编码(内部实现:Keystone)
└── service/         接口层:组装 domain+algorithm+infrastructure,实现公共 API
    ├── session.{h,cpp}  会话管理(附加的 pid/handle、状态目录路径)
    ├── store.{h,cpp}    状态文件读写(断点/watch/注入记录持久化)
    ├── process/memory/module/thread/debug/disasm/resolve/watch/inject/asm
    └── ...              每个公共 API 一个 service 文件
```

- **命名空间**:公共 API 在 `deeptrace` 命名空间;内部实现(algorithm/infrastructure/session/store)在 `deeptrace::internal` 命名空间。
- **公共头**:`deeptrace/include/deeptrace.h` 是消费者唯一允许 include 的头文件(55 个公共 API),公共类型见 `deeptrace/include/domain/types.h`。公共头禁止暴露 windows.h 类型,全部使用标准 C++ 类型。
- **依赖方向**(禁止跨层):
  ```
  service → algorithm + infrastructure + domain
  infrastructure → domain + WinAPI(错误统一转 Result)
  algorithm → domain(纯计算)
  domain → 无
  ```
  算法层不得调用 infrastructure;service 不得绕开 infrastructure 直接调用 WinAPI。
- **三方引擎**:Keystone(汇编)与 Capstone(反汇编)以源码方式放在 `deeptrace/third_party/`,CMake 裁剪 X86 后端自建;静态库不合并依赖,消费方(CLI)需显式链接 `keystone.lib` / `capstone.lib`。

### 1.1.3 cli 三层架构

```
cli/src/
├── main.cpp          入口:初始化 → parse_args → execute → 退出码;捕获异常兜底
├── command/          命令解析层:全局选项(-p/-h/-v)+ 命令路由 + 参数校验
│   ├── commands.{h,cpp}  命令表(组/子命令/参数规格)+ 帮助文本
│   ├── parser.{h,cpp}    getopt 全局选项 + 命令路由 + 参数校验
│   └── request.h          CommandRequest 结构体
├── interface/        接口调用层:命令请求 → deeptrace API 调用 → 结果结构
│   ├── executor.{h,cpp}  分发到各命令执行函数
│   ├── cmd.h              内部声明
│   └── cmd_*.cpp         按命令组拆分(process/memory/module/thread/debug/disasm/resolve/watch/inject/asm/shellcode)
└── printing/         命令打印层:结果结构 → ASCII 文本输出
    └── printer.{h,cpp}   纯格式化(表/hex/错误/帮助/版本)
```

- **命名空间**:`deeptrace_cli`(公共),内部辅助在 `deeptrace_cli::internal`。
- **依赖方向**(单向):
  ```
  main → command → interface → deeptrace 公共 API
                    ↓
               printing(纯格式化)
  ```
  printing 不依赖 command/interface,不调用 deeptrace。
- **边界**(CLI 生产代码):禁止 include windows.h/tlhelp32.h 等平台头;禁止第三方库;禁止阻塞式输入(getchar/scanf/cin);全部输出纯 ASCII。

### 1.1.4 跨项目依赖

```
cli(独立 CMake 项目)
  ├── include 路径 → ../../deeptrace/include(公共头,无 install 中间层)
  └── find_library → ../../deeptrace/out/lib/<配置>/deeptrace.lib
                     + keystone.lib(../../deeptrace/out/build/<配置小写>/third_party/keystone/lib)
                     + capstone.lib(../../deeptrace/out/lib/<配置>)
```

### 1.1.5 构建体系(两项目一致)

- CMake + CMakePresets.json(Ninja 生成器)+ MSVC(cl.exe,经 vcvars64)
- Debug:CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL(/MDd),triplet x64-windows
- Release:MultiThreaded(/MT),triplet x64-windows-static(静态运行时,单文件免 DLL)
- vcpkg manifest(`vcpkg.json`):gtest(仅测试依赖)
- 脚本:`script/build_debug.bat` / `build_release.bat`(Windows)+ `*_wsl.sh`(WSL → cmd.exe 桥接)
- 产物:exe → `out/bin/<配置>/`,lib → `out/lib/<配置>/`
- 打包:`cli/script/package.bat`(构建 deeptrace+cli Release → zip 归档到 `cli/out/dist/`)

### 1.1.6 测试体系

| 项目 | 层级 | 内容 |
|------|------|------|
| deeptrace | unit | `deeptrace_unit_test.exe`(hex/scan/disasm/asm/format,gtest) |
| deeptrace | integration | `deeptrace_integration_test.exe`(真实 target 进程串联多个 API) |
| deeptrace | target | `deeptrace_target.exe`(关闭 ASLR、已知地址放已知值、输出 PID 行) |
| deeptrace | dll | `testdll.dll`(注入测试的伴生 DLL) |
| cli | unit | `deeptrace_cli_unit_test.exe`(parser/printer/executor) |
| cli | integration | `deeptrace_cli_integration_test.exe`(parse→execute→deeptrace API 全链路) |
| cli | target | `deeptrace_target.exe`(e2e 用,同 deeptrace target) |
| cli | e2e | `test_cli_e2e.py`(Python 启动真实 exe 断言命令行行为,独立于 CMake) |

- 测试目标程序不链接 deeptrace,ASLR 关闭,保证地址确定。
- 集成测试需要真实 target 进程;e2e 需要 Debug 构建产物 + testdll.dll。

### 1.1.7 状态持久化约定

断点/watch/注入状态跨 CLI 调用保留,状态文件位于 `%TEMP%/deeptrace_<pid>/`:
- `breakpoints.dat`(软件/硬件断点原字节)
- `watch.dat`(监视条目)
- `inject.dat`(注入 DLL/壳码记录)

持久化由 deeptrace service 层实现(session.cpp 提供状态目录路径,store.cpp 读写)。

---

## 1.2 读者分析

| 读者类型 | 想了解的内容 | 文档侧重点 |
|----------|------------|-----------|
| **新入项目** | 项目结构、怎么编译运行、核心概念 | 入门指南(README/BUILDING)、概念说明(ARCHITECTURE) |
| **贡献者** | 扩展点在哪、怎么写新模块、测试要求 | 扩展指南(EXTENDING)、测试指南(TESTING) |
| **维护者** | 设计决策、已知限制、性能考量 | 架构说明(ARCHITECTURE)、技术决策记录(DESIGN_DECISIONS) |

目标读者是**开发者**,不是终端用户。终端用户文档(若有)独立于本文档集。

---

## 1.3 文档需求分析

| 文档 | 目标读者 | 内容 |
|------|---------|------|
| README.md | 所有入项目开发者 | 双项目一句话简介、编译、快速开始(≤50 行) |
| BUILDING.md | 新入项目 | 环境要求(MSVC+Ninja+CMake+vcpkg)、Debug/Release 编译步骤、WSL 桥接、常见编译问题 |
| ARCHITECTURE.md | 新入项目、维护者 | deeptrace 四层 + cli 三层、模块图、数据流、跨项目依赖、状态持久化 |
| TESTING.md | 贡献者 | 如何运行单元/集成/E2E 测试、编写新测试、target 程序说明 |
| EXTENDING.md | 贡献者 | 扩展点(添加命令/添加 API/替换引擎/添加算法)、完整示例 |
| DESIGN_DECISIONS.md | 维护者 | 技术决策记录(分层、引擎选型、运行时、状态持久化等) |
| CHANGELOG.md | 维护者 | 文档版本变更历史 |

所有文档的函数级说明**链接到 API 文档**(`docs/api/v1.3/`),不重复编写。
