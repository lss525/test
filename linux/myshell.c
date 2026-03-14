#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>

char dangqianmulu[1024];
char shangcimulu[1024];
int youmeiyouhoutai;

// 处理ctrl+c
void chuliCTRLC(int xinhao){
    if(xinhao == SIGINT){
        printf("\n");
        printf("%s $ ", dangqianmulu);
        fflush(stdout);
    }
}

// 执行命令
void zhixing(char **args){
    pid_t pid = fork();
    
    if(pid == 0){
        // 子进程
        signal(SIGINT, SIG_DFL);
        if(execvp(args[0], args) == -1){
            printf("命令找不到: %s\n", args[0]);
        }
        exit(0);
    } 
    else if(pid < 0){
        printf("创建进程失败\n");
    } 
    else {
        // 父进程等待
        if(!youmeiyouhoutai){
            wait(NULL);
        } 
        else {
            printf("后台运行%d\n", pid);
        }
    }
}

// 执行重定向
void chongdingxiang(char **args){
    int i = 0;
    int shuru_fd = -1;
    int shuchu_fd = -1;
    int zhuijia = 0;
    char *shuru_wenjian = NULL;
    char *shuchu_wenjian = NULL;
    
    while(args[i] != NULL){
        if(strcmp(args[i], "<") == 0){
            args[i] = NULL;
            shuru_wenjian = args[i+1];
            i++;
        } 
        else if(strcmp(args[i], ">") == 0){
            args[i] = NULL;
            shuchu_wenjian = args[i+1];
            zhuijia = 0;
            i++;
        } 
        else if(strcmp(args[i], ">>") == 0){
            args[i] = NULL;
            shuchu_wenjian = args[i+1];
            zhuijia = 1;
            i++;
        }
        i++;
    }
    
    pid_t pid = fork();
    if(pid == 0){
        if(shuru_wenjian != NULL){
            shuru_fd = open(shuru_wenjian, O_RDONLY);
            if(shuru_fd < 0){
                printf("打开文件失败: %s\n", shuru_wenjian);
                exit(1);
            }
            dup2(shuru_fd, 0);
            close(shuru_fd);
        }
        
        if(shuchu_wenjian != NULL){
            if(zhuijia){
                shuchu_fd = open(shuchu_wenjian, O_WRONLY|O_CREAT|O_APPEND, 0644);
            } 
            else {
                shuchu_fd = open(shuchu_wenjian, O_WRONLY|O_CREAT|O_TRUNC, 0644);
            }
            if(shuchu_fd < 0){
                printf("创建文件失败: %s\n", shuchu_wenjian);
                exit(1);
            }
            dup2(shuchu_fd, 1);
            close(shuchu_fd);
        }
        
        if(execvp(args[0], args) == -1){
            printf("命令找不到: %s\n", args[0]);
        }
        exit(0);
    } 
    else if(pid < 0){
        printf("创建进程失败\n");
    } 
    else {
        if(!youmeiyouhoutai){
            wait(NULL);
        } 
        else {
           
            printf("[后台运行] %d\n", pid);
        }
    }
}

