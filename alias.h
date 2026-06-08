#ifndef ALIAS_H
#define ALIAS_H

// 别名键值对结构体
typedef struct {
    char *name;   // 比如 "ll"
    char *value;  // 比如 "ls -l"
} Alias;

// 内建命令调用的函数：添加或打印别名
int builtin_alias(char **args);

// 核心功能：在切分完参数后，对 args[0] 进行别名尝试替换
void expand_alias(char **args);

#endif