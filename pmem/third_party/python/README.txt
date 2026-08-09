嵌入式 Python 3.11.9(官方 embeddable zip,免安装)
==================================================

用途:仅构建期使用——keystone 的 LLVM CMake 构建需要 python 解释器
(llvm-build 脚本生成 LLVMBuild.cmake 数据)。Windows 侧未安装真实
python(系统只有 Microsoft Store 占位符),因此使用官方嵌入式发行版。

来源:https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip

对 python311._pth 的修改:
- 追加了相对路径 ..\keystone\llvm\utils\llvm-build,使嵌入式 python
  能 import llvmbuild 包(llvm-build 脚本的同目录模块)。
- 该相对路径依赖目录结构:third_party/python 与 third_party/keystone
  必须保持同级(都在 pmem/third_party/ 下),移动目录前需同步修改。

注意:
- 此 python 仅用于构建,不参与运行时。
- 若未来 Windows 侧安装了真实 python,可删除本目录并在
  pmem/CMakeLists.txt 中改用系统 PYTHON_EXECUTABLE。
