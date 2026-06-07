#include <glob.h>
#include <string.h>
#include <stdlib.h>

// 传入原参数数组，将其内部的通配符展开，直接修改原数组
void expand_wildcards(char **args) {
    // 准备一个足够大的新参数数组（比如最大支持 512 个参数）
    char *new_args[512];
    int new_argc = 0;

    for (int i = 0; args[i] != NULL; i++) {
        // 检查参数中是否包含通配符 '*' 或 '?'
        if (strchr(args[i], '*') != NULL || strchr(args[i], '?') != NULL) {
            glob_t glob_res;
            
            // 调用系统库进行展开
            // GLOB_NOCHECK 表示：如果万一没匹配到任何文件（比如压根没有.txt文件），就保留原本的 "*.txt" 字符串
            if (glob(args[i], GLOB_NOCHECK, NULL, &glob_res) == 0) {
                // 把匹配到的所有文件名，依次塞进新参数数组
                for (size_t j = 0; j < glob_res.gl_pathc; j++) {
                    if (new_argc < 511) {
                        new_args[new_argc++] = strdup(glob_res.gl_pathv[j]);
                    }
                }
                globfree(&glob_res); // 释放 glob 内部动态分配的内存
            }
        } else {
            // 普通参数（不带通配符），直接原封不动复制过去
            if (new_argc < 511) {
                new_args[new_argc++] = args[i]; // 或者是 strdup(args[i])
            }
        }
    }
    new_args[new_argc] = NULL; // 别忘了给新数组封底

    // 斩断老数组，把新数组的内容复制回 args 中（注意：这里假设外层的 args 数组有足够的空间）
    for (int i = 0; i <= new_argc; i++) {
        args[i] = new_args[i];
    }
}