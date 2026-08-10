# 架构概览(ARCHITECTURE)

> 目标读者:新入项目开发者、维护者。
> 设计依据:design/v1.0、v1.1、v1.2(实际以代码为准)。
> 函数级 API 说明见 [API 文档](../../api/v1.3/README.md)。

## 1. 总览

```
┌─────────────────────────────────────────────────────────────┐
│  cli(独立 CMake 项目)                                        │
│                                                             │
│  main.cpp                                                    │
│    │                                                         │
│    ├─ command/     命令解析层(argv → CommandRequest)          │
│    ├─ interface/   接口调用层(CommandRequest → deeptrace API) │
│    └─ printing/    命令打印层(结果 → ASCII 文本)              │
│         │                                                     │
│         │  include ../../deeptrace/include + find_library     │
│         ▼                                                     │
│  deeptrace(独立 CMake 项目,静态库)                             │
│                                                             │
│  service/        接口层:组装,实现公共 API + 会话/持久化        │
│    │                                                         │
│    ├─ algorithm/  算法层:纯计算(hex/AOB/format)               │
│    ├─ infrastructure/  原子化层:WinAPI 封装 + 引擎适配         │
│    │   (process/memory/module/thread/debug/inject/           │
│    │    disassembly/Capstone + assembly/Keystone)            │
│    └─ domain/     数据层:公共类型(仅类型,无逻辑)              │
└─────────────────────────────────────────────────────────────┘
```

## 2. deeptrace 静态库:四层

### 2.1 数据层 domain/ `deeptrace` 命名空间

- **职责**:定义全部公共数据结构与枚举(`Result`/`ValueType`/`BreakpointType` + `ProcessInfo`/`MemoryRegion`/`ModuleInfo`/`ThreadInfo`/`RegisterInfo`/`BreakpointInfo`/`WatchEntry`/`Instruction`/`DebugStatus`/`InjectInfo` 等)。
- **禁止项**:任何逻辑、任何 I/O。
- 公共头 `deeptrace/include/domain/types.h` 与 `src/domain/types.h` 内容一致,必须保持同步。

### 2.2 算法层 algorithm/ `deeptrace::internal` 命名空间

| 文件 | 能力 |
|------|------|
| hex.{h,cpp} | hex 编解码 |
| scan.{h,cpp} | AOB 模式匹配(纯字节流) |
| format.{h,cpp} | 数值/字节格式化 |

- **职责**:纯计算,输入输出均为内存数据(字节流/字符串)。
- **禁止项**:WinAPI、I/O、进程读写、非纯函数;只依赖 domain。
- 只依赖数据层类型。

### 2.3 原子化层 infrastructure/ `deeptrace::internal` 命名空间

按能力类型分子目录:

```
infrastructure/
├── process/     OpenProcess / 进程快照 / 挂起 / 恢复 / 结束
├── memory/      Read/WriteProcessMemory / VirtualQueryEx
├── module/      模块快照 / PE 导出解析
├── thread/      线程快照 / Suspend / Resume / Terminate
├── debug/       调试器(附加/暂停/单步/寄存器/断点写入)
├── inject/      VirtualAllocEx / 远端线程 / LoadLibrary 路径
├── disassembly/ 反汇编(内部实现:Capstone 5.0.9)
└── assembly/    汇编编码(内部实现:Keystone 0.9.2)
```

- **职责**:WinAPI 最小封装(一次系统调用)+ 三方引擎适配,错误统一转换为 `deeptrace::Result`。
- **禁止项**:组装业务流程、持久化、跨多次调用的状态。
- 引擎适配文件(disasm/asmenc)只暴露纯函数接口,引擎替换不影响上层。

### 2.4 接口层 service/ `deeptrace`(公共 API)与 `deeptrace::internal`(session/store)

| 文件 | 职责 |
|------|------|
| session.{h,cpp} | 会话管理:附加的 pid/handle、`state_dir()` 状态目录路径 |
| store.{h,cpp} | 状态文件读写(断点/watch/注入记录) |
| process/memory/module/thread/debug/disasm/resolve/watch/inject/asm | 各公共 API 实现 |

- **职责**:组装 domain + algorithm + infrastructure 实现 55 个公共 API;会话管理;断点/watch/注入状态持久化;`result_message` 错误语义化。
- **禁止项**:直接调用 WinAPI(必须经 infrastructure)、内联算法。
- service 公共函数在 `deeptrace` 命名空间(session/store 内部辅助在 `deeptrace::internal`)。

### 2.5 依赖方向(禁止跨层)

```
service → algorithm + infrastructure + domain
infrastructure → domain + WinAPI
algorithm → domain
domain → 无
```

- 算法层不得调用 infrastructure。
- service 不得绕开 infrastructure 直接调用 WinAPI。

