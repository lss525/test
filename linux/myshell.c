#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>

char now[1024];
char old[1024];
int bg;

void hand(int sig){
    if(sig == SIGINT){
        printf("\n");
        printf("%s $ ", now);
        fflush(stdout);
    }
}

void zhixing(char **a){
    pid_t pid = fork();
    
    if(pid == 0){
        signal(SIGINT, SIG_DFL);
        if(execvp(a[0], a) == -1){
            printf("no: %s\n", a[0]);
        }
        exit(0);
    } 
    else if(pid < 0){
        printf("fork error\n");
    } 
    else {
        if(!bg){
            wait(NULL);
        } 
        else {
            printf("[%d]\n", pid);
        }
    }
}

void chongdingxiang(char **a){
    int i = 0;
    int fd1 = -1;
    int fd2 = -1;
    int app = 0;
    char *f1 = NULL;
    char *f2 = NULL;
    
    while(a[i] != NULL){
        if(strcmp(a[i], "<") == 0){
            a[i] = NULL;
            f1 = a[i+1];
            i++;
        } 
        else if(strcmp(a[i], ">") == 0){
            a[i] = NULL;
            f2 = a[i+1];
            app = 0;
            i++;
        } 
        else if(strcmp(a[i], ">>") == 0){
            a[i] = NULL;
            f2 = a[i+1];
            app = 1;
            i++;
        }
        i++;
    }
    
    pid_t pid = fork();
    if(pid == 0){
        if(f1 != NULL){
            fd1 = open(f1, O_RDONLY);
            if(fd1 < 0){
                printf("open fail: %s\n", f1);
                exit(1);
            }
            dup2(fd1, 0);
            close(fd1);
        }
        
        if(f2 != NULL){
            if(app){
                fd2 = open(f2, O_WRONLY|O_CREAT|O_APPEND, 0644);
            } 
            else {
                fd2 = open(f2, O_WRONLY|O_CREAT|O_TRUNC, 0644);
            }
            if(fd2 < 0){
                printf("create fail: %s\n", f2);
                exit(1);
            }
            dup2(fd2, 1);
            close(fd2);
        }
        
        if(execvp(a[0], a) == -1){
            printf("no: %s\n", a[0]);
        }
        exit(0);
    } 
    else if(pid < 0){
        printf("fork error\n");
    } 
    else {
        if(!bg){
            wait(NULL);
        } 
        else {
            printf("[%d]\n", pid);
        }
    }
}