// 执行管道
void guandao(char **args1, char **args2){
    int fd[2];
    pid_t pid1, pid2;
    
    if(pipe(fd) == -1){
        printf("创建管道失败\n");
        return;
    }
    
    pid1 = fork();
    if(pid1 == 0){
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        
        if(execvp(args1[0], args1) == -1){
            printf("命令找不到: %s\n", args1[0]);
        }
        exit(0);
    }
    
    pid2 = fork();
    if(pid2 == 0){
        dup2(fd[0], 0);
        close(fd[1]);
        close(fd[0]);
        
        if(execvp(args2[0], args2) == -1){
            printf("命令找不到: %s\n", args2[0]);
        }
        exit(0);
    }
    
    close(fd[0]);
    close(fd[1]);
    
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// 执行带管道的重定向
void guandaochongdingxiang(char **args1, char **args2, 
                           char *shuru_wenjian, char *shuchu_wenjian, int zhuijia){
    int fd[2];
    int shuru_fd = -1;
    int shuchu_fd = -1;
    pid_t pid1, pid2;
    
    if(pipe(fd) == -1){
        printf("创建管道失败\n");
        return;
    }
    
    pid1 = fork();
    if(pid1 == 0){
        if(shuru_wenjian != NULL){
            shuru_fd = open(shuru_wenjian, O_RDONLY);
            if(shuru_fd < 0){
                printf("打开文件失败: %s\n", shuru_wenjian);
                exit(1);
            }
            dup2(shuru_fd, 0);
            close(shuru_fd);
        }
        
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        
        if(execvp(args1[0], args1) == -1){
            printf("命令找不到: %s\n", args1[0]);
        }
        exit(0);
    }
    
    pid2 = fork();
    if(pid2 == 0){
        dup2(fd[0], 0);
        close(fd[1]);
        close(fd[0]);
        
        if(shuchu_wenjian != NULL){
            if(zhuijia){
                shuchu_fd = open(shuchu_wenjian, O_WRONLY|O_CREAT|O_APPEND, 0644);
            } 
            else {
                shuchu_fd = open(shuchu_wenjian, O_WRONLY|O_CREAT|O_TRUNC, 0644);
            }
            if(shuchu_fd < 0){
                printf("创建文件失败: %s\n", shuchu_wenjian);
                exit(1);
            }
            dup2(shuchu_fd, 1);
            close(shuchu_fd);
        }
        
        if(execvp(args2[0], args2) == -1){
            printf("命令找不到: %s\n", args2[0]);
        }
        exit(0);
    }
    
    close(fd[0]);
    close(fd[1]);
    
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

// 找重定向符号
int zhaochongdingxiang(char **args, char **shuru, char **shuchu, int *zhuijia){
    int i = 0;
    *shuru = NULL;
    *shuchu = NULL;
    *zhuijia = 0;
    
    while(args[i] != NULL){
        if(strcmp(args[i], "<") == 0){
            args[i] = NULL;
            if(args[i+1] != NULL){
                *shuru = args[i+1];
            }
            i++;
        } 
        else if(strcmp(args[i], ">") == 0){
            args[i] = NULL;
            if(args[i+1] != NULL){
                *shuchu = args[i+1];
                *zhuijia = 0;
            }
            i++;
        } 
        else if(strcmp(args[i], ">>") == 0){
            args[i] = NULL;
            if(args[i+1] != NULL){
                *shuchu = args[i+1];
                *zhuijia = 1;
            }
            i++;
        }
        i++;
    }
    
    if(*shuru != NULL || *shuchu != NULL){
        return 1;
    }
    return 0;
}

// 找后台符号
int zhaohoutai(char **args){
    int i = 0;
    while(args[i] != NULL){
        if(strcmp(args[i], "&") == 0){
            args[i] = NULL;
            return 1;
        }
        i++;
    }
    return 0;
}

int main(){
    char line[4096];
    char *args[100];
    char *args1[100];
    char *args2[100];
    char cwd[1024];
    int i, pos, youmeiyouzhongdingxiang;
    char *shuru_wenjian, *shuchu_wenjian;
    int zhuijia;
    
    getcwd(dangqianmulu, sizeof(dangqianmulu));
    strcpy(shangcimulu, dangqianmulu);
    
    signal(SIGINT, chuliCTRLC);
    

    printf("   我的shell\n");
    printf("支持: 管道 |  重定向 < > >>  后台运行 &\n");
    
    while(1){
        getcwd(dangqianmulu, sizeof(dangqianmulu));
        printf("%s $ ", dangqianmulu);
        fflush(stdout);
        
        if(fgets(line, sizeof(line), stdin) == NULL){
            printf("\n");
            break;
        }
        
        line[strlen(line)-1] = '\0';
        
        if(strlen(line) == 0){
            continue;
        }
        
        jiexi(line, args);
        
        if(strcmp(args[0], "exit") == 0){
            printf("拜\n");
            break;
        }
        
        if(strcmp(args[0], "cd") == 0){
            if(args[1] == NULL || strcmp(args[1], "~") == 0){
                chdir(getenv("HOME"));
            } else if(strcmp(args[1], "-") == 0){
                if(strlen(shangcimulu) > 0){
                    printf("%s\n", shangcimulu);
                    chdir(shangcimulu);
                } else {
                    printf("没有上一个目录\n");
                }
            } else {
                if(chdir(args[1]) != 0){
                    printf("目录不存在: %s\n", args[1]);
                }
            }
            getcwd(dangqianmulu, sizeof(dangqianmulu));
            strcpy(shangcimulu, dangqianmulu);
            continue;
        }
        
        if(strcmp(args[0], "pwd") == 0){
            printf("%s\n", dangqianmulu);
            continue;
        }
        
        if(strcmp(args[0], "echo") == 0){
            for(i = 1; args[i] != NULL; i++){
                printf("%s ", args[i]);
            }
            printf("\n");
            continue;
        }
        
        youmeiyouhoutai = zhaohoutai(args);
        
        pos = zhaoguandao(args);
        if(pos > 0){
            for(i = 0; i < pos; i++){
                args1[i] = args[i];
            }
            args1[i] = NULL;
            
            for(i = pos + 1; args[i] != NULL; i++){
                args2[i - pos - 1] = args[i];
            }
            args2[i - pos - 1] = NULL;
            
            // 检查第一个命令有没有重定向
            char *shuru1, *shuchu1;
            int zhuijia1;
            int you1 = zhaochongdingxiang(args1, &shuru1, &shuchu1, &zhuijia1);
            
            // 检查第二个命令有没有重定向
            char *shuru2, *shuchu2;
            int zhuijia2;
            int you2 = zhaochongdingxiang(args2, &shuru2, &shuchu2, &zhuijia2);
            
            if(you1 || you2){
                guandaochongdingxiang(args1, args2, shuru1, shuchu2, zhuijia2);
            } else {
                guandao(args1, args2);
            }
        } else {
            youmeiyouzhongdingxiang = zhaochongdingxiang(args, &shuru_wenjian, 
                                                         &shuchu_wenjian, &zhuijia);
            if(youmeiyouzhongdingxiang){
                chongdingxiang(args);
            } else {
                zhixing(args);
            }
        }
    }
    
    return 0;
}