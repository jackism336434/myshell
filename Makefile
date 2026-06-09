# 定义编译器和编译选项
CC = gcc
CFLAGS = -Wall -Wextra -g

# 定义目标可执行文件名和源文件
TARGET = myshell
SRC = myshell.c builtins.c redirect.c wildcard.c alias.c

# 默认目标：编译生成可执行文件
all: $(TARGET)

LDFLAGS = -lreadline

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# 运行伪目标：编译并直接启动 shell
run: $(TARGET)
	./$(TARGET)

# 测试伪目标：自动化测试
# 使用 echo "exit" 模拟用户输入，验证 shell 能否正常读取、处理并优雅退出
test: $(TARGET)
	@echo "=== Running Automated Test ==="
	@echo "exit" | ./$(TARGET)
	@echo "=== Test Passed (Clean Exit) ==="

# 清理伪目标：删除生成的可执行文件和调试符号
clean:
	rm -f $(TARGET)

# 声明伪目标，防止与同名文件冲突
.PHONY: all run test clean