## 3. cli 三层

### 3.1 main.cpp

- **职责**:初始化 → `parse_args` → `execute` → 退出码;`std::exception` 兜底返回 1。
- **禁止项**:业务逻辑、直接调用 deeptrace。

### 3.2 命令解析层 command/

| 文件 | 职责 |
|------|------|
| commands.{h,cpp} | 命令表(组/子命令/参数规格)+ 帮助文本 |
| parser.{h,cpp} | getopt 全局选项(-p/-h/-v)+ 命令路由 + 参数校验 |
| request.h | `CommandRequest` 结构体 |

- **职责**:解析与校验,构造 CommandRequest。
- **禁止项**:执行业务、调用 deeptrace、输出业务结果(允许输出参数错误)。

### 3.3 接口调用层 interface/

| 文件 | 职责 |
|------|------|
| executor.{h,cpp} | 按命令分发到各执行函数 |
| cmd.h | 内部声明(`deeptrace_cli::internal`) |
| cmd_*.cpp | 按命令组拆分(process/memory/module/thread/debug/disasm/resolve/watch/inject/asm/shellcode) |

- **职责**:根据 CommandRequest 调用 deeptrace 公共 API,组织返回结果交给打印层。
- **禁止项**:直接 WinAPI、自己实现 deeptrace 已有能力、格式化输出。

### 3.4 命令打印层 printing/

- **职责**:纯格式化(进程表/区域表/模块表/寄存器表/hex dump/错误/帮助/版本),`printer` 命名空间。
- **禁止项**:调用 deeptrace、依赖 command/interface、业务逻辑。
- 输出约束:纯 ASCII;宽字符不可打印部分替换为 `?`;地址 `0x%016llX`;字节 `%02X` 大写。

### 3.5 依赖方向

```
main → command → interface → deeptrace 公共 API
                    ↓
               printing(独立,仅依赖 deeptrace 公共类型)
```

## 4. 数据流(命令生命周期)

```
argv → parser 解析(全局选项 + 命令路由 + 参数校验)
     → CommandRequest
     → executor 分发 → cmd_xxx() 调 deeptrace 公共 API
     → Result + 结果结构
     → printer 格式化 → stdout / stderr
     → 退出码(0 成功 / 1 执行失败 / 2 用法错误)
```

会话约定:main 解析出 pid 后先 `deeptrace::attach(pid)`,执行完命令 `deeptrace::detach()`;断点/watch/注入状态由状态文件持久化,跨 CLI 调用保留。

## 5. 跨项目依赖

```
cli(独立 CMake 项目)
  ├── include 路径 → ../../deeptrace/include(公共头,无 install 中间层)
  ├── find_library(DEEPTRACE_LIB)  → ../../deeptrace/out/lib/<配置>/deeptrace.lib
  ├── find_library(KEYSTONE_LIB)   → ../../deeptrace/out/build/<配置小写>/third_party/keystone/lib
  └── find_library(CAPSTONE_LIB)   → ../../deeptrace/out/lib/<配置>/capstone.lib
```

- deeptrace 是静态库,三方依赖(keystone/capstone)不会自动传入链接行,**CLI 必须显式链接**(cli/src/CMakeLists.txt 已处理)。
- 公共头只暴露标准 C++ 类型,CLI 生产代码禁止 include windows.h 等平台头。

## 6. 状态持久化

```
%TEMP%/deeptrace_<pid>/
├── breakpoints.dat   # 软件/硬件断点原字节(DR0-DR3 槽位)
├── watch.dat         # 监视条目
└── inject.dat        # 注入 DLL/壳码记录(kind=dll|shellcode)
```

- 由 service 层实现(session.cpp 提供路径,store.cpp 读写,ASCII `|` 分隔行格式)。
- 作用:断点/watch/注入状态跨 CLI 进程保留(会话=单次 CLI 进程,但状态文件跨进程)。
- 清理:对同一 pid 重新操作时按需覆盖;测试用例需自行清理避免状态污染。

## 7. 核心概念

| 概念 | 说明 |
|------|------|
| 会话 | `attach(pid)` 后持有目标进程句柄;`detach()` 释放;`debug_attach()` 进入调试模式,`debug_detach()` 退出调试但保持附加 |
| 断点 | 软件断点(写 0xCC,保存原字节)/ 硬件断点(DR0-DR3)/ 页守卫(guard) |
| watch | 描述+地址+类型;`watch_refresh`/`watch_list` 读取目标内存显示实时值 |
| 注入 | DLL(LoadLibrary 路径 + 远端线程)/ 壳码(VirtualAllocEx + 远端线程);`dll_list`/`shellcode_status` 查询运行状态 |
| 反汇编/汇编 | disasm_at/range 调 Capstone;asm_assemble 调 Keystone(X86 后端) |
