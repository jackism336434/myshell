//条件编译指令，防止重复引用
#ifndef BUILTINS_H
#define BUILTINS_H
#define MAX_HISTORY 100 // 最多保存 100 条历史记录
// 声明内建命令的公共路由结构体，用于之后查表
struct builtin {
    char *name;
    int (*func)(char **args);
};

// 声明对外公开的查表和执行接口
// 传入用户命令参数，如果是内建命令则执行并返回 1（继续）或 0（退出），并把 is_builtin 设为 1
// 如果不是内建命令，返回 1，并将 is_builtin 设为 0
int handle_builtin(char **args, int *is_builtin);
// 【新增】允许主程序把用户输入的原始命令传进来保存
void add_to_history(const char *cmd);
#endif