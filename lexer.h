#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// Token 类型枚举：区分普通单词和 shell 运算符
typedef enum {
    TOK_WORD,          // 普通单词（命令名、参数等）
    TOK_PIPE,          // |
    TOK_REDIR_OUT,     // >
    TOK_REDIR_APPEND,  // >>
    TOK_REDIR_IN,      // <
} TokenType;

// 词法分析器
// 将 input 字符串解析为 tokens，结果写入 buf，args 指针指向 buf 中的各 token。
// types 数组记录每个 token 的类型。
// 返回 token 数量，-1 表示错误（如未闭合的引号）。
int lex(const char *input, char *buf, int buf_size,
        char **args, TokenType *types, int max_args);

#endif
