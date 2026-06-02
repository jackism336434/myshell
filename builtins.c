#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#include "builtins.h"
#include "redirect.h"

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
        return 1;
    }
    return 0;
}

static int builtin_exit(char **args) {
    printf("Goodbye!\n");
    (void)args;
    return 0;
}





// 助手函数：获取历史记录文件的绝对路径（避免硬编码）
// ========================================================
static void get_history_path(char *path_buf, size_t buf_size) {
    char *home = getenv("HOME");
    if (home != NULL) {
        // 如果能拿到主目录，拼装成 /home/user/.myshell_history
        snprintf(path_buf, buf_size, "%s/.myshell_history", home);
    } else {
        // 极少发生的保底情况：如果拿不到 HOME，就生在当前目录下
        snprintf(path_buf, buf_size, ".myshell_history");
    }
}

// 1. 启动时加载历史记录
// ========================================================
void load_history_from_file() {
    char history_path[1024];
    get_history_path(history_path, sizeof(history_path));

    FILE *file = fopen(history_path, "r"); // 以只读模式打开
    if (file == NULL) {
        // 如果文件不存在s，直接返回，不报错
        return;
    }

    char line[1024];
    // 逐行读取文件，直到读完或者内存数组装满
    while (fgets(line, sizeof(line), file) != NULL && history_count < MAX_HISTORY) {
        // 去掉从文件读出来的末尾换行符 \n
        line[strcspn(line, "\n")] = '\0';
        
        // 如果读取的不是空行，载入内存
        if (strlen(line) > 0) {
            strncpy(history_books[history_count], line, 1024);
            history_count++;
        }
    }

    fclose(file);
}

// 每次输入时：内存追加 + 磁盘实时追加
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


    // 磁盘文件实时追加
    char history_path[1024];
    get_history_path(history_path, sizeof(history_path));

    FILE *file = fopen(history_path, "a"); // "a" 为追加模式
    if (file != NULL) {
        fprintf(file, "%s\n", cmd);  
        fclose(file);               // 写入 Page Cache，速度极快
    }
}

//builtin history命令
static int builtin_history(char **args) {
    (void)args; // 暂时用不到参数，用来消除“未使用变量”的编译器警告
    for (int i = 0; i < history_count; i++) {
        // 模仿标准 Bash 的排版：序号 + 命令
        printf(" %4d  %s\n", i + 1, history_books[i]);
    }
    return 0;
}



// export命令
static int builtin_export(char **args) {
    // 1. 如果只输入了 export 没带参数，标准做法是打印当前所有的环境变量
    if (args[1] == NULL) {
        extern char **environ;
        for (int i = 0; environ[i] != NULL; i++) {
            printf("%s\n", environ[i]);
        }
        return 0; 
    }

    // 2. 解析 export MY_VAR=123 这种格式
    // 我们需要在这个字符串里寻找 '='
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');

    if (equal_sign == NULL) {
        // 说明用户输的是 export MY_VAR 这种没有赋值的
        return 0;
    }

    *equal_sign = '\0'; // 把 '=' 强行改成 '\0'，这样把原字符串一分为二了
    char *name = arg;            // 左半部分变成了变量名
    char *value = equal_sign + 1; // 右半部分变成了变量值

    // 3. 调用系统核心函数注入环境变量
    if (setenv(name, value, 1) != 0) {
        perror("myshell: export");
        return 1; // 失败
    }

    return 0; // 成功
}



// 2. 内部的命令路由表
static struct builtin builtins[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"history", builtin_history},
    {"export", builtin_export}
};

static int num_builtins() {
    return sizeof(builtins) / sizeof(struct builtin);
}

// 3. 对外提供的统一接口
int handle_builtin(char **args, int *is_builtin,int *cmd_status) {
    *is_builtin = 0;
    *cmd_status = 0; //指示命令是成功还是失败

    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            *is_builtin = 1;  // 取1则为内建命令
            // 内建命令的重定向生命周期，在内部自我闭环
            
            RedirectContext ctx;


            // is_child 传 0
            // 如果应用失败），这里返回 1 保证主循环不崩，但命令不实际执行
            if (!apply_redirect(args, &ctx, 0)) {
                *cmd_status = 1;
                return 1; 
            }
            
            // 真正调用对应的内建函数
            int func_ret= builtins[i].func(args);
            
            // 执行完毕，立刻还原现场
            restore_redirect(&ctx);
            

            // 特殊处理 exit 拦截
            if (strcmp(builtins[i].name, "exit") == 0) {
                return 0; // 如果不处理会导致exit无法正常退出
            }
            *cmd_status = func_ret;
            return 1;
            
        }
    }
    
    return 1; // 不是内建命令，默认返回 1 让主循环继续
}