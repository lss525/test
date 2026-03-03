#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64


void mycommand(char *input) {
    pid_t pid;
    char *args[MAX_ARGS];
    int i = 0;
    // 分割命令参数
    char *token = strtok(input, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    
    // 创建子进程执行命令
    pid = fork();
    if (pid == 0) {
        // 子进程
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
    }
} 

// 检查是否为内置命令
int is_builtin(char *command) {
    char *builtins[] = {"cd", "help", "exit", "pwd", "echo"};
    for (int i = 0; i < 5; i++) {
        if (strcmp(command, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// 执行内置命令
void execute_builtin(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            chdir(getenv("HOME"));
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
        }
    } else if (strcmp(args[0], "help") == 0) {
        printf("简单的 Shell 实现\n");
        printf("支持的命令: cd, pwd, echo, exit, help\n");
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    } else if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s ", args[i]);
        }
        printf("\n");
    }
}

void print_prompt() {
    printf("myshell> ");
    fflush(stdout);
}

int main() {
    char input[MAX_INPUT];
    
    while (1) {
        print_prompt();
        
        // 读取用户输入
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // 去除换行符
        input[strcspn(input, "\n")] = 0;
        
        // 跳过空行
        if (strlen(input) == 0) {
            continue;
        }
        
        // 退出命令
        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        // 执行命令
        execute_command(input);
    }
    
    return 0;
}