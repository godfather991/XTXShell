# Makefile for building and running the shell project

# 项目文件和目标
TARGET = xtxshell
# SRC = new_shell.c
SRC = xtxshell.c
OBJ = $(SRC:.c=.o)

# 编译器和选项
CC = gcc
CFLAGS = -Wall -g -I/usr/include
LDFLAGS = -lreadline -L/usr/lib

# 检查依赖
all: check_readline $(TARGET) cleanobj

# 编译目标
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# 编译源文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 检查 readline 是否已安装
check_readline:
	@if ! ldconfig -p | grep -q libreadline; then \
	    echo "libreadline not found! Installing..."; \
	    sudo apt-get update && sudo apt-get install -y libreadline-dev; \
	else \
	    echo "libreadline is already installed."; \
	fi

# 清理构建
clean:
	rm -f $(OBJ) $(TARGET)

# 清理obj
cleanobj:
	rm -f $(OBJ)

# 运行 Shell 程序
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run check_readline