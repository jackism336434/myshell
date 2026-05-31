#!/bin/bash

# ==============================================================================
# MyShell 管道符自动化测试脚本
# ==============================================================================

# 1. 准备测试用的脏数据文件
echo "Generating dummy data..."
echo -e "apple\nbanana\ncherry\nblueberry\nremedy\nstrawberry" > test_fruits.txt
echo -e "123\n456\n789\n12345" > test_nums.txt

echo "------------------------------------------------"
echo "Running Pipe Tests on MyShell..."
echo "------------------------------------------------"

# 2. 定义一个核心测试函数
# 参数 1: 测试的名字
# 参数 2: 灌给 MyShell 的管道命令字符串
run_test() {
    local test_name=$1
    local cmd_string=$2

    echo -n "Testing [ $test_name ] ... "

    # 核心黑魔法：利用输入重定向，把管道命令加一个 exit 灌给我们的可执行程序 ./myshell
    # 并且把 stdout 重定向到临时文件，把 stderr 扔掉
    echo -e "${cmd_string}\nexit" | ./myshell > temp_myshell.out 2>/dev/null

    # 同样的命令，在原生的标准 Bash 里也跑一次，作为“标准答案”
    echo -e "${cmd_string}" | bash > temp_bash.out 2>/dev/null

    # 过滤掉 MyShell 彩色提示符带来的视觉干扰，只对比命令真正的输出
    # （注：如果你的提示符带彩色代码，可能需要根据实际输出用 grep 或 sed 简单过滤，
    #   这里我们假设对比命令最后几行的输出差异）
    
    # 用 diff 工具对比两个输出文件的异同
    if diff -q temp_myshell.out temp_bash.out > /dev/null; then
        echo -e "\033[0;32m[ PASSED ]\033[0m"
    else
        # 如果不一致，说明管道实现有缺陷，或者输出了多余的东西
        echo -e "\033[0;31m[ FAILED ]\033[0m"
        echo "   --> Expected (Standard Bash):"
        cat temp_bash.out | sed 's/^/       /'
        echo "   --> Got (Your MyShell):"
        cat temp_myshell.out | sed 's/^/       /'
    fi
}

# ==============================================================================
# 3. 正式注入压测用例
# ==============================================================================

# 用例一：内建 ➜ 外部
#run_test "Builtin to External" "history | grep test"

# 用例二：外部 ➜ 外部（带参数）
run_test "External to External" "cat test_fruits.txt | grep berry"

# 用例三：多级管道长链（3级）
run_test "Multi-stage Pipe" "cat test_fruits.txt | grep berry | wc -l"

# 用例四：管道搭配输出重定向（终极缝合怪）
run_test "Pipe with Redirect" "cat test_nums.txt | grep 123 > result.txt"


# 4. 打扫战场，擦除临时文件
rm -f temp_myshell.out temp_bash.out test_fruits.txt test_nums.txt
echo "------------------------------------------------"
echo "Test Session Finished."