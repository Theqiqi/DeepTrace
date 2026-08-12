# CHANGELOG — deeptrace_cli v2.11.0

> v2.11.0 = v2.10.0 + attach 权限透出(静态库 + CLI 双改动)

## 改动点清单

| # | 改动点 | 新增/修改 | 影响 |
|---|--------|-----------|------|
| 1 | 静态库 `Session` 记录 attach 实际权限掩码 | 修改 | deeptrace session |
| 2 | 静态库新增公共 API `session_permissions()` | 新增 | deeptrace.h |
| 3 | CLI `ps attach` 输出权限摘要(语义化名列表) | 修改 | cmd_process.cpp、printer |
| 4 | 版本号 2.11.0 | 修改 | 全局 |

## 决策记录

- 权限来源:attach 实际成功的 OpenProcess access 掩码(Session 记录),
  零额外探测、零副作用;`-p` 选哪个进程就检测哪个。
- 语义化名列表(人类/AI 友好,非 PROCESS_* C 语义):
  `read` / `write` / `vm_operate` / `create_thread` / `suspend_resume` /
  `terminate` / `query` / `query_limited` / `dup_handle` / `create_process` /
  `set_quota` / `set_info`;缺失位不显示。
- `ps attach` 成功输出 `OK (permissions: read|write|...)`;
  失败仍透出 AccessDenied(既有语义)。
- 版本号:修改位 +1 → v2.11.0(独立小版本)。
