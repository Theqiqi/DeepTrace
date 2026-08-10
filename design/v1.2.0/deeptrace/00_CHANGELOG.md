# deeptrace 静态库 - 版本变更记录

## v1.2.0 职责修正 + 反汇编引擎替换(相对 v1.1.1)

### 1. 职责修正:汇编能力独立为 infrastructure/assembly/

- **问题**:v1.1.0 架构调整时,keystone 汇编实现(asmenc)被放进 `infrastructure/disassembly/` 目录,与反汇编(disasm)混在一起——汇编与反汇编是**两类不同的基础能力**,职责未分清。
- **修正**:新建 `src/infrastructure/assembly/`(asmenc.h/cpp,基于 keystone 的 x64 编码器),从 `disassembly/` 迁出;`src/infrastructure/disassembly/` 只保留反汇编(disasm)。与流程文件约定一致:Infrastructure 按能力类型分子目录(process/ memory/ thread/ debug/ inject/ **assembly/** **disassembly/** ...)。

### 2. 反汇编引擎替换:自研子集解码器 → Capstone 5.0.9

- **问题**:自研 x64 解码器(手写 ~26KB 子集)覆盖不全:SSE/SSE2、REP 字符串指令等大量指令无法解码,且维护成本高。
- **替换**:基础设施层 `disassembly/disasm` 内部实现切换为 Capstone 5.0.9(源码自建,`third_party/capstone`,仅启用 x86 后端)。Capstone 为 BSD 许可、维护活跃,已在 sandbox 验证本环境可用(vcpkg port 在此环境默认禁用全部架构 → 手动下载源码构建,理由见 11_技术选型)。
- **接入方式**:与 keystone 同理按"中大型/环境适配库手动下载到项目 third_party/ 优先"原则,拷贝 sandbox 已验证的 capstone 源码到 deeptrace/third_party/capstone;CMake 仅启用 x86 后端(CAPSTONE_ARCHITECTURE_DEFAULT=OFF + CAPSTONE_X86_SUPPORT=ON)加速构建,关闭 tests/cstool/install。
- **接口不变**:`disasm_one` 单指令接口、`DecodedInsn{length,text}` 结构、service/disasm.cpp、公共 API deeptrace.h(disasm_at/disasm_range)、CLI 均无需改动;仅基础设施层内部实现切换。
- **能力提升**:解码覆盖提升为 Capstone 全量 x86-64 指令集(含 SSE/AVX/字符串指令/未知指令的确定性失败),修复自研引擎"解码不了就静默停"与覆盖缺失问题。
- **输出格式变化**:解码文本改用 Capstone Intel 语法规范格式(如 `mov rax, qword ptr [rbp + 8]`、`movabs rax, 0x...`、`lea rax, [rip + 0x10]`),单元测试断言同步更新。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| src/infrastructure/assembly/asmenc.{h,cpp} | 新增(从 disassembly/ 迁出,内容不变) |
| src/infrastructure/disassembly/asmenc.{h,cpp} | 删除 |
| src/infrastructure/disassembly/disasm.{h,cpp} | 实现替换为 Capstone(接口不变) |
| src/service/asm.cpp | include 路径改为 infrastructure/assembly/asmenc.h |
| test/unit/asm_test.cpp | include 路径同步 |
| test/unit/disasm_test.cpp | 断言更新为 Capstone 格式 |
| CMakeLists.txt | 新增 capstone add_subdirectory |
| src/CMakeLists.txt | assembly/ 目录 + capstone 链接 |
| third_party/README.md | 新增 capstone 收据 |

公共 API(deeptrace.h)与 CLI 层零改动。

### 4. 实现要点与踩坑(测试驱动发现)

1. **capstone 5.0.9 + MSVC 下 cs_disasm_iter 崩溃**:首版 disasm_one 用 `cs_disasm_iter` + 栈上未初始化 `cs_insn`,所有解码路径(含 nop)立即访问违例(0xc0000005);sandbox 同源独立验证程序用 `cs_disasm` 正常。改用 `cs_disasm(count=1)` 单条解码路径(内部自分配 insn 数组)后全部通过。**结论:本环境统一使用 cs_disasm 路径,不用 cs_disasm_iter + 栈结构体。**
2. **静态库不合并三方依赖,消费方需显式链接**:deeptrace 是静态库,`target_link_libraries(deeptrace PRIVATE capstone_static)` 的依赖不会进入 CLI 的链接行——CLI 是独立 CMake 项目,经 find_library 引用 deeptrace.lib,链接时报 cs_disasm/cs_free 未解析。按既有 keystone 约定,在 cli/src/CMakeLists.txt 显式 find_library(capstone) 并加入链接。
3. **capstone_static 目标名**:capstone 自身是 OBJECT 库 `capstone` + 静态库 `capstone_static` + 共享库 `capstone_shared`,deeptrace 链接静态目标 `capstone_static`。
4. **运行时策略**:capstone `BUILD_STATIC_RUNTIME` 保持默认 OFF,遵循 preset 的 CMAKE_MSVC_RUNTIME_LIBRARY(/MDd Debug、/MT Release),与 deeptrace 及 CLI 一致,无冲突。

## v1.1.1 汇编能力替换为 Keystone(相对 v1.1.0)

1. **自研编码器替换**:基础设施层 disassembly/asmenc 的内部实现由手写子集替换为 Keystone 0.9.2(源码自建,third_party/keystone),修复 `asm assemble "add rax,0"` 等指令报 BadFormat 的汇编失效 bug。
2. **接入方式**:按包管理选型原则(小型库 vcpkg 优先、中大型库手动下载优先),keystone 属中大型库——vcpkg 官方虽有 keystone port(0.9.2),但默认全架构构建需数十分钟,故手动下载源码到 third_party/keystone 并裁剪 X86 后端(LLVM_TARGETS_TO_BUILD=X86);绕开 keystone 根 CMakeLists(避免 kstool/fuzz 等无关目标),直接 add_subdirectory 其 llvm 子目录;Windows 侧无 python,用 third_party/python 嵌入式 python 供 llvm-build 使用;keystone 目标强制 C++14 兼容老 LLVM 代码。
3. **接口不变**:asm_one 单指令接口、service/asm.cpp、公共 API deeptrace.h、CLI 均无需改动,仅基础设施层内部实现切换。
4. **能力提升**:补齐内存操作数、lea、SSE/AVX、字符串指令等自研编码器缺失的指令。
