# fastLEC 调试指南

## 概述

fastLEC是一个逻辑等价性检查工具，支持多种调试方法来定位和解决问题。

## 快速开始

### 1. 基本调试流程

```bash
# 1. 构建调试版本
./build.sh debug

# 2. 使用调试脚本
./debug.sh gdb        # GDB调试
./debug.sh valgrind   # 内存调试
./debug.sh asan       # AddressSanitizer
```

### 2. 检查调试环境

```bash
# 检查调试工具是否安装
./debug_utils.sh check
```

## 调试方法详解

### 1. GDB调试器

#### 基本使用
```bash
# 启动GDB
gdb ./build/bin/fastLEC

# 在GDB中设置断点
(gdb) break main
(gdb) break fastLEC::Task::set_state
(gdb) break src/pSAT_task.cpp:97

# 运行程序
(gdb) run -i ../ins/all/test_14_TOP30.aiger -m hybrid_sweeping -c 16 -v 2 -t 500
```

#### 常用GDB命令
```bash
# 执行控制
(gdb) next          # 执行下一行
(gdb) step          # 进入函数
(gdb) continue      # 继续执行
(gdb) finish        # 执行完当前函数

# 查看信息
(gdb) print variable_name    # 打印变量
(gdb) backtrace             # 查看调用栈
(gdb) info locals          # 查看局部变量
(gdb) info args            # 查看函数参数

# 线程调试
(gdb) info threads         # 查看线程
(gdb) thread 2            # 切换到线程2
(gdb) thread apply all bt # 查看所有线程的调用栈
```

#### 高级GDB技巧
```bash
# 条件断点
(gdb) break src/pSAT_task.cpp:97 if state == SATISFIABLE

# 观察点
(gdb) watch task_state

# 命令序列
(gdb) commands 1
> print task_state
> continue
> end

# 多线程调试
(gdb) set scheduler-locking on
(gdb) thread apply all bt
```

### 2. Valgrind内存调试

#### 内存泄漏检查
```bash
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes \
         ./build/bin/fastLEC [参数]
```

#### 线程错误检查
```bash
# 检查竞争条件
valgrind --tool=helgrind ./build/bin/fastLEC [参数]

# 检查线程错误
valgrind --tool=drd ./build/bin/fastLEC [参数]
```

#### Valgrind选项说明
```bash
--leak-check=full      # 完整内存泄漏检查
--show-leak-kinds=all  # 显示所有类型的泄漏
--track-origins=yes    # 跟踪未初始化值的来源
--suppressions=file    # 使用抑制文件
```

### 3. AddressSanitizer (ASan)

#### 构建ASan版本
```bash
./build.sh sanitize
```

#### 运行ASan
```bash
# 基本运行
./build/bin/fastLEC [参数]

# 带环境变量
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
./build/bin/fastLEC [参数]
```

#### ASan环境变量
```bash
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:check_initialization_order=1
```

### 4. 性能分析

#### 使用perf
```bash
# 记录性能数据
perf record ./build/bin/fastLEC [参数]

# 查看报告
perf report

# 实时监控
perf top -p $(pgrep fastLEC)
```

#### 使用gprof
```bash
# 需要重新编译（添加-pg选项）
g++ -pg -g -O2 ...

# 运行程序
./build/bin/fastLEC [参数]

# 分析结果
gprof ./build/bin/fastLEC gmon.out
```

### 5. 系统调用跟踪

#### 使用strace
```bash
# 基本跟踪
strace ./build/bin/fastLEC [参数]

# 保存到文件
strace -o trace.log ./build/bin/fastLEC [参数]

# 跟踪特定系统调用
strace -e trace=memory ./build/bin/fastLEC [参数]
```

### 6. 核心转储分析

#### 启用核心转储
```bash
ulimit -c unlimited
echo "core.%p" > /proc/sys/kernel/core_pattern
```

#### 分析核心转储
```bash
# 使用GDB分析
gdb ./build/bin/fastLEC core.1234

# 在GDB中
(gdb) bt
(gdb) info registers
(gdb) thread apply all bt
```

## 常见问题调试

### 1. 内存问题

#### 内存泄漏
```bash
# 使用Valgrind检查
valgrind --leak-check=full ./build/bin/fastLEC [参数]

# 使用ASan检查
./build.sh sanitize
ASAN_OPTIONS=detect_leaks=1 ./build/bin/fastLEC [参数]
```

#### 内存访问错误
```bash
# 使用ASan
./build.sh sanitize
./build/bin/fastLEC [参数]

# 使用Valgrind
valgrind --tool=memcheck ./build/bin/fastLEC [参数]
```

### 2. 线程问题

#### 竞争条件
```bash
# 使用Helgrind
valgrind --tool=helgrind ./build/bin/fastLEC [参数]

# 使用DRD
valgrind --tool=drd ./build/bin/fastLEC [参数]
```

#### 死锁检测
```bash
# 在GDB中
(gdb) thread apply all bt
(gdb) info threads
```

### 3. 性能问题

#### CPU使用率高
```bash
# 使用perf分析热点
perf record -g ./build/bin/fastLEC [参数]
perf report

# 使用gprof
gprof ./build/bin/fastLEC gmon.out
```

#### 内存使用高
```bash
# 监控内存使用
watch -n 1 'cat /proc/$(pgrep fastLEC)/status | grep -E "(VmSize|VmRSS)"'

# 使用Valgrind的massif工具
valgrind --tool=massif ./build/bin/fastLEC [参数]
```

## 调试脚本使用

### 1. 主调试脚本 (debug.sh)
```bash
./debug.sh gdb        # GDB调试
./debug.sh valgrind   # Valgrind内存调试
./debug.sh asan       # AddressSanitizer
./debug.sh helgrind   # 线程竞争检查
./debug.sh core       # 核心转储分析
```

### 2. 调试辅助工具 (debug_utils.sh)
```bash
./debug_utils.sh check           # 检查调试环境
./debug_utils.sh core core.1234  # 分析核心转储
./debug_utils.sh memory 1234     # 内存使用分析
./debug_utils.sh thread 1234     # 线程分析
./debug_utils.sh perf            # 性能分析工具
```

## 调试最佳实践

### 1. 调试前准备
- 确保使用调试构建版本 (`./build.sh debug`)
- 检查调试工具是否安装
- 设置适当的环境变量

### 2. 问题定位策略
1. **崩溃问题**: 使用GDB + 核心转储
2. **内存问题**: 使用Valgrind或ASan
3. **线程问题**: 使用Helgrind/DRD
4. **性能问题**: 使用perf或gprof

### 3. 调试技巧
- 使用条件断点减少干扰
- 结合多种工具进行交叉验证
- 保存调试日志和配置文件
- 使用版本控制管理调试脚本

### 4. 常见错误处理
- **段错误**: 检查指针使用和数组越界
- **内存泄漏**: 检查资源释放和RAII使用
- **死锁**: 检查锁的获取顺序
- **性能问题**: 分析算法复杂度和数据结构选择

## 调试配置文件

项目包含以下调试配置文件：
- `.gdbinit`: GDB配置文件
- `debug.sh`: 主调试脚本
- `debug_utils.sh`: 调试辅助工具
- `test.sh`: 测试脚本（包含调试选项）

## 总结

fastLEC项目提供了完整的调试工具链，支持从基本的GDB调试到高级的性能分析。选择合适的调试方法取决于具体的问题类型。建议从简单的GDB调试开始，然后根据需要使用更专业的工具。
