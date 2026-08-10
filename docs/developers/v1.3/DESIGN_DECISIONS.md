# 技术决策记录(DESIGN_DECISIONS)

> 目标读者:维护者。每个决策记录「背景 → 方案对比 → 最终选择 → 理由」,不只写结论。
> 依据:design/v1.0~v1.2 技术选型与实现踩坑记录(design/v1.2/deeptrace/00_CHANGELOG.md)。

## ADR-01 为什么是「静态库 + 独立 CLI」双项目

- **背景**:能力(进程内存操作)与交互(命令行)是两类不同生命周期的产物;库需要被多次调用且可复用,CLI 只是库的一个消费者。
- **方案对比**:
  - 单项目:库与 CLI 混在一个 CMake 目标里 → 无法单独复用库、无法独立测试库。
  - 双独立 CMake 项目:cli 经 `find_library` + include 路径引用 deeptrace 产物 → 库可独立交付(design 约定产物 `deeptrace.lib` + `deeptrace.h`,无 install 中间层)。
- **选择**:双项目。CLI 是 deeptrace 库的**第一个且当前唯一**的消费者,库 API 设计的验收标准就是「CLI 能干净调用」。

## ADR-02 为什么 deeptrace 用四层(domain/algorithm/infrastructure/service)

- **背景**:进程内存操作涉及纯计算(hex/AOB/解码)与系统调用(WinAPI)两类本质不同的逻辑,混在一起难以测试与替换。
- **方案对比**:
  - 两层(接口 + 实现):WinAPI 与算法内联 → 算法不可单测、引擎不可替换(v1.0 自研解码器无法替换为 Capstone 就是教训)。
  - 四层:算法纯计算无 I/O(可独立单测);infrastructure 只做「一次系统调用」封装;service 组装 + 持久化。
- **选择**:四层。约束:算法层禁止 WinAPI/I/O;service 禁止直接 WinAPI;依赖单向向下。引擎替换(ADR-04)零上层改动正是此分层的收益。

## ADR-03 为什么 cli 用三层(command/interface/printing)

- **背景**:CLI 需要把 55 个 API 映射为命令;解析、调用、格式化是三类独立可测的职责。
- **方案对比**:
  - 单文件 main:不可测、不可扩展。
  - 三层:command 只解析与校验、interface 只调 API、printing 只格式化(纯 ASCII,不依赖前两层)。
- **选择**:三层。单元测试可分别覆盖 parser/printer/executor;新增命令只需加 cmd_*.cpp + commands 表。

## ADR-04 为什么反汇编用 Capstone、汇编用 Keystone(源码自建)

- **背景**:
  - 自研 x64 解码器(~26KB 子集)覆盖不全(SSE/SSE2/REP 字符串指令等),且「解码不了就静默停」;
  - 自研编码器对 `add rax,0` 等指令报 BadFormat(汇编失效 bug);
  - vcpkg 的 capstone port 在本环境默认禁用全部架构(cs_open 返回 CS_ERR_ARCH)。
- **方案对比**:
  - vcpkg 安装:keystone port 全架构构建需数十分钟;capstone port 本环境不可用。
  - 源码自建到 `third_party/`:keystone 裁剪 `LLVM_TARGETS_TO_BUILD=X86`;capstone 仅启用 X86 后端(`CAPSTONE_ARCHITECTURE_DEFAULT=OFF` + `CAPSTONE_X86_SUPPORT=ON`),关闭 tests/cstool/install。
- **选择**:源码自建(中大型/环境适配库「手动下载到 third_party 优先」原则)。接口不变,service/公共 API/CLI 零改动。
- **踩坑**:keystone 绕开根 CMakeLists 直接集成 llvm 子目录(避免 kstool/fuzz 等目标与 /MD 替换冲突);LLVM 构建需 python,Windows 侧用 `third_party/python` 嵌入式 python。

## ADR-05 为什么反汇编用 `cs_disasm` 而不用 `cs_disasm_iter`

