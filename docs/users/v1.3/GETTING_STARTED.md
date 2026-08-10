# 快速开始(GETTING_STARTED)

> 目标读者:零基础用户。跟着做,10 分钟内完成「安装 → 查看进程 → 读取内存」。
> 遇到看不懂的提示,先看 [常见问题](FAQ.md)。

## 1. 需要什么

- 一台 **Windows 10/11 64 位**电脑(x64)
- `deeptrace_cli.exe` 程序文件(见第 2 节)
- 想查看/修改的**目标程序**(任何 Windows 程序,比如一个游戏)

> 说明:本工具是**命令行程序**,用键盘输入命令、按回车执行,没有图形窗口。

## 2. 获取程序

### 方式一:使用打包好的压缩包(推荐)

拿到 `deeptrace_cli-<版本>-win64.zip` 后:

1. 右键压缩包 → **解压到当前文件夹**(或任意文件夹)。
2. 解压后你会看到一个文件:`deeptrace_cli.exe`。
3. 记住这个文件夹的位置(例如 `C:\Users\你\Downloads\deeptrace_cli`)。

### 方式二:使用开发版本(Debug 构建产物)

如果你是从源码构建的,程序在 `cli\out\bin\Debug\deeptrace_cli.exe`。

## 3. 第一次运行

1. 按 **Win + R**,输入 `cmd`,按回车 → 打开命令提示符。
2. 输入以下命令进入程序所在文件夹(把路径换成你的实际位置):

```
cd /d C:\Users\你\Downloads\deeptrace_cli
```

3. 输入 `deeptrace_cli -h` 查看帮助,你会看到程序版本和全部命令列表:

```
deeptrace_cli v1.0.0

Usage: deeptrace_cli [options] <command> [args...]
...
```

4. 输入 `deeptrace_cli -v` 查看版本:

```
deeptrace_cli v1.0.0
```

看到上面两行,说明程序可以正常使用了。

## 4. 第一个任务:查看正在运行的进程

输入:

```
deeptrace_cli ps list
```

你会看到一个进程表格(每行是一个正在运行的程序,列依次是:进程号 PID、名称、线程数、父进程号):

```
PID        NAME                                     THREADS  PPID
0          [System Process]                         24       0
4          System                                   361      0
...
```

**预期结果**:屏幕上列出几十个进程。如果看到这个表格,恭喜,第一步完成!

## 5. 第二个任务:读取一个进程的内存

要读取某进程的内存,先要知道它的**进程号(PID)**。

1. 从第 4 步的表格里,记下一个想查看的进程号(比如 `1234`)。
2. 输入以下命令查看该进程的信息:

```
deeptrace_cli -p 1234 ps info
```

你会看到类似:

```
PID: 1234
Name: notepad.exe
Threads: 3
ParentPID: 1000
```

3. 读取该进程某个内存地址的值(地址用十六进制,前面加 `0x`):

```
deeptrace_cli -p 1234 mem read 0x10000 4 hex
```

这会读出地址 `0x10000` 开始的 4 个字节,以十六进制显示,例如:

```
44 33 22 11
```

**预期结果**:一行十六进制字节。如果报错(比如 `Error: ReadFault` 或 `Error: AccessDenied`),说明这个地址不可读或没有权限——换一个地址试试,或参考[故障排除](TROUBLESHOOTING.md)。

## 6. 常用下一步

- 想读「数值」而不是原始字节 → [读数值 mem readval](USER_MANUAL.md#2-内存-mem)
- 想修改内存 → [写入 mem write](USER_MANUAL.md#2-内存-mem)
- 想看所有命令 → 随时输入 `deeptrace_cli -h`

## 7. 遇到问题?

- 提示 `'deeptrace_cli' 不是内部或外部命令` → 文件夹没进对,回第 3 步用 `cd /d` 进入程序所在文件夹
- 提示 `NoSuchProcess` → 进程号不对或程序已退出,回第 5 步重新找 PID
- 其他问题 → [常见问题](FAQ.md) / [故障排除](TROUBLESHOOTING.md)
