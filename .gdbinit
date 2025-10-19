# fastLEC GDB配置文件

# 设置断点
break main
break fastLEC::Task::set_state
break fastLEC::Task::terminate_info_upd

# 设置有用的别名
alias bt = backtrace
alias p = print
alias n = next
alias s = step
alias c = continue
alias q = quit

# 显示线程信息
set print thread-events on

# 设置断点条件
# break src/pSAT_task.cpp:97 if state == SATISFIABLE

# 显示源代码
set listsize 20

# 自动显示变量
set print pretty on
set print array on
set print array-indexes on

# 显示结构体成员
set print union on

# 设置历史记录
set history save on
set history filename ~/.gdb_history

echo "fastLEC GDB配置已加载\n"
echo "常用命令:"
echo "  bt - 显示调用栈"
echo "  p variable - 打印变量"
echo "  n - 下一行"
echo "  s - 进入函数"
echo "  c - 继续执行"
echo "  info threads - 显示线程"
echo "  thread N - 切换到线程N"
echo ""
