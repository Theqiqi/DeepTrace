# deeptrace/third_party 三方依赖收据

> 本目录存放**手动下载的三方库源码**。约定:**第三方源码不入库**(与 vcpkg 一致——vcpkg 库本体在 `out/.../vcpkg_installed/` 被忽略,git 只追踪配方 `vcpkg.json`)。手动下载场景下,本 README 就是配方:记录每个库的来源、版本、构建方式,保证源码丢失时可完整重建。`README*` 收据文件会随 git 入库,库源码本体被 `.gitignore` 忽略。

## keystone(汇编引擎)

- **用途**:提供汇编能力(asm 指令 → 字节),service 层 `asmenc.cpp` 通过 Keystone API 调用
- **版本**:0.9.2(2020-06-21,ChangeLog 确认)
- **来源**:https://github.com/keystone-engine/keystone/archive/refs/tags/0.9.2.zip
- **构建方式**:非 vcpkg——vcpkg 官方有 keystone port,但默认全架构 LLVM 构建需数十分钟;手动下载后可裁剪 `LLVM_TARGETS_TO_BUILD=X86` 加速(见 `deeptrace/CMakeLists.txt` 中 `add_subdirectory(third_party/keystone/llvm)` 的集成方式)
- **关键约束**:
  - 绕开 keystone 根 CMakeLists.txt(避免 kstool/fuzz 等无关目标),直接集成其 `llvm/` 子目录
  - keystone 目标强制 C++14(老 LLVM 3.9 代码与 C++20 不兼容)
  - 需要 Python 解释器供 llvm-build 脚本使用 → 见下方 python 收据
- **重新获取**:
  ```
  curl -L -o keystone-0.9.2.zip https://github.com/keystone-engine/keystone/archive/refs/tags/0.9.2.zip
  解压到 deeptrace/third_party/keystone/ 并保留目录名 keystone
  ```

## capstone(反汇编引擎)

- **用途**:提供反汇编能力(字节 → 指令文本),基础设施层 `disassembly/disasm.cpp` 通过 Capstone API 调用(x64, Intel 语法)
- **版本**:5.0.9
- **来源**:https://github.com/capstone-engine/capstone/archive/refs/tags/5.0.9.zip(亦见 sandbox/third_party/capstone 同源验证)
- **构建方式**:非 vcpkg——本环境 vcpkg 的 capstone port 默认禁用全部架构(cs_open 返回 CS_ERR_ARCH,sandbox 验证记录),故手动下载源码到 third_party/capstone 并仅启用 x86 后端;CMake 集成见 `deeptrace/CMakeLists.txt`(CAPSTONE_ARCHITECTURE_DEFAULT=OFF + CAPSTONE_X86_SUPPORT=ON,关闭 tests/cstool/cstest/install)
- **关键约束**:
  - 链接目标为 `capstone_static`(capstone 自身 OBJECT 库 `capstone` + 静态库 `capstone_static` + 共享库 `capstone_shared`)
  - `BUILD_STATIC_RUNTIME` 保持默认 OFF,capstone 遵循 preset 的 CMAKE_MSVC_RUNTIME_LIBRARY(/MDd 与 /MT),与 deeptrace 运行时一致;若未来改为 ON 会强制 /MT 造成与 deeptrace Debug /MDd 冲突
- **重新获取**:
  ```
  curl -L -o capstone-5.0.9.zip https://github.com/capstone-engine/capstone/archive/refs/tags/5.0.9.zip
  解压到 deeptrace/third_party/capstone/ 并保留目录名 capstone
  ```

## python(嵌入式解释器,仅构建期用)

- **用途**:仅构建期使用——keystone 的 LLVM CMake 构建需要 python(llvm-build 脚本生成 LLVMBuild.cmake 数据);Windows 侧未安装真实 python,故用官方嵌入式发行版
- **版本**:3.11.9
- **来源**:https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip
- **对 python311._pth 的修改**:追加相对路径 `..\keystone\llvm\utils\llvm-build`,使嵌入式 python 能 import llvmbuild 包;该相对路径依赖目录结构(third_party/python 与 third_party/keystone 同级),移动目录前需同步修改
- **注意**:此 python 仅用于构建,不参与运行时;若未来 Windows 侧安装真实 python,可删除本目录并在 deeptrace/CMakeLists.txt 改用系统 PYTHON_EXECUTABLE
- 详细说明见 `python/README.txt`
