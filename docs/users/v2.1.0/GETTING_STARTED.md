# Quick Start (GETTING_STARTED)

> Audience: absolute beginners. Follow along and finish "install → view processes → read memory" within 10 minutes.
> If you see a message you don't understand, check the [FAQ](FAQ.md) first.

## 1. What You Need

- A **Windows 10/11 64-bit** computer (x64)
- The `deeptrace_cli.exe` program file (see section 2)
- A **target program** you want to view/modify (any Windows program, e.g. a game)

> Note: this tool is a **command-line program** — you type commands and press Enter; there is no graphical window.

## 2. Getting the Program

### Option 1: Use the packaged zip (recommended)

Once you have `deeptrace_cli-<version>-win64.zip`:

1. Right-click the zip → **Extract All** (or extract to any folder).
2. After extraction you'll see one file: `deeptrace_cli.exe`.
3. Remember this folder's location (e.g. `C:\Users\you\Downloads\deeptrace_cli`).

### Option 2: Use a dev build (Debug build artifact)

If you built from source, the program is at `cli\out\bin\Debug\deeptrace_cli.exe`.

## 3. First Run

1. Press **Win + R**, type `cmd`, press Enter → the command prompt opens.
2. Type the following command to enter the folder containing the program (replace the path with your actual location):

```
cd /d C:\Users\you\Downloads\deeptrace_cli
```

3. Type `deeptrace_cli -h` to view help; you'll see the program version and the full command list:

```
deeptrace_cli v2.1.0

Usage: deeptrace_cli [options] <command> [args...]
...
```

4. Type `deeptrace_cli -v` to view the version:

```
deeptrace_cli v2.1.0
```

Seeing both lines above means the program works correctly.

## 4. First Task: View Running Processes

Type:

```
deeptrace_cli ps list
```

You'll see a process table (each row is a running program; columns are: process ID PID, name, thread count, parent PID):

```
PID        NAME                                     THREADS  PPID
0          [System Process]                         24       0
4          System                                   361      0
...
```

**Expected result**: dozens of processes listed on screen. If you see this table, congratulations — step one is done!

## 5. Second Task: Read a Process's Memory

To read a process's memory, you first need its **process ID (PID)**.

1. From the table in step 4, note down a process ID you want to inspect (e.g. `1234`).
2. Type the following command to view that process's info:

```
deeptrace_cli -p 1234 ps info
```

You'll see something like:

```
PID: 1234
Name: notepad.exe
Threads: 3
ParentPID: 1000
```

3. Read the value at a memory address in that process (addresses are hexadecimal, prefixed with `0x`):

```
deeptrace_cli -p 1234 mem read 0x10000 4 hex
```

This reads 4 bytes starting at address `0x10000`, displayed in hexadecimal (2 digits per byte, space-separated):

```
44 33 22 11
```

**Expected result**: one line of hexadecimal bytes. Note: the `44 33 22 11` above is only an example — **the actual content depends on the value at that address in the target process**; seeing different bytes is normal.

If you get an error (e.g. `Error: ReadFault` or `Error: AccessDenied`), the address is unreadable or you lack permission — try a different address, or see [Troubleshooting](TROUBLESHOOTING.md).

## 6. Common Next Steps

- Want to read a "value" instead of raw bytes → [Read typed values: mem readval](USER_MANUAL.md#2-memory-mem)
- Want to modify memory → [Write: mem write](USER_MANUAL.md#2-memory-mem)
- Want to see all commands → type `deeptrace_cli -h` any time

## 7. Having Trouble?

- `'deeptrace_cli' is not recognized as an internal or external command` → you're not in the right folder; go back to step 3 and use `cd /d` to enter the program's folder
- `NoSuchProcess` → the PID is wrong or the program has exited; go back to step 5 and find the PID again
- Anything else → [FAQ](FAQ.md) / [Troubleshooting](TROUBLESHOOTING.md)
