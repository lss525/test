#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>

void zhixing(char **args);
void guandao(char **args1, char **args2);
void jiexi(char *line, char **args);
int zhaoguandao(char **args);

// 执行命令
void zhixing(char **args){
    pid_t pid = fork();
    
    if(pid == 0){
        // 子进程
        if(execvp(args[0], args) == -1){
            printf("命令找不到: %s\n", args[0]);
        }
        exit(0);
    } else if(pid < 0){
        printf("创建进程失败\n");
    } else {
        // 父进程等待
        wait(NULL);
    }
}

// 执行管道命令
void guandao(char **args1, char **args2){
    int fd[2];
    pid_t pid1, pid2;
    
    if(pipe(fd) == -1){
        printf("创建管道失败\n");
        return;
    }
    
    // 第一个命令 ls
    pid1 = fork();
    if(pid1 == 0){
        // 把输出写到管道
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        
        if(execvp(args1[0], args1) == -1){
            printf("命令找不到: %s\n", args1[0]);
        }
        exit(0);
    }
    
    // 第二个命令 grep
    pid2 = fork();
    if(pid2 == 0){
        // 从管道读输入
        dup2(fd[0], 0);
        close(fd[1]);
        close(fd[0]);
        
        if(execvp(args2[0], args2) == -1){
            printf("命令找不到: %s\n", args2[0]);
        }
        exit(0);
    }
    
    // 父进程关管道
    close(fd[0]);
    close(fd[1]);
    
    // 等两个子进程
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// 解析输入
void jiexi(char *line, char **args){
    int i = 0;
    char *token = strtok(line, " \t\n");
    
    while(token != NULL && i < 99){
        args[i] = token;
        i++;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

// 找管道符号
int zhaoguandao(char **args){
    int i = 0;
    while(args[i] != NULL){
        if(strcmp(args[i], "|") == 0){
            return i;
        }
        i++;
    }
    return -1;
}

int main(){
    char line[1024];
    char *args[100];
    char cwd[1024];
    int i, pos;
    
    printf("我的小shell，输入exit退出\n");
    printf("试试: ls -l | grep .c\n");
    
    while(1){
        // 显示当前目录
        getcwd(cwd, sizeof(cwd));
        printf("%s $ ", cwd);
        
        // 读输入
        if(fgets(line, sizeof(line), stdin) == NULL){
            break;
        }
        
        // 去换行
        line[strlen(line)-1] = '\0';
        
        // 空命令继续
        if(strlen(line) == 0){
            continue;
        }
        
        // 解析命令
        jiexi(line, args);
        
        // 处理exit
        if(strcmp(args[0], "exit") == 0){
            printf("拜拜\n");
            break;
        }
        
        // 处理cd
        if(strcmp(args[0], "cd") == 0){
            if(args[1] == NULL){
                chdir(getenv("HOME"));
            } else {
                if(chdir(args[1]) != 0){
                    printf("目录不存在\n");
                }
            }
            continue;
        }
        
        // 找管道
        pos = zhaoguandao(args);
        if(pos > 0){
            // 有管道，分成两个命令
            char *args1[100];
            char *args2[100];
            
            // 第一个命令
            for(i = 0; i < pos; i++){
                args1[i] = args[i];
            }
            args1[i] = NULL;
            
            // 第二个命令
            for(i = pos + 1; args[i] != NULL; i++){
                args2[i - pos - 1] = args[i];
            }
            args2[i - pos - 1] = NULL;
            
            // 执行管道
            guandao(args1, args2);
        } else {
            // 没管道直接执行
            zhixing(args);
        }
    }
    
    return 0;
}