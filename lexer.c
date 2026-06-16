#include "lexer.h"

int lex(const char *input, char *buf, int buf_size,
        char **args, TokenType *types, int max_args)
{
    int buf_pos = 0;
    int arg_idx = 0;
    const char *p = input;

    while (*p) {
        // 1. 跳过空白字符（空格、Tab）
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        // 检查是否还有空间存放更多参数
        if (arg_idx >= max_args - 1) break;

        // 2. 识别运算符

        // 管道符 |
        if (*p == '|') {
            types[arg_idx] = TOK_PIPE;
            buf[buf_pos++] = '|';
            buf[buf_pos++] = '\0';
            args[arg_idx++] = &buf[buf_pos - 2];
            p++;
        }
        // 追加重定向 >>（必须优先匹配两个字符，再匹配单个 >）
        else if (*p == '>' && *(p + 1) == '>') {
            types[arg_idx] = TOK_REDIR_APPEND;
            buf[buf_pos++] = '>';
            buf[buf_pos++] = '>';
            buf[buf_pos++] = '\0';
            args[arg_idx++] = &buf[buf_pos - 3];
            p += 2;
        }
        // 覆写重定向 >
        else if (*p == '>') {
            types[arg_idx] = TOK_REDIR_OUT;
            buf[buf_pos++] = '>';
            buf[buf_pos++] = '\0';
            args[arg_idx++] = &buf[buf_pos - 2];
            p++;
        }
        // 输入重定向 <
        else if (*p == '<') {
            types[arg_idx] = TOK_REDIR_IN;
            buf[buf_pos++] = '<';
            buf[buf_pos++] = '\0';
            args[arg_idx++] = &buf[buf_pos - 2];
            p++;
        }
        // 3. 单引号字符串（字面量，无转义，包括空串 ''）
        else if (*p == '\'') {
            types[arg_idx] = TOK_WORD;
            args[arg_idx++] = &buf[buf_pos];
            p++; // 跳过开引号
            while (*p && *p != '\'') {
                if (buf_pos >= buf_size - 1) break;
                buf[buf_pos++] = *p++;
            }
            if (*p == '\'') {
                p++; // 跳过闭引号
            } else {
                return -1; // 未闭合引号
            }
            buf[buf_pos++] = '\0';
        }
        // 4. 双引号字符串（支持转义：\" \\ \n \t \$）
        else if (*p == '"') {
            types[arg_idx] = TOK_WORD;
            args[arg_idx++] = &buf[buf_pos];
            p++; // 跳过开引号
            while (*p && *p != '"') {
                if (buf_pos >= buf_size - 1) break;
                if (*p == '\\' && *(p + 1) != '\0') {
                    p++; // 跳过反斜杠，处理转义字符
                    switch (*p) {
                        case 'n':  buf[buf_pos++] = '\n'; break;
                        case 't':  buf[buf_pos++] = '\t'; break;
                        case '\\': buf[buf_pos++] = '\\'; break;
                        case '"':  buf[buf_pos++] = '"';  break;
                        case '$':  buf[buf_pos++] = '$';  break;
                        default:   buf[buf_pos++] = *p;   break;
                    }
                    p++;
                } else {
                    buf[buf_pos++] = *p++;
                }
            }
            if (*p == '"') {
                p++; // 跳过闭引号
            } else {
                return -1; // 未闭合引号
            }
            buf[buf_pos++] = '\0';
        }
        // 5. 未引号单词（以空白或运算符为界，支持反斜杠转义）
        else {
            types[arg_idx] = TOK_WORD;
            args[arg_idx++] = &buf[buf_pos];
            while (*p && *p != ' ' && *p != '\t' &&
                   *p != '|' && *p != '>' && *p != '<') {
                if (buf_pos >= buf_size - 1) break;
                // 反斜杠转义：下一个字符按字面量处理（可以转义空格和运算符）
                if (*p == '\\' && *(p + 1) != '\0') {
                    p++;
                    buf[buf_pos++] = *p++;
                } else {
                    buf[buf_pos++] = *p++;
                }
            }
            buf[buf_pos++] = '\0';
        }
    }

    args[arg_idx] = NULL;
    return arg_idx;
}
