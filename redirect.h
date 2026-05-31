#ifndef REDIRECT_H
#define REDIRECT_H

// 定义一个结构体，用来存放重定向的状态，方便在函数间传递
typedef struct {
    char *file;         // 重定向的目标文件名
    int redirect_idx;   // '>' 在 args 数组中的下标
    int saved_stdout;
    int saved_stdin;   
} RedirectContext;

// 1. 解析参数数组并应用重定向（内部处理 open 和 dup2）
// 内建命令is_child传0.
int apply_redirect(char **args, RedirectContext *ctx, int is_child);

// 2. 恢复原本的屏幕输出现场
void restore_redirect(RedirectContext *ctx);

#endif
