# Example: Remote Memory Read/Write and Pattern Scanning

Demonstrates typed reads, writes, and pattern scanning. Source:
[src/read_write_memory.cpp](src/read_write_memory.cpp).

## API Call Order

| Step | API | Description |
|------|-----|-------------|
| 1 | `attach(pid)` | establish a session |
| 2 | `memory_readval` | typed read of the value at an address |
| 3 | `memory_write` | write back a new value |
| 4 | `pattern_scan` | scan all memory for a pattern |
| 5 | `detach()` | close the session |

## Code

```cpp
// Full code in src/read_write_memory.cpp; key excerpts:
std::string text;
deeptrace::memory_readval(0x140000000, deeptrace::ValueType::Dword, text);  // 2. read value
std::printf("readval: %s\n", text.c_str());

uint32_t v = 0xCAFEBABE;
size_t written = 0;
deeptrace::memory_write(0x140000000, &v, sizeof v, &written);                // 3. write value

std::vector<uintptr_t> hits;
deeptrace::pattern_scan("DE AD BE EF", hits);                                // 4. scan
std::printf("hits: %zu\n", hits.size());
```

## Build & Run

```bat
build_examples.bat
read_write_memory.exe 1234
```

## Tips

- `memory_readval` supports six types: `ValueType::Byte/Word/Dword/Qword/Float/Double`.
- The `??` wildcard in `pattern_scan` matches any single byte, useful for locating signatures stable across versions.
- Writing to a read-only page returns `WriteFault`; modifying a code section usually requires adjusting page protection first.

## Related APIs

- [memory_readval](../Modules/MEMORY.md#deeptracememory_readval)
- [memory_write](../Modules/MEMORY.md#deeptracememory_write)
- [pattern_scan](../Modules/RESOLVE.md#deeptracepattern_scan)
