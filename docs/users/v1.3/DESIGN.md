# 用户文档 - 设计阶段(v1.3)

> 本文件是 `.flow/user_docs_development_process.md` 第 2 阶段的产出:
> 2.1 信息架构设计
> 2.2 文档结构设计
> 2.3 示例设计

---

## 2.1 信息架构设计

**组织方式**:按任务为主、按层级归档(与流程推荐一致)。

| 层级 | 文档 | 组织方式 | 说明 |
|------|------|---------|------|
| L1 入门 | README / GETTING_STARTED | 按任务(安装 → 首次使用) | 一条主线走通 |
| L2 日常 | USER_MANUAL | 按功能/命令组 | 每个命令组一个章节,任务式步骤 |
| L3 参考 | FAQ / TROUBLESHOOTING | 按问题 | 高频问题在前 |

**不按代码模块组织**:文档按用户任务(查看进程/读内存/设断点)组织,不按源码目录组织。

## 2.2 文档结构设计

```
docs/users/v1.3/
├── README.md             # 产品简介(一句话)+ 快速链接
├── GETTING_STARTED.md    # 快速开始
│   ├── 需要什么(Windows x64、deeptrace_cli.exe)
│   ├── 下载与安装(Release zip 解压 / Debug 构建产物)
│   ├── 第一次运行(打开命令窗口、-h/-v)
│   ├── 第一个任务:查看进程(ps list)
│   ├── 第二个任务:读取目标进程内存(找 PID、-p、mem read/readval)
│   └── 遇到问题?(指向 FAQ)
├── USER_MANUAL.md        # 用户手册(逐命令组)
│   ├── 通用:命令格式、-p/-h/-v、退出码、地址写法
│   ├── ps 进程:list / attach / detach / info / suspend / resume / kill
│   ├── mem 内存:read / write / dump / regions / readval
│   ├── module 模块:list / find / base / exports / dump
│   ├── thread 线程:list / suspend / resume / kill
│   ├── debug 调试:attach / detach / pause / resume / step / next / break / clear / hbreak / hclear / guard / unguard / status / registers / register
│   ├── disasm 反汇编:at / range
│   ├── resolve 解析:base / scan
│   ├── watch 监视:list / add / remove / refresh / clear
│   ├── dll 注入:inject / eject / list / status
│   ├── asm 汇编:assemble(--hex / --c-array)
│   └── shellcode 壳码:inject / injectat / status
├── FAQ.md                # 常见问题(按频率)
│   ├── 为什么 attach 报 NoSuchProcess?
│   ├── 为什么读内存失败/需要管理员权限?
│   ├── 为什么找不到进程 / 地址?
│   ├── 断点/watch 为什么跨命令还在?
│   ├── 注入不成功怎么办?
│   └── 命令参数记不住?
├── TROUBLESHOOTING.md    # 故障排除
│   ├── 错误提示对照表(Error: xxx → 含义 → 怎么办)
│   ├── 已知限制(仅 Windows x64、64 位目标、权限)
│   └── 验证方法(用 deeptrace_target.exe 练习)
├── ANALYSIS.md           # 分析阶段产出
├── DESIGN.md             # 本文档
└── CHANGELOG.md          # 变更历史
```

交叉引用:USER_MANUAL 各命令章节互相链到相关章节;FAQ 问题指向 USER_MANUAL 对应章节;GETTING_STARTED 指向 FAQ。

## 2.3 示例设计(贯穿全文的真实场景)

| 示例 | 场景 | 用于文档 |
|------|------|---------|
| 示例 A | 用测试目标程序练习:启动 deeptrace_target.exe,查看进程、读它的已知值 | GETTING_STARTED、USER_MANUAL 开头 |
| 示例 B | 读/写一个 4 字节值:mem read 0x14000D000 4 hex → 44 33 22 11;mem write 改写后再读 | USER_MANUAL 内存章节 |
| 示例 C | 设置并查看断点:debug break → debug status → debug clear | USER_MANUAL 调试章节 |
| 示例 D | AOB 扫描:resolve scan "DE AD BE EF" 找到 g_bytes 地址 | USER_MANUAL 解析章节 |
| 示例 E | 汇编:asm assemble "nop; ret" → 90C3 | USER_MANUAL 汇编章节 |

**示例真实性**:全部示例的操作输出已在真实运行中捕获(见 `/tmp/cli_outputs.txt`),文档中的输出块逐字取自真实运行结果,非虚构。测试目标地址(0x14000D000 等)来自 deeptrace_target.exe 的固定基址(ASLR 关闭),用户自行启动该目标程序时地址一致。
