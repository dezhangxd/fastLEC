#!/bin/bash

# fastLEC 调试脚本
# 使用方法: ./debug.sh [模式] [参数]

MODE=${1:-"gdb"}
BUILD_DIR="build"
COMMAND="-m hybrid_sweeping -c 16 -v 2 -t 500"
INPUT_FILE="../ins/all/test_14_TOP30.aiger"

# 构建调试版本
echo "=== 构建调试版本 ==="
./build.sh debug

case $MODE in
    "gdb")
        echo "=== 使用GDB调试 ==="
        echo "启动GDB调试器..."
        echo "常用命令:"
        echo "  (gdb) break main"
        echo "  (gdb) break fastLEC::Task::set_state"
        echo "  (gdb) run -i $INPUT_FILE $COMMAND"
        echo "  (gdb) next, step, continue, print, backtrace"
        echo ""
        gdb ./${BUILD_DIR}/bin/fastLEC
        ;;
    
    "valgrind")
        echo "=== 使用Valgrind内存调试 ==="
        echo "检查内存泄漏和错误..."
        valgrind --leak-check=full --show-leak-kinds=all \
                 --track-origins=yes \
                 ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        ;;
    
    "asan")
        echo "=== 使用AddressSanitizer ==="
        echo "构建Sanitize版本..."
        ./build.sh sanitize
        echo "运行AddressSanitizer..."
        ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
        ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        ;;
    
    "helgrind")
        echo "=== 使用Helgrind线程调试 ==="
        echo "检查线程竞争条件..."
        valgrind --tool=helgrind \
                 ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        ;;
    
    "drd")
        echo "=== 使用DRD线程调试 ==="
        echo "检查线程错误..."
        valgrind --tool=drd \
                 ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        ;;
    
    "profile")
        echo "=== 性能分析 ==="
        echo "使用gprof进行性能分析..."
        # 需要先构建带-pg选项的版本
        gprof ./${BUILD_DIR}/bin/fastLEC gmon.out
        ;;
    
    "trace")
        echo "=== 函数调用跟踪 ==="
        echo "使用strace跟踪系统调用..."
        strace -o trace.log ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        echo "跟踪日志保存到 trace.log"
        ;;
    
    "core")
        echo "=== 生成核心转储文件 ==="
        echo "启用核心转储..."
        ulimit -c unlimited
        echo "运行程序，如果崩溃会生成core文件"
        ./${BUILD_DIR}/bin/fastLEC -i $INPUT_FILE $COMMAND
        echo "使用以下命令分析core文件:"
        echo "gdb ./${BUILD_DIR}/bin/fastLEC core"
        ;;
    
    *)
        echo "用法: $0 [模式]"
        echo ""
        echo "可用模式:"
        echo "  gdb      - GDB调试器 (默认)"
        echo "  valgrind - Valgrind内存调试"
        echo "  asan     - AddressSanitizer"
        echo "  helgrind - Helgrind线程调试"
        echo "  drd      - DRD线程调试"
        echo "  profile  - 性能分析"
        echo "  trace    - 系统调用跟踪"
        echo "  core     - 生成核心转储"
        echo ""
        echo "示例:"
        echo "  $0 gdb"
        echo "  $0 valgrind"
        echo "  $0 asan"
        ;;
esac
