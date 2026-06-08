#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alias.h"

#define MAX_ALIASES 128
static Alias alias_table[MAX_ALIASES];
static int alias_count = 0;

// 内建命令实现：alias ll='ls -l' 或者是单敲 alias
int builtin_alias(char **args) {
    // 1. 如果只输入 alias，打印当前所有别名
    if (args[1] == NULL) {
        for (int i = 0; i < alias_count; i++) {
            printf("alias %s='%s'\n", alias_table[i].name, alias_table[i].value);
        }
        return 0;
    }

    // 2. 解析 alias ll=ls -l (简化处理，假设用户没有打单引号，或者参数已被切分)
    // 为了健壮性，我们解析第一个参数里是否包含 '='
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');
    if (equal_sign == NULL) {
        fprintf(stderr, "myshell: alias: invalid syntax\n");
        return 1;
    }

    *equal_sign = '\0';
    char *name = arg;
    char *value = equal_sign + 1;

    // 3. 检查是否已经存在同名别名，存在则覆盖
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_table[i].name, name) == 0) {
            free(alias_table[i].value);
            alias_table[i].value = strdup(value);
            return 0;
        }
    }

    // 4. 不存在则存入新别名
    if (alias_count < MAX_ALIASES) {
        alias_table[alias_count].name = strdup(name);
        alias_table[alias_count].value = strdup(value);
        alias_count++;
    }
    return 0;
}

// 别名替换器
void expand_alias(char **args) {
    if (args[0] == NULL) return;

    // 查表
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(args[0], alias_table[i].name) == 0) {
            // 发现了别名！比如 args[0] 是 "ll"，而 value 是 "ls -l"
            // 在工业级工程里，"ls -l" 应当被重新切分为 "ls" 和 "-l" 插入 args 数组前面。
            // 为了展示核心原理，我们这里做一个最直观的单词“快捷方式”替换：
            args[0] = alias_table[i].value; 
            break;
        }
    }
}