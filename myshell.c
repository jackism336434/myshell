#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // 提供 getcwd 函数
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>  // 提供 open 函数


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



        // 文件重定向功能





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


                // 处理重定向
               
        char *redirect_file = NULL;
        int redirect_idx = -1;

        // 遍历参数数组，寻找 ">"
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0) {
                redirect_idx = i;
                redirect_file = args[i + 1]; // ">" 的下一个参数就是文件名
                break;
            }
        }

        // 如果找到了 ">" 符号
        if (redirect_idx != -1) {
            if (redirect_file == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token `newline'\n");
                exit(EXIT_FAILURE);
            }

            // 1. 打开目标文件
            // O_WRONLY: 只写模式
            // O_CREAT: 如果文件不存在则创建
            // O_TRUNC: 如果文件存在，先清空（擦除）内容
            // 0644: 创建文件时的标准 Linux 权限（所有者读写，其他人只读）
            int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("myshell: open file failed");
                exit(EXIT_FAILURE);
            }

            // 2. 偷天换日：把标准输出 (1) 重定向到我们打开的文件描述符 (fd) 上
            if (dup2(fd, STDOUT_FILENO) < 0) { // STDOUT_FILENO 就是 1
                perror("myshell: dup2 failed");
                exit(EXIT_FAILURE);
            }

            // 3. 关闭用完了的旧文件描述符，保持整洁
            close(fd);

            // 4. 关键：把 args 数组中的 ">" 及其后面的文件名“斩断”
            // 比如将 {"ls", "-l", ">", "out.txt", NULL} 
            // 变成 {"ls", "-l", NULL, "out.txt", NULL}
            // 这样随后的 execvp 才会只看到 "ls -l"，而不会因为看到 ">" 而报错
            args[redirect_idx] = NULL;
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
        }

                
            }

    return 0;
}