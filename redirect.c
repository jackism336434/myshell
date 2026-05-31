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
    ctx->saved_stdin = -1;
    
    int mode=0; //0:no,1:>,2:>>,3:<

    // 1. 扫描 `>` 符号
    for (int i = 0; args[i] != NULL; i++) {
       if (strcmp(args[i], ">>") == 0) {
            ctx->redirect_idx = i;
            ctx->file = args[i + 1];
            mode=2; // 这是追加模式
            break;
        } else if (strcmp(args[i], ">") == 0) {
            ctx->redirect_idx = i;
            ctx->file = args[i + 1];
            mode=1; // 这是覆写模式
            break;
        }else if(strcmp(args[i], "<") == 0){
            ctx->redirect_idx = i;
            ctx->file = args[i + 1];
            mode=3; // 这是读取模式
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

    // 2. 根据不同的模式，打开文件并换道
    if (mode == 1 || mode == 2) {
        // 输出重定向流 (>, >>) 
        int flags = O_WRONLY | O_CREAT;
        if (mode == 2) flags |= O_APPEND;
        else           flags |= O_TRUNC;

        int fd = open(ctx->file, flags, 0644);
        if (fd < 0) { perror("myshell: open failed"); return 0; }

        if (!is_child) ctx->saved_stdout = dup(STDOUT_FILENO); // 父进程备份输出

        if (dup2(fd, STDOUT_FILENO) < 0) { perror("myshell: dup2 failed"); close(fd); return 0; }
        close(fd);
    } 
    else if (mode == 3) {
        //  输入重定向流 (<)
        // 
        int fd = open(ctx->file, O_RDONLY);
        if (fd < 0) {
            // 如果用户试图读取一个不存在的文件，友好地报错
            fprintf(stderr, "myshell: %s: No such file or directory\n", ctx->file);
            return 0;
        }

        // 如果是父进程，需要备份标准输入0,防止之后shell的输入被重定向
        if (!is_child) {
            ctx->saved_stdin = dup(STDIN_FILENO);
        }

        // 把标准输入 (STDIN_FILENO) 绑定到文件上
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("myshell: dup2 failed");
            close(fd);
            return 0;
        }
        close(fd);
    }


    return 1;
}




void restore_redirect(RedirectContext *ctx) {
    // 只有当备份过现场（saved_stdout >= 0）时，才需要还原
    if (ctx->saved_stdout >= 0) {
        dup2(ctx->saved_stdout, STDOUT_FILENO); // 把屏幕通道接回来
        close(ctx->saved_stdout);              // 关闭备份副本
        ctx->saved_stdout = -1;
    }

    // 恢复标准输入（新增强壮性防护）
    if (ctx->saved_stdin >= 0) {
        dup2(ctx->saved_stdin, STDIN_FILENO);
        close(ctx->saved_stdin);
        ctx->saved_stdin = -1;
    }
}