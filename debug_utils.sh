#!/bin/bash

# fastLEC 调试辅助工具

# 检查调试环境
check_debug_env() {
    echo "=== 检查调试环境 ==="
    
    # 检查GDB
    if command -v gdb &> /dev/null; then
        echo "✓ GDB: $(gdb --version | head -n1)"
    else
        echo "✗ GDB 未安装"
    fi
    
    # 检查Valgrind
    if command -v valgrind &> /dev/null; then
        echo "✓ Valgrind: $(valgrind --version)"
    else
        echo "✗ Valgrind 未安装"
    fi
    
    # 检查编译器调试支持
    if gcc -v 2>&1 | grep -q "gcc version"; then
        echo "✓ GCC: $(gcc --version | head -n1)"
    else
        echo "✗ GCC 未找到"
    fi
    
    # 检查核心转储设置
    CORE_LIMIT=$(ulimit -c)
    if [ "$CORE_LIMIT" = "unlimited" ]; then
        echo "✓ 核心转储: 已启用"
    else
        echo "✗ 核心转储: 未启用 (当前限制: $CORE_LIMIT)"
        echo "  运行: ulimit -c unlimited"
    fi
    
    echo ""
}

# 分析核心转储文件
analyze_core() {
    local core_file=${1:-"core"}
    local binary="./build/bin/fastLEC"
    
    if [ ! -f "$core_file" ]; then
        echo "核心转储文件 $core_file 不存在"
        return 1
    fi
    
    if [ ! -f "$binary" ]; then
        echo "可执行文件 $binary 不存在"
        return 1
    fi
    
    echo "=== 分析核心转储文件: $core_file ==="
    echo "使用以下命令进行详细分析:"
    echo "gdb $binary $core_file"
    echo ""
    echo "在GDB中运行:"
    echo "  (gdb) bt"
    echo "  (gdb) info registers"
    echo "  (gdb) thread apply all bt"
    echo ""
    
    # 自动分析
    gdb -batch -ex "bt" -ex "info registers" -ex "quit" $binary $core_file
}

# 内存使用分析
memory_analysis() {
    local pid=$1
    
    if [ -z "$pid" ]; then
        echo "请提供进程ID"
        echo "用法: memory_analysis <PID>"
        return 1
    fi
    
    echo "=== 内存使用分析 (PID: $pid) ==="
    
    # 基本内存信息
    echo "--- /proc/$pid/status ---"
    grep -E "(VmSize|VmRSS|VmPeak|VmHWM)" /proc/$pid/status 2>/dev/null || echo "进程不存在"
    
    echo ""
    echo "--- /proc/$pid/maps ---"
    head -20 /proc/$pid/maps 2>/dev/null || echo "无法访问内存映射"
    
    echo ""
    echo "--- 内存使用趋势 ---"
    echo "使用以下命令监控内存使用:"
    echo "watch -n 1 'cat /proc/$pid/status | grep -E \"(VmSize|VmRSS)\"'"
}

# 线程分析
thread_analysis() {
    local pid=$1
    
    if [ -z "$pid" ]; then
        echo "请提供进程ID"
        echo "用法: thread_analysis <PID>"
        return 1
    fi
    
    echo "=== 线程分析 (PID: $pid) ==="
    
    # 线程数量
    local thread_count=$(ls /proc/$pid/task 2>/dev/null | wc -l)
    echo "线程数量: $thread_count"
    
    # 线程状态
    echo "--- 线程状态 ---"
    for task in /proc/$pid/task/*; do
        if [ -d "$task" ]; then
            local tid=$(basename $task)
            local state=$(cat $task/stat 2>/dev/null | awk '{print $3}')
            echo "线程 $tid: $state"
        fi
    done
    
    echo ""
    echo "使用以下命令进行详细线程分析:"
    echo "gdb -p $pid"
    echo "在GDB中运行:"
    echo "  (gdb) info threads"
    echo "  (gdb) thread apply all bt"
}

# 性能分析
performance_analysis() {
    echo "=== 性能分析工具 ==="
    echo ""
    echo "1. 使用perf进行性能分析:"
    echo "   perf record ./build/bin/fastLEC [参数]"
    echo "   perf report"
    echo ""
    echo "2. 使用gprof进行性能分析:"
    echo "   需要重新编译: g++ -pg ..."
    echo "   ./build/bin/fastLEC [参数]"
    echo "   gprof ./build/bin/fastLEC gmon.out"
    echo ""
    echo "3. 使用htop监控实时性能:"
    echo "   htop -p \$(pgrep fastLEC)"
    echo ""
    echo "4. 使用strace跟踪系统调用:"
    echo "   strace -o trace.log ./build/bin/fastLEC [参数]"
}

# 主函数
case $1 in
    "check")
        check_debug_env
        ;;
    "core")
        analyze_core $2
        ;;
    "memory")
        memory_analysis $2
        ;;
    "thread")
        thread_analysis $2
        ;;
    "perf")
        performance_analysis
        ;;
    *)
        echo "fastLEC 调试辅助工具"
        echo ""
        echo "用法: $0 [命令] [参数]"
        echo ""
        echo "命令:"
        echo "  check           - 检查调试环境"
        echo "  core [文件]     - 分析核心转储文件"
        echo "  memory <PID>    - 内存使用分析"
        echo "  thread <PID>    - 线程分析"
        echo "  perf            - 性能分析工具"
        echo ""
        echo "示例:"
        echo "  $0 check"
        echo "  $0 core core.1234"
        echo "  $0 memory 1234"
        echo "  $0 thread 1234"
        ;;
esac
