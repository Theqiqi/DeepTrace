# Module: Watch

Manages "watch entries" for target addresses: records address/type/description and can refresh the current value at any time. Watch entries are persisted to `%TEMP%/deeptrace_<pid>/watch.dat` and remain listable after the target process is reopened.

`watch_list`, `watch_remove`, and `watch_clear` only need the session pid (they work without an attached handle; values then show as `??`); `watch_add` and `watch_refresh` need the attached process handle.

## deeptrace::watch_list

### Syntax

```cpp
Result watch_list(std::vector<WatchEntry>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<WatchEntry>&` | output parameter, watch entry list |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | list generated (may be empty) |
| `Result::NotAttached` | no session (no pid set) |

### Description

Lists all watch entries of the session target process, reading current values live by address: with an attached handle it reads the real memory value; without one, the value is marked `"??"` with `valid=false` (no silently stale data). The `index` field is the persisted order index, used by `watch_remove`.

Prerequisites: `attach(pid)` done (handle optional; values invalid without it). Postconditions: none.

### Example

```cpp
std::vector<deeptrace::WatchEntry> ws;
deeptrace::watch_list(ws);
for (const auto& w : ws) {
    std::cout << w.index << ": " << w.value << "\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::watch_add](#deeptracewatch_add)
- [deeptrace::watch_refresh](#deeptracewatch_refresh)

---

## deeptrace::watch_add

### Syntax

```cpp
Result watch_add(const std::string& desc, uintptr_t addr, ValueType type);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `desc` | `const std::string&` | description text (`|` is replaced with a space when written to the file) |
| `addr` | `uintptr_t` | watched address |
| `type` | `ValueType` | value type |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | watch entry added and persisted |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | target address currently unreadable (addition refused) |

### Description

Adds a watch entry and writes it to `watch.dat`. Before adding, it reads 1 byte to verify the address is readable; an unreadable address returns `ReadFault` without adding, avoiding permanently invalid records. Duplicate addresses are allowed (each entry is independent). Refresh values with `watch_refresh`, remove with `watch_remove`, clear all with `watch_clear`.

Prerequisites: `attach(pid)` done. Postconditions: watch entry persisted.

### Example

```cpp
deeptrace::watch_add("hp", 0x140001000, deeptrace::ValueType::Dword);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::watch_remove](#deeptracewatch_remove)

---

## deeptrace::watch_remove

### Syntax

```cpp
Result watch_remove(uint32_t index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `index` | `uint32_t` | watch entry index (from the `index` field of `watch_list`) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | watch entry deleted |
| `Result::NotAttached` | no session |
| `Result::NotFound` | index out of range |

### Description

Deletes a watch entry by index and writes back to `watch.dat`. After deletion, the remaining entries' indices shift forward; always use the latest `watch_list` result.

Prerequisites: `attach(pid)` done. Postconditions: watch persistence updated.

### Example

```cpp
deeptrace::watch_remove(0);  // remove the first watch entry
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::watch_list](#deeptracewatch_list)

---

## deeptrace::watch_refresh

### Syntax

```cpp
Result watch_refresh(std::vector<WatchEntry>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<WatchEntry>&` | output parameter, watch entry list with fresh values |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | refresh completed |
| `Result::NotAttached` | no attached session |

### Description

Forcibly re-reads the current value of every watch entry and returns the list (behaves like `watch_list` but requires an attached handle). Used for polling target variable changes periodically (e.g. game HP, coins). When a single entry fails to read, that entry has `valid=false` and value `"??"`, without affecting the others.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::WatchEntry> ws;
deeptrace::watch_refresh(ws);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::watch_list](#deeptracewatch_list)

---

## deeptrace::watch_clear

### Syntax

```cpp
Result watch_clear();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all watch entries cleared |
| `Result::NotAttached` | no session |

### Description

Deletes all watch entries of the session target process (clears `watch.dat`).

Prerequisites: `attach(pid)` done. Postconditions: watch entries cleared.

### Example

```cpp
deeptrace::watch_clear();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::watch_remove](#deeptracewatch_remove)
