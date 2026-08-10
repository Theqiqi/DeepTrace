# deeptrace 静态库 - 版本变更记录

## v1.1 架构分层调整(相对 v1.0)

v1.0 的四层(src/data、src/algorithm、src/atomic、src/api)调整为:

| v1.0 | v1.1 | 说明 |
|------|------|------|
| 数据层 src/data | Domain 层 src/domain | 职责不变(纯类型定义) |
| 算法层 src/algorithm | 算法层 src/algorithm | 保留 hex/scan/format 纯业务计算;x64 解码/编码移出 |
| 原子化层 src/atomic | Infrastructure 层 src/infrastructure | 扩展为按能力类型分子目录的基础能力层 |
| 接口层 src/api | Service 层 src/service | 职责不变(组装 + 会话 + 持久化) |
| — | utils 层 src/utils | 新增(按需):便捷门面,只转发调用不实现能力 |

调整原因:

1. **原子化层定义过窄**:"WinAPI 最小封装,一次系统调用"装不下反汇编、日志、线程池、网络、数据库等基础能力 → 扩展为 Infrastructure 层,按能力类型分子目录。
2. **x64 解码/编码是机器级能力**而非业务算法,从算法层移入 Infrastructure/disassembly(仍为纯函数,依赖规则不变:禁 I/O、禁平台 API)。
3. **跨平台组织方式**:不按平台建目录;同一能力的多平台实现以文件后缀区分文件名(如 process_win32.cpp / process_linux.cpp)。
4. **日志能力分层**:Infrastructure/logging 实现 Logger/LogSink/ConsoleSink;utils/log.h 只做便捷封装(转发调用),不实现日志系统。

v1.0 文档保留在 design/v1.0/deeptrace/ 作为历史记录。

## v1.1.1 汇编能力替换为 Keystone(相对 v1.1)

1. **自研编码器替换**:基础设施层 disassembly/asmenc 的内部实现由手写子集替换为 Keystone 0.9.2(源码自建,third_party/keystone),修复 `asm assemble "add rax,0"` 等指令报 BadFormat 的汇编失效 bug。
2. **接入方式**:按包管理选型原则(小型库 vcpkg 优先、中大型库手动下载优先),keystone 属中大型库——vcpkg 官方虽有 keystone port(0.9.2),但默认全架构构建需数十分钟,故手动下载源码到 third_party/keystone 并裁剪 X86 后端(LLVM_TARGETS_TO_BUILD=X86);绕开 keystone 根 CMakeLists(避免 kstool/fuzz 等无关目标),直接 add_subdirectory 其 llvm 子目录;Windows 侧无 python,用 third_party/python 嵌入式 python 供 llvm-build 使用;keystone 目标强制 C++14 兼容老 LLVM 代码。
3. **接口不变**:asm_one 单指令接口、service/asm.cpp、公共 API deeptrace.h、CLI 均无需改动,仅基础设施层内部实现切换。
4. **能力提升**:补齐内存操作数、lea、SSE/AVX、字符串指令等自研编码器缺失的指令。
