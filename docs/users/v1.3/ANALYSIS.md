# 用户文档 - 分析阶段(v1.3)

> 本文件是 `.flow/user_docs_development_process.md` 第 1 阶段的产出:
> 1.1 用户画像
> 1.2 用例分析
> 1.3 文档分级规划

---

## 1.1 用户画像

| 要素 | 内容 |
|------|------|
| 用户身份 | 游戏修改/逆向分析人员、软件调试人员、安全研究者、AI 工具使用者 |
| 技术水平 | 会打开命令提示符/PowerShell 并输入命令;**不假设会编程、不假设会看代码** |
| 使用目标 | 查看进程列表、读取/修改目标进程内存、查看模块与导出、控制线程、调试(断点/单步/寄存器)、反汇编、汇编、AOB 扫描、监视变量、DLL/壳码注入 |
| 使用环境 | Windows 10/11 x64(目标进程也是 Windows 程序) |
| 关键认知 | 用户通过**命令行参数**与工具交互;每次执行一条命令;断点/watch/注入状态跨命令保留 |

## 1.2 用例分析(用真实命令描述,不用代码术语)

```
用例1:查看系统里正在运行的进程
  输入 deeptrace_cli ps list → 看到进程表格(名称/PID/线程数/父进程)

用例2:选中一个目标进程
  找到目标程序进程号(PID)→ 用 -p <PID> 参数指向它 → 查看进程信息 ps info

用例3:读取目标进程的内存值
  用 -p <PID> 加上 mem read <地址> → 看到十六进制字节;mem readval <地址> dword 直接看到数值

用例4:修改目标进程的内存值
  mem write <地址> <值> → 返回 OK;再读一次确认值已变化

用例5:查看模块与导出
  module list 列出已加载模块;module base <名字> 得到模块基址;module exports <模块> 列出导出函数

用例6:扫描内存特征码(AOB)
  resolve scan "48 8B ?? ?? 00" → 找到所有匹配地址

用例7:监视变量变化
  watch add <描述> <地址> <类型> → watch refresh / watch list 实时看到当前值

用例8:设置断点并查看状态
  debug break <地址> → debug status 看到断点计数 → debug clear <地址> 清除

用例9:查看 CPU 寄存器
  debug registers 看到所有寄存器;debug register rip 只看某个寄存器

用例10:反汇编一段内存
  disasm at <地址> <条数> → 看到地址/机器码/汇编指令对照

用例11:把汇编代码变成机器码
  asm assemble "nop; ret" → 得到 90C3

用例12:注入 DLL 或壳码
  dll inject <dll路径> → dll list 查看运行状态;shellcode inject <hex> → shellcode status 查看
```

> 注意:本产品是**命令行工具**,用户流程即上述命令输入 → 输出查看。所有命令在目标进程上执行前需先用 `-p <PID>` 指定目标。

## 1.3 文档分级规划

| 级别 | 目标读者 | 内容 | 文档 |
|------|---------|------|------|
| **L1 入门** | 零基础用户 | 下载安装、如何打开命令窗口、第一次查看进程、第一次读内存 | `GETTING_STARTED.md` |
| **L2 日常使用** | 有基础的用户 | 每个命令组怎么用(ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode) | `USER_MANUAL.md` |
| **L3 参考** | 高级用户 | 常见问题、错误提示对照、故障排除 | `FAQ.md`、`TROUBLESHOOTING.md` |

分级原则:入门文档不出现进阶内容(不解释 AOB 语法、不解释断点类型);参考文档不重复入门步骤(直接指向对应章节)。
