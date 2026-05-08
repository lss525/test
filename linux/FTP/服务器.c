#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>


#define DK 2100
#define HCHANG 1024
#define GENMULU "/tmp/ftp_root"

int fuwu_sock=-1;
int yunxing=1;

void xinhao_chuli(int xh){
    yunxing=0;
    if(fuwu_sock>=0){
        close(fuwu_sock);
        fuwu_sock=-1;
    }
}

int main(int argc,char*argv[]){

    signal(SIGINT,xinhao_chuli);
    signal(SIGTERM,xinhao_chuli);
    signal(SIGPIPE,SIG_IGN);

    int dk=DK;
    if(argc>1)dk=atoi(argv[1]);

    //根目录
    mkdir(GENMULU,0755);

    char fp[512];
    snprintf(fp,sizeof(fp),"%s/welcome.txt",GENMULU);
    FILE*f=fopen(fp,"w");
    if(f){
        fclose(f);
    }
    snprintf(fp,sizeof(fp),"%s/test.txt",GENMULU);
    fopen(fp,"w");
    if(f){
        fclose(f);
    }
    
    //socket
    fuwu_sock=socket(AF_INET,SOCK_STREAM,0);
    if(fuwu_sock<0){
        perror("socket");
        return 1;
    }

    int opt=1;
    setsockopt(fuwu_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

      
    
}