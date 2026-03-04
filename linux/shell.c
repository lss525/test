#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_SIZE 64
#define MAX_PATH_SIZE 256

void print_prompt();
int parse_input(char *input, char **args);
void execute_command(char **args);
int inminlin(char **args);
void handle_redirection(char **args);
void handle_pipe(char *input);

// 内置命令处理
int inminlin(char **args) {
    if(strcmp(args[0], "cd")==0) {
        if(args[1]==NULL) {
            chdir(getenv("HOME"));
        }
        else {
            if(chdir(args[1])!=0) {
                perror("cd失败");
            }
        }
        return 1; 
    }
    else if(strcmp(args[0], "pwd")==0) {
        char cwd[MAX_PATH_SIZE];
        if(getcwd(cwd, sizeof(cwd))!=NULL) {
            printf("%s\n", cwd);
        }
        return 1;
    }
    else if(strcmp(args[0], "help")==0) {
        printf("命令：cd, pwd, exit, quit, help\n");
        printf("支持：重定向 (> < >>) 和 管道 (|)\n");
        return 1;
    }
    return 0; 
}

void handle_redirection(char **args){
    int i=0;
    int input_fd=-1, output_fd=-1;
    
    while(args[i]!=NULL) {
        if(strcmp(args[i], ">")==0) {
            args[i] = NULL;
            output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(output_fd == -1) {
                perror("重定向失败");
                return;
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        else if(strcmp(args[i], ">>") == 0) {
            args[i] = NULL;
            output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(output_fd == -1) {
                perror("重定向失败");
                return;
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        else if(strcmp(args[i], "<") == 0) {
            args[i] = NULL;
            input_fd = open(args[i+1], O_RDONLY);
            if(input_fd == -1) {
                perror("重定向失败");
                return;
            }
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        i++;
    }
}

// 管道处理
void handle_pipe(char *input) {
    char *commands[10];
    int num_commands = 0;
    
    char *token = strtok(input, "|");
    while(token != NULL && num_commands < 10) {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }
    
    if(num_commands == 1) {
        // 无管道，直接解析执行
        char *args[MAX_ARG_SIZE];
        parse_input(commands[0], args);
        if(args[0] != NULL) {
            execute_command(args);
        }
        return;
    }
    
    // 多管道处理
    int i;
    int prev_pipe[2];
    int next_pipe[2];
    
    for(i = 0; i < num_commands; i++) {
        char *args[MAX_ARG_SIZE];
        parse_input(commands[i], args);
        
        // 创建管道（除了最后一个命令）
        if(i < num_commands - 1) {
            if(pipe(next_pipe) == -1) {
                perror("管道创建失败");
                return;
            }
        }
        
        pid_t pid = fork();
        if(pid == 0) {
            // 子进程
            if(i > 0) {
                // 从上一个管道读取
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            
            if(i < num_commands - 1) {
                // 写入下一个管道
                dup2(next_pipe[1], STDOUT_FILENO);
                close(next_pipe[0]);
                close(next_pipe[1]);
            }
            
            handle_redirection(args);
            execvp(args[0], args);
            perror("命令执行失败");
            exit(1);
        }
        else if(pid < 0) {
            perror("fork失败");
            return;
        }
        
        // 父进程：关闭管道描述符
        if(i > 0) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        
        if(i < num_commands - 1) {
            prev_pipe[0] = next_pipe[0];
            prev_pipe[1] = next_pipe[1];
        }
    }
    
    // 等待所有子进程结束
    for(i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

// 解析输入
int parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " \t");
    
    while(token != NULL && i < MAX_ARG_SIZE - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;
    return i;
}

// 执行命令
void execute_command(char **args) {
    pid_t pid = fork();
    
    if(pid < 0) {
        perror("fork失败");
    } 
    else if(pid == 0) {
        // 子进程
        handle_redirection(args);
        
        if(execvp(args[0], args) == -1) {
            printf("命令未找到: %s\n", args[0]);
            exit(1);
        }
    } 
    else {
        // 父进程等待
        int status;
        waitpid(pid, &status, 0);
    }
}

// 打印提示符（带当前目录）
void print_prompt() {
    char cwd[MAX_PATH_SIZE];
    getcwd(cwd, sizeof(cwd));
    printf("\033[1;32m%s\033[0m\033[1;36m$\033[0m ", cwd);
    fflush(stdout);
}

// 主函数
int main() {
    char input[MAX_INPUT_SIZE];
    
    printf("\033[1;34mMYShell！\033[0m\n");
    printf("输入 'help' 查看帮助，'exit' 退出\n\n");
    
    while(1) {
        print_prompt();
        
        // 读取输入
        if(fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // 移除换行符
        input[strcspn(input, "\n")] = 0;
        
        // 跳过空输入
        if(strlen(input) == 0) {
            continue;
        }
        
        // 检查是否包含管道
        if(strchr(input, '|') != NULL) {
            char input_copy[MAX_INPUT_SIZE];
            strcpy(input_copy, input);
            handle_pipe(input_copy);
            continue;
        }
        
        // 解析命令
        char *args[MAX_ARG_SIZE];
        int arg_count = parse_input(input, args);
        
        if(arg_count == 0) {
            continue;
        }
        
        // 检查退出命令
        if(strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
            printf("\033[1;31m再见！\033[0m\n");
            break;
        }
        
        // 处理内置命令
        if(!handle_builtin(args)) {
            // 执行外部命令
            execute_command(args);
        }
    }
    return 0;
}