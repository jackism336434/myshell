#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "builtins.h"
#include "redirect.h"
#include "alias.h"




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

    // 使用 readline 内置函数从文件加载历史
    read_history(history_path);
    // 限制内存中保存的历史条数
    stifle_history(MAX_HISTORY);
}

// 每次输入时：磁盘实时追加（readline 已在内存中管理历史）
void add_to_history(const char *cmd) {
    // 过滤掉空输入
    if (cmd == NULL || strlen(cmd) == 0) return;

    // 追加最后一条命令到磁盘文件
    char history_path[1024];
    get_history_path(history_path, sizeof(history_path));
    append_history(1, history_path);
}

//builtin history命令
static int builtin_history(char **args) {
    (void)args; // 暂时用不到参数，用来消除”未使用变量”的编译器警告
    HIST_ENTRY **hist_list = history_list();
    if (hist_list) {
        for (int i = 0; hist_list[i] != NULL; i++) {
            // history_base 是第一条记录的编号（通常为 1）
            printf(" %4d  %s\n", i + history_base, hist_list[i]->line);
        }
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
    {"export", builtin_export},
    {"alias", builtin_alias}
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