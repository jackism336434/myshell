# MyShell — 自制 Unix Shell

一个从零开始编写的 Unix Shell，用纯 C 语言实现，支持管道、重定向、通配符、别名、内建命令和持久化历史记录。适合用于学习 Linux 系统编程、进程管理、文件描述符操作等底层知识。

## 功能特性

- **命令执行**：运行任意外部系统命令（通过 `fork` + `execvp`）
- **管道符 `|`**：支持多级管道串联（最多 64 级），如 `cat file | grep foo | wc -l`
- **输出重定向 `>` / `>>`**：覆盖写入或追加写入文件
- **输入重定向 `<`**：从文件读取标准输入
- **通配符 `*` `?`**：基于 `glob(3)` 实现的文件名通配展开
- **别名 `alias`**：定义命令别名，如 `alias ll=ls -l`
- **环境变量 `export`**：设置环境变量，如 `export FOO=bar`
- **历史记录**：基于 GNU Readline 的行编辑（上下箭头、Ctrl+F 搜索），自动持久化到 `~/.myshell_history`
- **彩色提示符**：绿色箭头表示上一条命令成功，红色表示失败
- **信号处理**：`Ctrl+C` 中止前台子进程，Shell 本身不受影响

## 快速开始

### 依赖

- GCC（或其他 C99 编译器）
- GNU Readline 库（`libreadline-dev`）
- GNU Make

在 Debian/Ubuntu 上安装依赖：

```bash
sudo apt install build-essential libreadline-dev
```

### 编译 & 运行

```bash
make              # 编译生成 myshell 二进制文件
make run          # 编译并启动 Shell
make test         # 冒烟测试（验证正常启动和退出）
make clean        # 清理编译产物
```

### 管道集成测试

```bash
chmod +x test_pipe.sh
./test_pipe.sh    # 自动对比 MyShell 与 Bash 的管道输出
```

## 使用示例

```
myshell:~/myshell ❯ ls -l
myshell:~/myshell ❯ echo "hello world" > hello.txt
myshell:~/myshell ❯ cat hello.txt
hello world
myshell:~/myshell ❯ cat hello.txt | grep hello | wc -l
1
myshell:~/myshell ❯ ls *.c | grep my
myshell:~/myshell ❯ alias ll=ls -l
myshell:~/myshell ❯ ll
myshell:~/myshell ❯ export MYVAR=myshell
myshell:~/myshell ❯ echo $MYVAR
myshell:~/myshell ❯ history
    1  ls -l
    2  echo "hello world" > hello.txt
    ...
myshell:~/myshell ❯ cd /tmp
myshell:/tmp ❯ exit
Goodbye!
```

## 内建命令

| 命令 | 用法 | 说明 |
|---|---|---|
| `cd [dir]` | `cd /path` 或 `cd`（回家目录） | 切换当前工作目录 |
| `exit` | `exit` | 退出 Shell |
| `history` | `history` | 打印历史命令列表 |
| `export` | `export NAME=value` 或 `export`（打印全部） | 设置/查看环境变量 |
| `alias` | `alias name=value` 或 `alias`（打印全部） | 定义/查看别名 |

## 架构概览

### 两路执行模型

MyShell 的核心设计在于命令解析后的分支判断——根据是否包含管道符 `|`，进入完全不同的执行路径：

```
用户输入 → 解析 → 别名展开 → 通配符展开
                        ↓
              扫描是否存在 | ?
              /            \
          无管道            有管道
            ↓                 ↓
    ┌──────────────┐   ┌──────────────────┐
    │ 单命令路径    │   │ 管道路径          │
    │              │   │                  │
    │ 内建: 父进程  │   │ 全部 fork 子进程  │
    │ 外部: fork   │   │ 创建 N-1 根管道   │
    │ 重定向支持    │   │ dup2 连接管道端   │
    │              │   │ 内建需 exit()     │
    └──────────────┘   └──────────────────┘
```

- **单命令路径**：内建命令在父进程直接运行（避免 fork 开销），外部命令 fork 子进程执行
- **管道路径**：所有命令一律 fork 子进程，通过 `dup2` 将管道读写端连接到标准输入/输出，父进程关闭所有管道 fd 后回收子进程

### 模块划分

| 文件 | 职责 |
|---|---|
| `myshell.c` | 主入口，REPL 循环，命令解析，fork/exec，管道编排，信号处理 |
| `builtins.c` / `.h` | 内建命令实现（`cd`、`exit`、`history`、`export`、`alias`），历史持久化 |
| `redirect.c` / `.h` | `apply_redirect()` 解析 `>` / `>>` / `<`，`dup2` 重定向；`restore_redirect()` 恢复父进程 |
| `alias.c` / `.h` | 别名表管理（最大 128 条），`expand_alias()` 做 `args[0]` 字符串替换 |
| `wildcard.c` / `.h` | `expand_wildcards()` 用 `glob(3)` 展开 `*` / `?` 为匹配的文件名列表 |

### REPL 循环执行顺序

1. 打印彩色提示符（绿色/红色箭头指示上条命令成败）
2. Readline 读取输入 → 加入内存历史 → 持久化到磁盘
3. `strtok` 按空格切分为 `args[]`
4. `expand_alias()` — 检查并替换别名
5. `expand_wildcards()` — 展开通配符
6. 扫描 `|` → 分支进入单命令或管道执行路径
7. 回收子进程，更新 `last_status` 供下一轮提示符颜色

## 项目演进

按提交顺序逐步实现的功能：

1. **基础 REPL** — 死循环读取输入、空格切分、内建命令、外部程序执行
2. **输出重定向 `>`** — 文件描述符操作，`dup2` 换道
3. **追加重定向 `>>`** — `O_APPEND` 标志
4. **输入重定向 `<`** — 从文件读取标准输入
5. **管道符 `|`** — 多进程 + 管道 + `dup2` 串联，最大 64 级
6. **彩色提示符** — ANSI 转义序列，根据上条命令成败切换颜色
7. **历史持久化** — 文件读写 + 内存环形缓冲区
8. **`export` 命令** — `setenv` 设置环境变量
9. **通配符** — `glob(3)` 展开 `*` 和 `?`
10. **别名系统** — 简易别名表，覆盖检查
11. **Readline 集成** — 替换 `fgets`，支持行编辑、Ctrl+F 搜索、上下键历史导航

## 文件清单

```
.
├── Makefile          # 编译配置
├── myshell.c         # 主程序（REPL、fork/exec、管道编排）
├── builtins.c / .h   # 内建命令 + 历史持久化
├── redirect.c / .h   # 输入/输出重定向
├── alias.c / .h      # 别名系统
├── wildcard.c / .h   # 通配符展开
├── test_pipe.sh      # 管道集成测试脚本
├── CLAUDE.md         # Claude Code 项目指南
└── README.md         # 本文件
```

## 技术要点

- **并发**：fork + exec + waitpid 的标准 Unix 进程模型
- **IPC**：`pipe()` 创建匿名管道，`dup2()` 重定向文件描述符
- **文件 I/O**：`open()` + `dup2()` 实现重定向，`O_TRUNC` / `O_APPEND` 区分覆盖与追加
- **内存管理**：`strdup()` 分配、`free()` 释放、`globfree()` 清理 glob 结果
- **信号**：父进程 `SIG_IGN` 忽略 SIGINT，子进程 `SIG_DFL` 默认处理，确保 `Ctrl+C` 正确的行为
- **持久化**：Readline 的 `read_history()` / `write_history()` / `append_history()` API

## 许可证

本项目仅用于学习目的。
