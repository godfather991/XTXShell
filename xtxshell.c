#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <limits.h> // for PATH_MAX

#define MAX_CMD_LEN 1024
#define MAX_ARGS 128
#define HISTORY_FILE ".xtxshell_history"
#define HISTORY_LIMIT 1000
#define PROMPT_COLOR "\033[1;32m"  // 绿色
#define RESET_COLOR  "\033[0m"     // 重置颜色

// 全局变量
pid_t bg_processes[128];
int bg_count = 0;
// char history[MAX_HISTORY][MAX_CMD_LEN];
int history_count = 0;

typedef struct BackgroundProcess {
    pid_t pid;
    char command[256];
    struct BackgroundProcess *next;
} BackgroundProcess;

BackgroundProcess *bg_process_list = NULL;

void sigint_handler(int sig) {
    // 捕获 Ctrl+C 信号，打印提示符，并不退出 Shell
    printf("\nInterrupted! Press Ctrl+D to exit.\n");
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return;
    }
    printf("%s%s%s$ %s",
                 PROMPT_COLOR, cwd, RESET_COLOR, RESET_COLOR);
}

void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 查找并从后台进程列表中移除
        BackgroundProcess **curr = &bg_process_list;
        while (*curr) {
            if ((*curr)->pid == pid) {
                printf("\n[Process %d finished]\n", pid);
                BackgroundProcess *to_free = *curr;
                *curr = (*curr)->next;
                free(to_free);
                break;
            }
            curr = &((*curr)->next);
        }
    }
}

void load_history() {
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), HISTORY_FILE);

    // 从文件加载历史记录
    if (read_history(history_path) != 0) {
        fprintf(stderr, "No previous history found.\n");
    }

    // 设置最大历史条目数
    stifle_history(HISTORY_LIMIT);
}

void save_history() {
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), HISTORY_FILE);

    // 将历史记录写入文件
    if (write_history(history_path) != 0) {
        fprintf(stderr, "Failed to save history.\n");
    }
}

void add_background_process(pid_t pid, char *command) {
    BackgroundProcess *new_process = malloc(sizeof(BackgroundProcess));
    new_process->pid = pid;
    strncpy(new_process->command, command, 255);
    new_process->command[255] = '\0';
    new_process->next = bg_process_list;
    bg_process_list = new_process;
    printf("[Started background process: %d]\n", pid);
}

void print_jobs() {
    BackgroundProcess *curr = bg_process_list;
    int job_number = 1;
    while (curr) {
        printf("[%d] %d %s\n", job_number++, curr->pid, curr->command);
        curr = curr->next;
    }
    if (job_number == 1) {
        printf("No background jobs.\n");
    }
}
// // 添加到历史记录
// void add_to_history(char *input) {
//     if (history_count < MAX_HISTORY) {
//         strcpy(history[history_count++], input);
//     }
// }

// // 打印历史记录
// void show_history() {
//     for (int i = 0; i < history_count; i++) {
//         printf("%d: %s\n", i + 1, history[i]);
//     }
// }

// 解析输入为命令和参数
int parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " \t\n");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

// 处理环境变量
void handle_env_vars(char **args) {
    if (strcmp(args[0], "export") == 0 && args[1]) {
        char *var = strtok(args[1], "=");
        char *val = strtok(NULL, "=");
        if (var && val) {
            setenv(var, val, 1);
        } else {
            printf("Usage: export VAR=value\n");
        }
    } else {
        for (int i = 0; args[i] != NULL; i++) {
            if (args[i][0] == '$') {
                char *val = getenv(args[i] + 1);
                if (val) {
                    args[i] = val; // 替换变量值
                }
            }
        }
    }
}

// 执行单条命令
void execute_command(char **args, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        if (background) setpgid(0, 0); // 后台运行

        // 检查输入/输出重定向
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0) {
                int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[i] = NULL;
            } else if (strcmp(args[i], "<") == 0) {
                int fd = open(args[i + 1], O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[i] = NULL;
            }
        }
        execvp(args[0], args);
        perror("command not found or");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (background) {
            // printf("[Started background process: %d]\n", pid);
            add_background_process(pid, args[0]);
        } else {
            waitpid(pid, NULL, 0);
        }
    } else {
        perror("fork");
    }
}

// 执行带管道的命令
void execute_pipeline(char *input) {
    char *commands[10];
    int num_commands = 0;

    // 分解管道
    commands[num_commands++] = strtok(input, "|");
    while ((commands[num_commands++] = strtok(NULL, "|")) != NULL);

    int pipes[2], in_fd = 0;

    for (int i = 0; i < num_commands - 1; i++) {
        pipe(pipes);
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO);
            if (i < num_commands - 2) {
                dup2(pipes[1], STDOUT_FILENO);
            }
            close(pipes[0]);
            close(pipes[1]);

            // 执行命令
            char *args[MAX_ARGS];
            parse_input(commands[i], args);
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
            waitpid(pid, NULL, 0);
            close(pipes[1]);
            in_fd = pipes[0];
        }
    }
}

