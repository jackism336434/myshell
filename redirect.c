#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "redirect.h"

int apply_redirect(char **args, RedirectContext *ctx, int is_child) {
    // 初始化上下文
    ctx->file = NULL;
    ctx->redirect_idx = -1;
    ctx->saved_stdout = -1;
    int is_append = 0; // 默认是覆写模式

    // 1. 扫描 `>` 符号
    for (int i = 0; args[i] != NULL; i++) {
       if (strcmp(args[i], ">>") == 0) {
            ctx->redirect_idx = i;
            ctx->file = args[i + 1];
            is_append = 1; // 这是追加模式
            break;
        } else if (strcmp(args[i], ">") == 0) {
            ctx->redirect_idx = i;
            ctx->file = args[i + 1];
            is_append = 0; // 这是覆写模式
            break;
        }
    }

    // 2. 如果没找到 `>`，说明不需要重定向，平安无事返回成功
    if (ctx->redirect_idx == -1) {
        return 1; 
    }

    // 3. 安全性检查
    if (ctx->file == NULL) {
        fprintf(stderr, "myshell: syntax error near unexpected token `newline'\n");
        return 0; // 返回失败
    }

    // 4. 斩断 args 数组，剥离出干净的命令参数
    args[ctx->redirect_idx] = NULL;



    int flags = O_WRONLY | O_CREAT;
    if (is_append) {
        flags |= O_APPEND; // 叠加上 O_APPEND
    } else {
        flags |= O_TRUNC;  
    }



   // 打开文件
    int fd = open(ctx->file, flags, 0644);
    if (fd < 0) {
        perror("myshell: open failed");
        return 0;
    }

    // 6. 根据是父进程还是子进程，决定要不要备份现场
    if (!is_child) {
        // 如果是父进程（运行内建命令），必须先备份当前的屏幕 FD 1
        ctx->saved_stdout = dup(STDOUT_FILENO);
    }

    // 7. 重定向标准输出到文件
    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("myshell: dup2 failed");
        close(fd);
        return 0;
    }
    close(fd); // 用完关闭

    return 1;
}




void restore_redirect(RedirectContext *ctx) {
    // 只有当备份过现场（saved_stdout >= 0）时，才需要还原
    if (ctx->saved_stdout >= 0) {
        dup2(ctx->saved_stdout, STDOUT_FILENO); // 把屏幕通道接回来
        close(ctx->saved_stdout);              // 关闭备份副本
        ctx->saved_stdout = -1;
    }
}