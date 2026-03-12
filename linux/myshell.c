#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_PATH 256

char current_dir[MAX_PATH];
char prev_dir[MAX_PATH];
int last_status = 0;

void daoxv(double arr[],int n){
    for(int i=0;i<n-1;i++){
        for(int j=0;j<n-1-i;j++){
            if(arr[j+1]>arr[j]){
                int temp=arr[j+1];
                    arr[j+1]=arr[j];
                    arr[j]=temp;   
            }
        }
    }
}

void handle_signal(int sig){
    if(sig == SIGINT){
        printf("\n");
        printf("myshell> ");
        fflush(stdout);
    }
}

void get_prompt(char *prompt){
    getcwd(current_dir,sizeof(current_dir));
    char *home = getenv("HOME");
    if(home && strncmp(current_dir,home,strlen(home))==0){
        sprintf(prompt,"~%s$ ",current_dir+strlen(home));
    }else{
        sprintf(prompt,"%s$ ",current_dir);
    }
}

int parse_input(char *input, char **args){
    int i=0;
    char *token = strtok(input," \t\n");
    while(token && i<MAX_ARGS-1){
        args[i++] = token;
        token = strtok(NULL," \t\n");
    }
    args[i] = NULL;
    return i;
}

void handle_redirection(char **args){
    for(int i=0; args[i]!=NULL; i++){
        if(strcmp(args[i],"<")==0){
            args[i] = NULL;
            int fd = open(args[i+1], O_RDONLY);
            if(fd<0){
                perror("open");
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        else if(strcmp(args[i],">")==0){
            args[i] = NULL;
            int fd = open(args[i+1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd<0){
                perror("open");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        else if(strcmp(args[i],">>")==0){
            args[i] = NULL;
            int fd = open(args[i+1], O_WRONLY|O_CREAT|O_APPEND, 0644);
            if(fd<0){
                perror("open");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }
}

int is_builtin(char **args){
    if(strcmp(args[0],"cd")==0){
        if(args[1]==NULL || strcmp(args[1],"~")==0){
            chdir(getenv("HOME"));
        }else if(strcmp(args[1],"-")==0){
            if(strlen(prev_dir)>0){
                printf("%s\n",prev_dir);
                chdir(prev_dir);
            }
        }else{
            if(chdir(args[1])!=0){
                perror("cd");
            }
        }
        getcwd(current_dir,sizeof(current_dir));
        strcpy(prev_dir,current_dir);
        return 1;
    }
    else if(strcmp(args[0],"exit")==0){
        exit(0);
    }
    else if(strcmp(args[0],"pwd")==0){
        printf("%s\n",current_dir);
        return 1;
    }
    else if(strcmp(args[0],"echo")==0){
        for(int i=1; args[i]!=NULL; i++){
            printf("%s ",args[i]);
        }
        printf("\n");
        return 1;
    }
    return 0;
}

void execute_command(char **args){
    pid_t pid = fork();
    if(pid<0){
        perror("fork");
        return;
    }
    if(pid==0){
        signal(SIGINT,SIG_DFL);
        handle_redirection(args);
        execvp(args[0],args);
        printf("command not found: %s\n",args[0]);
        exit(1);
    }else{
        int status;
        waitpid(pid,&status,0);
        last_status = WEXITSTATUS(status);
    }
}

void execute_pipe(char *input){
    char *cmd1 = strtok(input,"|");
    char *cmd2 = strtok(NULL,"|");
    if(cmd2==NULL){
        char *args[MAX_ARGS];
        parse_input(cmd1,args);
        execute_command(args);
        return;
    }
    
    int pipefd[2];
    pipe(pipefd);
    
    pid_t pid1 = fork();
    if(pid1==0){
        dup2(pipefd[1],STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        char *args[MAX_ARGS];
        parse_input(cmd1,args);
        execvp(args[0],args);
        exit(1);
    }
    
    pid_t pid2 = fork();
    if(pid2==0){
        dup2(pipefd[0],STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        char *args[MAX_ARGS];
        parse_input(cmd2,args);
        execvp(args[0],args);
        exit(1);
    }
    
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1,NULL,0);
    waitpid(pid2,NULL,0);
}

int has_pipe(char *input){
    for(int i=0; input[i]; i++){
        if(input[i]=='|') return 1;
    }
    return 0;
}

int has_ampersand(char **args){
    for(int i=0; args[i]!=NULL; i++){
        if(strcmp(args[i],"&")==0){
            args[i] = NULL;
            return 1;
        }
    }
    return 0;
}

int main(){
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    char prompt[256];
    
    getcwd(current_dir,sizeof(current_dir));
    strcpy(prev_dir,current_dir);
    
    signal(SIGINT,handle_signal);
    
    while(1){
        get_prompt(prompt);
        printf("%s",prompt);
        fflush(stdout);
        
        if(fgets(input,sizeof(input),stdin)==NULL){
            printf("\n");
            break;
        }
        
        input[strcspn(input,"\n")] = 0;
        if(strlen(input)==0) continue;
        
        if(has_pipe(input)){
            char input_copy[MAX_INPUT];
            strcpy(input_copy,input);
            execute_pipe(input_copy);
        }else{
            int argc = parse_input(input,args);
            if(argc==0) continue;
            
            if(is_builtin(args)) continue;
            
            int bg = has_ampersand(args);
            if(bg){
                pid_t pid = fork();
                if(pid==0){
                    execute_command(args);
                    exit(0);
                }else{
                    printf("[%d]\n",pid);
                }
            }else{
                execute_command(args);
            }
        }
    }
    return 0;
}