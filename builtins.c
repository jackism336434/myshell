#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "builtins.h"


// 【新增】全局静态变量，用于存放历史命令（只对当前文件可见，保证封装性）
static char history_books[MAX_HISTORY][1024];
static int history_count = 0;




// 1. 实现具体的内建命令
static int builtin_cd(char **args) {
    char *target_dir = args[1];

    // 1. 如果用户没有输入参数（直接输入 cd）
    if (target_dir == NULL) {
        // 去获取当前用户的家目录路径
        target_dir = getenv("HOME");
        
        // 安全检查：防止万一系统没有配置 HOME 变量
        if (target_dir == NULL) {
            fprintf(stderr, "myshell: cd: HOME not set\n");
            return 1;
        }
    }

    // 2. 统一调用 chdir 进行目录切换
    if (chdir(target_dir) != 0) {
        perror("myshell: cd");
    }
    return 1;
}

static int builtin_exit(char **args) {
    printf("Goodbye!\n");
    (void)args;
    return 0;
}



// 【新增】实现保存历史命令的函数
void add_to_history(const char *cmd) {
    // 过滤掉空输入或纯空格
    if (cmd == NULL || strlen(cmd) == 0) return;

    // 如果记满了，这里采用最简单的覆盖逻辑（高级做法可以用循环队列，我们先求稳）
    if (history_count < MAX_HISTORY) {
        strncpy(history_books[history_count], cmd, 1024);
        history_count++;
    } else {
        // 往前平移一行，腾出最后一行
        for (int i = 1; i < MAX_HISTORY; i++) {
            strcpy(history_books[i-1], history_books[i]);
        }
        strncpy(history_books[MAX_HISTORY-1], cmd, 1024);
    }
}

//builtin history命令
static int builtin_history(char **args) {
    (void)args; // 暂时用不到参数，用来消除“未使用变量”的编译器警告
    for (int i = 0; i < history_count; i++) {
        // 模仿标准 Bash 的排版：序号 + 命令
        printf(" %4d  %s\n", i + 1, history_books[i]);
    }
    return 1;
}


// 2. 内部的命令路由表
static struct builtin builtins[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"history", builtin_history}
};

static int num_builtins() {
    return sizeof(builtins) / sizeof(struct builtin);
}

// 3. 对外提供的统一接口
int handle_builtin(char **args, int *is_builtin) {
    *is_builtin = 0;
    
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            *is_builtin = 1;         // 标记：这确实是一个内建命令
            return builtins[i].func(args); // 执行并返回状态
        }
    }
    
    return 1; // 不是内建命令，默认返回 1 让主循环继续
}