void guandao(char **a1, char **a2){
    int p[2];
    pid_t p1, p2;
    
    if(pipe(p) == -1){
        printf("pipe error\n");
        return;
    }
    
    p1 = fork();
    if(p1 == 0){
        dup2(p[1], 1);
        close(p[0]);
        close(p[1]);
        
        if(execvp(a1[0], a1) == -1){
            printf("no: %s\n", a1[0]);
        }
        exit(0);
    }
    
    p2 = fork();
    if(p2 == 0){
        dup2(p[0], 0);
        close(p[1]);
        close(p[0]);
        
        if(execvp(a2[0], a2) == -1){
            printf("no: %s\n", a2[0]);
        }
        exit(0);
    }
    
    close(p[0]);
    close(p[1]);
    
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

void pipewithred(char **a1, char **a2, char *f1, char *f2, int app){
    int p[2];
    int fd1 = -1;
    int fd2 = -1;
    pid_t p1, p2;
    
    if(pipe(p) == -1){
        printf("pipe error\n");
        return;
    }
    
    p1 = fork();
    if(p1 == 0){
        if(f1 != NULL){
            fd1 = open(f1, O_RDONLY);
            if(fd1 < 0){
                printf("open fail: %s\n", f1);
                exit(1);
            }
            dup2(fd1, 0);
            close(fd1);
        }
        
        dup2(p[1], 1);
        close(p[0]);
        close(p[1]);
        
        if(execvp(a1[0], a1) == -1){
            printf("no: %s\n", a1[0]);
        }
        exit(0);
    }
    
    p2 = fork();
    if(p2 == 0){
        dup2(p[0], 0);
        close(p[1]);
        close(p[0]);
        
        if(f2 != NULL){
            if(app){
                fd2 = open(f2, O_WRONLY|O_CREAT|O_APPEND, 0644);
            } 
            else {
                fd2 = open(f2, O_WRONLY|O_CREAT|O_TRUNC, 0644);
            }
            if(fd2 < 0){
                printf("create fail: %s\n", f2);
                exit(1);
            }
            dup2(fd2, 1);
            close(fd2);
        }
        
        if(execvp(a2[0], a2) == -1){
            printf("no: %s\n", a2[0]);
        }
        exit(0);
    }
    
    close(p[0]);
    close(p[1]);
    
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

void jiexi(char *line, char **a){
    int i = 0;
    char *t = strtok(line, " \t\n");
    
    while(t != NULL && i < 99){
        a[i] = t;
        i++;
        t = strtok(NULL, " \t\n");
    }
    a[i] = NULL;
}

int findpipe(char **a){
    int i = 0;
    while(a[i] != NULL){
        if(strcmp(a[i], "|") == 0){
            return i;
        }
        i++;
    }
    return -1;
}

int findred(char **a, char **f1, char **f2, int *app){
    int i = 0;
    *f1 = NULL;
    *f2 = NULL;
    *app = 0;
    
    while(a[i] != NULL){
        if(strcmp(a[i], "<") == 0){
            a[i] = NULL;
            if(a[i+1] != NULL){
                *f1 = a[i+1];
            }
            i++;
        } 
        else if(strcmp(a[i], ">") == 0){
            a[i] = NULL;
            if(a[i+1] != NULL){
                *f2 = a[i+1];
                *app = 0;
            }
            i++;
        } 
        else if(strcmp(a[i], ">>") == 0){
            a[i] = NULL;
            if(a[i+1] != NULL){
                *f2 = a[i+1];
                *app = 1;
            }
            i++;
        }
        i++;
    }
    
    if(*f1 != NULL || *f2 != NULL){
        return 1;
    }
    return 0;
}

int findbg(char **a){
    int i = 0;
    while(a[i] != NULL){
        if(strcmp(a[i], "&") == 0){
            a[i] = NULL;
            return 1;
        }
        i++;
    }
    return 0;
}

int main(){
    char line[4096];
    char *a[100];
    char *a1[100];
    char *a2[100];
    char tmp[1024];
    int i, p, hasred;
    char *f1, *f2;
    int app;
    
    getcwd(now, sizeof(now));
    strcpy(old, now);
    
    signal(SIGINT, hand);
    
    printf("my shell\n");
    
    while(1){
        getcwd(now, sizeof(now));
        printf("%s $ ", now);
        fflush(stdout);
        
        if(fgets(line, sizeof(line), stdin) == NULL){
            printf("\n");
            break;
        }
        
        line[strlen(line)-1] = '\0';
        
        if(strlen(line) == 0){
            continue;
        }
        
        jiexi(line, a);
        
        if(strcmp(a[0], "exit") == 0){
            printf("bye\n");
            break;
        }
        
        if(strcmp(a[0], "cd") == 0){
            if(a[1] == NULL || strcmp(a[1], "~") == 0){
                chdir(getenv("HOME"));
            } 
            else if(strcmp(a[1], "-") == 0){
                
                if(strlen(old) > 0){
                    printf("%s\n", old);
                    chdir(old);
                } 
                else {
                    printf("no old\n");
                }
            } 
            else {
                if(chdir(a[1]) != 0){
                    printf("no dir: %s\n", a[1]);
                }
            }
            getcwd(now, sizeof(now));
            strcpy(old, now);
            continue;
        }
        
        if(strcmp(a[0], "pwd") == 0){
            printf("%s\n", now);
            continue;
        }
        
        if(strcmp(a[0], "echo") == 0){
            for(i = 1; a[i] != NULL; i++){
                printf("%s ", a[i]);
            }
            printf("\n");
            continue;
        }
        
        bg = findbg(a);
        
        p = findpipe(a);
        if(p > 0){
            for(i = 0; i < p; i++){
                a1[i] = a[i];
            }
            a1[i] = NULL;
            
            for(i = p + 1; a[i] != NULL; i++){
                a2[i - p - 1] = a[i];
            }
            a2[i - p - 1] = NULL;
            
            char *x1, *y1;
            int z1;
            int r1 = findred(a1, &x1, &y1, &z1);
            
            char *x2, *y2;
            int z2;
            int r2 = findred(a2, &x2, &y2, &z2);
            
            if(r1 || r2){
                pipewithred(a1, a2, x1, y2, z2);
            } else {
                guandao(a1, a2);
            }
        } 
        else {
            hasred = findred(a, &f1, &f2, &app);
            if(hasred){
                chongdingxiang(a);
            } 
            else {
                zhixing(a);
            }
        }
    }
    
    return 0;
}