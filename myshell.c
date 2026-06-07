#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // 提供 getcwd 函数
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>  // 提供 open 函数


#include "builtins.h"
#include "redirect.h"
#include "wildcard.h"



#define MAX_CMD_LEN 1024
#define MAX_ARG_COUNT 64


// 颜色 ANSI 宏定义
#define COLOR_GREEN  "\033[1;32m"  // 炫酷绿（用于用户名/Shell名）
#define COLOR_BLUE   "\033[1;34m"  // 智慧蓝（用于路径）
#define COLOR_CYAN   "\033[1;36m"  // 青色（可选）
#define COLOR_RED    "\033[1;31m"  // 警告红（用于错误提示）
#define COLOR_RESET  "\033[0m"     // 恢复默认（极为重要！）
int main() {

    //定义全局变量
    char path[MAX_CMD_LEN];
    char input[MAX_CMD_LEN];
    char *args[MAX_ARG_COUNT];
    int last_status = 0; // 记录上一个命令的执行状态，初始为 0 (成功)

    //加载历史命令文件
    load_history_from_file();
    //父进程忽略ctrl+c的信号
    signal(SIGINT, SIG_IGN);

    
    
    // 进入 Shell 的核心死循环 (REPL)
    while (1) {


        // 根据上一个命令的成败，决定箭头的颜色
        char *arrow_color = (last_status == 0) ? COLOR_GREEN : COLOR_RED;
        // 1. 获取并打印当前工作路径
        if(isatty(STDIN_FILENO)){
        if (getcwd(path, sizeof(path)) != NULL) {
           printf("%smyshell%s:%s%s%s %s❯%s ", 
               COLOR_GREEN, COLOR_RESET, 
               COLOR_BLUE, path, COLOR_RESET,arrow_color,COLOR_RESET);
        } else {
            perror("getcwd failed");
            printf("[MyShell] unknown $ ");
        }
        
        // 强行刷新缓冲区，确保提示符立刻显示
        fflush(stdout);
    }
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
    
        continue; 
        }

// 4. 此时再安全的进行命令判断
        if (strcmp(args[0], "exit") == 0) {
        break;
        }

        // //test
        // int j=0;
        // while(1){
        //     if(args[j]==NULL){
        //         break;
        //     }
        //     printf("%s\n",args[j]);
        //     j++;
        // }

        // 4. 退出条件
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }


        // 5. 处理通配符
        expand_wildcards(args);

        // 核心架构层 1：扫描管道符，进行高层分流
       
        int has_pipe = 0;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "|") == 0) {
                has_pipe = 1;
                break;
            }
        }


        if(!has_pipe){
        // builtin命令

        int is_builtin = 0;
        int cmd_status = 0;
        int loop_status = handle_builtin(args, &is_builtin, &cmd_status);
    
        if (is_builtin) {
        if (loop_status == 0) break;            // 如果是 exit，退出 Shell
         last_status = cmd_status;              
        continue;                    // 如果是 其他内建命令，直接进入下一轮循环
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


        // 处理重定向
        RedirectContext ctx;
        if (!apply_redirect(args, &ctx, 1)) {
            exit(EXIT_FAILURE);
        }



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

            // 捕获子进程的退出状态
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status); // 拿到子进程 main 函数的 return 值或 exit 值
            } else {
                last_status = 1; // 异常退出（比如被 Ctrl+C 砍了）
            }
        }

                
            }

        else{
            //分支B：有管道符，需要处理管道
            // 1. 统计命令总数 N (通过数有几个 '|')
            int num_cmds = 0;
            for (int i = 0; args[i] != NULL; i++) {
                if (i == 0 || strcmp(args[i-1], "|") == 0) {
                    num_cmds++;
                }
            }

            // 2. 动态解析，提取每个子命令的起始指针
            char **cmd_args[64]; // 最多支持 64 级管道
            int cmd_idx = 0;
            
            cmd_args[cmd_idx++] = &args[0];
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], "|") == 0) {
                    args[i] = NULL; // 彻底斩断前一个命令的参数尾巴
                    cmd_args[cmd_idx++] = &args[i + 1];
                }
            }


            // 3. 批量创建管道（N 个命令需要 N-1 个管道）
            int num_pipes = num_cmds - 1;
            int pipes[64][2]; // 管道描述符阵列
            for (int i = 0; i < num_pipes; i++) {
                if (pipe(pipes[i]) < 0) {
                    perror("myshell: pipe creation failed");
                    continue;
                }
            }

            // 4. 循环 fork 出 N 个子进程
            pid_t pids[64];
            for (int i = 0; i < num_cmds; i++) {
                pids[i] = fork();
                
                if (pids[i] == 0) { // --------- 子进程 i 的独立世界 ---------
                    signal(SIGINT, SIG_DFL);

                    // 换道 1：如果不是第一个命令，它的标准输入要接上【前一个管道的读端】
                    if (i > 0) {
                        dup2(pipes[i - 1][0], STDIN_FILENO);
                    }
                    // 换道 2：如果不是最后一个命令，它的标准输出要接上【当前管道的写端】
                    if (i < num_cmds - 1) {
                        dup2(pipes[i][1], STDOUT_FILENO);
                    }

                    // 【核心细节】子进程在换道完成后，必须闭合所有管道描述符，防止卡死
                    for (int j = 0; j < num_pipes; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    // 独立享受重定向功能（例如 cmd1 | cmd2 > out.txt）
                    RedirectContext ctx;
                    if (!apply_redirect(cmd_args[i], &ctx, 1)) {
                        exit(EXIT_FAILURE);
                    }

                    // 执行判定：先试内建，不行走外部
                    int is_builtin = 0;
                    int cmd_status = 0;
                    int ret_status = handle_builtin(cmd_args[i], &is_builtin, &cmd_status);
                    if (is_builtin) {
                        // 内建命令在子进程中跑完后，必须通过 exit 释放子进程！
                        exit(ret_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
                    }

                    // 外部命令
                    execvp(cmd_args[i][0], cmd_args[i]);
                    perror("myshell");
                    exit(EXIT_FAILURE);
                }
            }

            // 5. 父进程，必须立刻关闭它手里的所有管道读写端，
            // 否则因为父进程的读端没关闭，所有管道的输入直接卡死在这
            
            for (int i = 0; i < num_pipes; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            // 6. 父进程挂起，依次回收这 N 个子进程，并捕捉最后一个命令的状态
            int last_cmd_status = 0;
            for (int i = 0; i < num_cmds; i++) {
                int status;
                waitpid(pids[i], &status, 0);
                if (i == num_cmds - 1) { // 只关心最后一个命令的成败
                    if (WIFEXITED(status)) {
                        last_cmd_status = WEXITSTATUS(status);
                    } else {
                        last_cmd_status = 1;
                    }
                }
            }
            last_status = last_cmd_status; // 刷新彩色箭头颜色
        }



        }






    return 0;
}