- **背景**:Capstone 5.0.9 + MSVC 下,`cs_disasm_iter` + 栈上未初始化 `cs_insn` 的所有解码路径立即访问违例(0xc0000005);sandbox 同源独立验证程序用 `cs_disasm` 正常。
- **方案对比**:`cs_disasm_iter`(调用方提供 cs_insn 缓冲区,本环境崩溃)vs `cs_disasm(count=1)`(内部自分配 insn 数组,稳定)。
- **选择**:统一使用 `cs_disasm` 路径,不用 `cs_disasm_iter` + 栈结构体(design/v1.2 CHANGELOG 已记录,回归防护注释在 disasm 源码中)。

## ADR-06 为什么 Debug=/MDd、Release=/MT

- **背景**:两项目共享构建约定,运行库必须一致,否则 LNK2038。
- **方案对比**:统一 /MDd(动态)→ Release 产物需要 VC 运行库 DLL,不便分发;Release 用 /MT(静态)→ 单文件免 DLL。
- **选择**:Debug=`/MDd`(x64-windows)、Release=`/MT`(x64-windows-static)。vcpkg triplet 同步切换;keystone/capstone 的 `BUILD_STATIC_RUNTIME` 保持默认 OFF,遵循 preset 的 `CMAKE_MSVC_RUNTIME_LIBRARY`,与库一致。

## ADR-07 为什么断点/watch/注入状态用文件持久化到 %TEMP%

- **背景**:CLI 是「单次命令」进程(会话=单次进程),但断点/watch/注入是跨命令的长期状态;进程退出后状态必须保留,下一次 CLI 调用继续生效。
- **方案对比**:内存驻留(不可跨进程)、注册表(污染系统)、`%TEMP%/deeptrace_<pid>/` 状态文件(进程私有、无需清理协议、按 pid 隔离)。
- **选择**:状态文件(`breakpoints.dat`/`watch.dat`/`inject.dat`,ASCII `|` 分隔行)。持久化由 service 层实现,算法层不参与。

## ADR-08 为什么测试目标程序关闭 ASLR

- **背景**:集成/e2e 测试需要「已知地址读已知值」,但 ASLR 使每次启动地址随机。
- **方案对比**:运行时解析地址(复杂、脆弱)vs 关闭 ASLR 使地址确定(简单、可断言)。
- **选择**:target 用 `/DYNAMICBASE:NO /HIGHENTROPYVA:NO` 关闭 ASLR,banner 输出 `PID:` 行 + 变量地址表(`g_int` 等)。target 不链接 deeptrace,是独立的可执行测试锚点。

## ADR-09 为什么静态库不合并三方依赖,消费方需显式链接

- **背景**:`target_link_libraries(deeptrace PRIVATE capstone_static)` 的依赖不会进入 CLI 的链接行——CLI 是独立 CMake 项目,经 find_library 引用 deeptrace.lib,链接时报 `cs_disasm`/`cs_free` 未解析。
- **方案对比**:把 capstone/keystone 合并进 deeptrace.lib(静态库本质不传递 PRIVATE 依赖,需换 OBJECT 库等复杂方案)vs 消费方显式 `find_library(keystone/capstone)` 并链接(透明、符合 CMake 静态库惯例)。
- **选择**:消费方显式链接(cli/src/CMakeLists.txt 已实现,并有注释说明,勿删除)。

## ADR-10 为什么状态目录用 `deeptrace_<pid>` 且状态文件按 pid 隔离

- **背景**:同一目标进程的断点/watch 状态必须唯一且跨调用稳定;不同 pid 的状态不能互相污染。
- **方案对比**:全局单文件(多进程冲突)vs 按 pid 子目录(`%TEMP%/deeptrace_<pid>/`,天然隔离、目录名含 pid 可追溯)。
- **选择**:`%TEMP%/deeptrace_<pid>/`(session.cpp `state_dir()` 实现)。

## 已知限制与取舍

- 仅 Windows x64;无跨平台计划(公共头已用标准类型,保留理论可移植性)。
- 断点状态文件在目标进程退出后仍残留于 %TEMP%(无害,但需手动清理)。
- 部分调试操作(硬件断点/页守卫)依赖 x64 架构能力,非 x64 目标不支持。
- e2e 需要 Debug 构建 + testdll.dll;Release 打包产物仅含 deeptrace_cli.exe(不包含测试件)。
