#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64


void execute_command(char *input) {
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

        execvp(args[0], args);

        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
 
        wait(NULL);
    } else {
        perror("fork failed");
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