#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // 提供 getcwd 函数
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "builtins.h"


#define MAX_CMD_LEN 1024
#define MAX_ARG_COUNT 64
int main() {

    //定义全局变量
    char path[MAX_CMD_LEN];
    char input[MAX_CMD_LEN];
    char *args[MAX_ARG_COUNT];
    
    
    //父进程忽略ctrl+c的信号
    signal(SIGINT, SIG_IGN);

    
    
    // 进入 Shell 的核心死循环 (REPL)
    while (1) {
        // 1. 获取并打印当前工作路径
        if (getcwd(path, sizeof(path)) != NULL) {
            printf("[MyShell] %s $ ", path);
        } else {
            perror("getcwd failed");
            printf("[MyShell] unknown $ ");
        }
        
        // 强行刷新缓冲区，确保提示符立刻显示
        fflush(stdout);

        // 2. 等待用户输入
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // 处理 Ctrl+D (EOF) 情况
            printf("\n");
            break;
        }
        // 3. 去掉末尾的回车符 '\n'
        input[strcspn(input, "\n")] = '\0';
        
        add_to_history(input);
        
        int i=0;
        // 1. 切分第一个 Token
        char *token = strtok(input, " ");


        while (token != NULL && i < MAX_ARG_COUNT - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");    // strtok 后续调用第一个参数要传 NULL
        }


        args[i] = NULL; // 必须以 NULL 结尾

        if (i == 0) {
    // 用户只输入了空格或者直接回车，不做处理，跳回循环开头
        continue; 
        }

// 4. 此时再安全的进行命令判断
        if (strcmp(args[0], "exit") == 0) {
        break;
        }

        //test
        int j=0;
        while(1){
            if(args[j]==NULL){
                break;
            }
            printf("%s\n",args[j]);
            j++;
        }

        // 4. 退出条件
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        int is_builtin = 0;
        int loop_status = handle_builtin(args, &is_builtin);
    
        if (is_builtin) {
        if (loop_status == 0) break; // 如果是 exit，退出 Shell
        continue;                    // 如果是 cd，直接进入下一轮循环
        }

        // 创建子进程，执行命令

        pid_t pid = fork(); // 创建子进程

        if (pid < 0) {
            // 1. 创建子进程失败
            perror("fork failed");
        } 
        else if (pid == 0) {
            // 2. 这里是子进程的空间！
            //在子进程中激活ctrl+c的信号，保证异常时能退出返回到父进程
            signal(SIGINT, SIG_DFL);
            // args[0] 是命令名（如 "ls"），args 是完整的参数数组（如 {"ls", "-l", NULL}）
            if (execvp(args[0], args) < 0) {
                perror("execvp failed"); 
                exit(1); // 失败后必须退出子进程，否则子进程会回到你的 Shell 死循环里，变成两个 Shell！
            }

        } 
        else {
            // 3. 这里是父进程（你的 Shell）的空间！
            // pid 此时是子进程的进程 ID。我们需要等待子进程结束。
            int status;
            waitpid(pid, &status, 0); // 挂起父进程，直到指定的子进程执行完毕
        }

                
            }

    return 0;
}