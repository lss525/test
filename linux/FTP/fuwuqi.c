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

void* jieshu_lianjie(void* arg){
    while(yunxing){
        struct sockaddr_in ka;
        socklen_t al=sizeof(ka);

        int ks=accept(fuwu_sock,(struct sockaddr*)&ka,&al);

        char ip[16];
        inet_ntop(AF_INET, &ka.sin_addr, ip, sizeof(ip));
        printf("link ip%s:%d\n",ntohs(ka.sin_port));

        int* sock_ptr=(int*)malloc(sizeof(int));
        *sock_ptr=ks;

        pthread_t tid;
        pthread_create(&tid, NULL, kongzhi_xiancheng, sock_ptr);
        pthread_detach(tid);
        
        printf("creat");
    }

    return NULL;

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

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(dk);

    if(bind(fuwu_sock,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("bind");
        close(fuwu_sock);
        return 1;
    }
    if(listen(fuwu_sock,10)<0){
        perror("listen");
        close(fuwu_sock);
        return 1;

    }

    printf("duankou:%d\n",dk);
    printf("%s\n",GENMULU);

    jieshu_lianjie(NULL);
    close(fuwu_sock);
    
    
    return 0;

}