// 实现内建命令 `cd`
void change_directory(char *path) {
    if (!path) {
        fprintf(stderr, "cd: missing argument\n");
    } else if (chdir(path) != 0) {
        perror("cd");
    }
}

// 实现内建命令 `pwd`
void print_working_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

// 函数：设置环境变量
void set_env_variable(char *command) {
    char *key = strtok(command, "=");
    char *value = strtok(NULL, "\n");
    if (key && value) {
        setenv(key, value, 1);
        printf("Exported: %s=%s\n", key, value);
    } else {
        fprintf(stderr, "Invalid export syntax. Use: export VAR=value\n");
    }
}

// 函数：替换命令中的环境变量
void replace_env_variables(char *command) {
    char result[MAX_CMD_LEN] = {0};
    size_t result_len = 0;
    char *start = command;
    char *pos;

    while ((pos = strstr(start, "${")) != NULL) {
        // 复制到${前的部分
        size_t prefix_len = pos - start;
        if (result_len + prefix_len >= MAX_CMD_LEN - 1) {
            fprintf(stderr, "Error: command too long after expansion\n");
            return;
        }
        strncat(result, start, prefix_len);
        result_len += prefix_len;

        char *end = strchr(pos, '}');
        if (end == NULL) {
            fprintf(stderr, "Syntax error: missing '}'\n");
            return;
        }

        // 提取变量名
        char var_name[128] = {0};
        size_t var_len = end - pos - 2;
        if (var_len >= sizeof(var_name)) {
            fprintf(stderr, "Error: variable name too long\n");
            return;
        }
        strncpy(var_name, pos + 2, var_len);

        // 获取环境变量值
        char *value = getenv(var_name);
        if (value) {
            size_t value_len = strlen(value);
            if (result_len + value_len >= MAX_CMD_LEN - 1) {
                fprintf(stderr, "Error: command too long after expansion\n");
                return;
            }
            strcat(result, value);
            result_len += value_len;
        }

        // 更新起始位置
        start = end + 1;
    }

    // 复制剩余部分
    if (result_len + strlen(start) >= MAX_CMD_LEN - 1) {
        fprintf(stderr, "Error: command too long after expansion\n");
        return;
    }
    strcat(result, start);
    strcpy(command, result);
}

// 主循环
void shell_loop() {
    signal(SIGCHLD, sigchld_handler); // 处理后台作业信号
    while (1) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            break;
        }
        
        // 修复的提示符生成代码
        char prompt[PATH_MAX + 50];
        snprintf(prompt, sizeof(prompt),
                 "%s%s%s$ %s",
                 PROMPT_COLOR, cwd, RESET_COLOR, RESET_COLOR);

        char *input = readline(prompt);
        if (!input) { // 检测到 EOF（Ctrl+D）
            printf("\nGoodbye!\n");
            break;
        }

        // 去除输入中的前后空白字符
        char *trimmed_input = input;
        while (*trimmed_input == ' ') trimmed_input++; // 移动到第一个非空白字符
        if (strlen(trimmed_input) == 0) { // 空输入，直接继续
            free(input);
            continue;
        }

        if (strlen(trimmed_input) > 0) {
            add_history(input);
        }
        // 检查后台运行符号 &
        int background = 0;
        if (strchr(input, '&')) {
            background = 1;
            *strchr(input, '&') = '\0';
        }

        // 检查管道
        if (strchr(input, '|')) {
            execute_pipeline(input);
            free(input);
            continue;
        }

        // 解析输入
        char *args[MAX_ARGS];
        replace_env_variables(input);
        parse_input(input, args);

        // 内建命令
        if (strcmp(args[0], "exit") == 0) {
            free(input);
            break;
        // } else if (strcmp(args[0], "history") == 0) {
        //     show_history();
        //     free(input);
        //     continue;
        } else if (strcmp(args[0], "jobs") == 0) {
            print_jobs();
            free(input);
            continue;
        } else if (strcmp(args[0], "pwd") == 0) {
            print_working_directory();
            free(input);
            continue;
        } else if (strcmp(args[0], "cd") == 0) {
            change_directory(args[1]);
            free(input);
            continue;
        } else if (strcmp(args[0], "export") == 0) {
            set_env_variable(args[1]);
            free(input);
            continue;
        }

        // 处理环境变量
        handle_env_vars(args);

        // 执行命令
        execute_command(args, background);
        free(input);
    }
}

// 主函数
int main() {
    // 捕获 SIGCHLD 信号
    signal(SIGCHLD, sigchld_handler);
    // 捕获 SIGINT 信号
    signal(SIGINT, sigint_handler); // 忽略 Ctrl+C（可以改成自定义处理）
    printf("Welcome to XtxShell!\n");
    load_history();
    shell_loop();
    save_history();
    return 0;
}
