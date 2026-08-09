# 示例:远程内存读写与特征码扫描

演示类型化读取、写入与特征码扫描。源码:
[src/read_write_memory.cpp](src/read_write_memory.cpp)。

## API 调用顺序

| 步骤 | API | 说明 |
|------|-----|------|
| 1 | `attach(pid)` | 建立会话 |
| 2 | `memory_readval` | 类型化读取地址处的值 |
| 3 | `memory_write` | 写回新值 |
| 4 | `pattern_scan` | 全内存扫描特征码 |
| 5 | `detach()` | 关闭会话 |

## 代码

```cpp
// 完整代码见 src/read_write_memory.cpp,关键片段:
std::string text;
deeptrace::memory_readval(0x140000000, deeptrace::ValueType::Dword, text);  // 2. 读值
std::printf("readval: %s\n", text.c_str());

uint32_t v = 0xCAFEBABE;
size_t written = 0;
deeptrace::memory_write(0x140000000, &v, sizeof v, &written);                // 3. 写值

std::vector<uintptr_t> hits;
deeptrace::pattern_scan("DE AD BE EF", hits);                                // 4. 扫描
std::printf("hits: %zu\n", hits.size());
```

## 构建与运行

```bat
build_examples.bat
read_write_memory.exe 1234
```

## 提示

- `memory_readval` 支持 `ValueType::Byte/Word/Dword/Qword/Float/Double` 六种类型。
- `pattern_scan` 的 `??` 通配符可匹配任意单字节,适合定位跨版本不变的特征。
- 写入只读页会返回 `WriteFault`;修改代码段前通常需要先调整页保护。

## 相关 API

- [memory_readval](../Modules/MEMORY.md#deeptracememory_readval)
- [memory_write](../Modules/MEMORY.md#deeptracememory_write)
- [pattern_scan](../Modules/RESOLVE.md#deeptracepattern_scan)
